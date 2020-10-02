//
// Game.cpp
//

#include "pch.h"
#include "Game.h"
#include <sstream>

extern void ExitGame();

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

Game::Game() noexcept(false)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
    m_deviceResources->RegisterDeviceNotify(this);

    for (int i = 0; i < circleSides; i++)
    {
        circleColours.push_back(DirectX::XMFLOAT4(Random(), Random(), Random(), 0));
    }
}

Game::~Game()
{
#ifdef DXTK_AUDIO
    if (m_audEngine)
    {
        m_audEngine->Suspend();
    }
#endif
}

// Initialize the Direct3D resources required to run.
void Game::Initialize(HWND window, int width, int height)
{
    m_keyboard = std::make_unique<Keyboard>();

    m_mouse = std::make_unique<Mouse>();
    m_mouse->SetWindow(window);

    m_deviceResources->SetWindow(window, width, height);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();


#ifdef DXTK_AUDIO
    // Create DirectXTK for Audio objects
    AUDIO_ENGINE_FLAGS eflags = AudioEngine_Default;
#ifdef _DEBUG
    eflags = eflags | AudioEngine_Debug;
#endif

    m_audEngine = std::make_unique<AudioEngine>(eflags);

    m_audioEvent = 0;
    m_audioTimerAcc = 10.f;
    m_retryDefault = false;

    m_waveBank = std::make_unique<WaveBank>(m_audEngine.get(), L"adpcmdroid.xwb");

    m_soundEffect = std::make_unique<SoundEffect>(m_audEngine.get(), L"MusicMono_adpcm.wav");
    m_effect1 = m_soundEffect->CreateInstance();
    m_effect2 = m_waveBank->CreateInstance(10);

    m_effect1->Play(true);
    m_effect2->Play();
#endif
}

#pragma region Frame Update
// Executes the basic game loop.
void Game::Tick()
{
	//take in input


	//Update all game objects
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

#ifdef DXTK_AUDIO
    // Only update audio engine once per frame
    if (!m_audEngine->IsCriticalError() && m_audEngine->Update())
    {
        // Setup a retry in 1 second
        m_audioTimerAcc = 1.f;
        m_retryDefault = true;
    }
#endif

	//Render all game content. 
    Render();
}

// Updates the world.
void Game::Update(DX::StepTimer const& timer)
{
	Vector3 eye(0.0f, 0.0f, 5.0f);
	Vector3 at(0.0f, 0.0f, 0.0f);

	m_view = Matrix::CreateLookAt(eye, at, Vector3::UnitY);

	m_world = Matrix::Identity;

	m_batchEffect->SetView(m_view);
	m_batchEffect->SetWorld(Matrix::Identity);

#ifdef DXTK_AUDIO
    m_audioTimerAcc -= (float)timer.GetElapsedSeconds();
    if (m_audioTimerAcc < 0)
    {
        if (m_retryDefault)
        {
            m_retryDefault = false;
            if (m_audEngine->Reset())
            {
                // Restart looping audio
                m_effect1->Play(true);
            }
        }
        else
        {
            m_audioTimerAcc = 4.f;

            m_waveBank->Play(m_audioEvent++);

            if (m_audioEvent >= 11)
                m_audioEvent = 0;
        }
    }
#endif

    auto kb = m_keyboard->GetState();
    if (kb.Escape)
    {
        ExitGame();
    }

}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Game::Render()
{	
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    Clear();

    m_deviceResources->PIXBeginEvent(L"Render");
    auto context = m_deviceResources->GetD3DDeviceContext();

    auto circleSides = floor(easeInOutSinusoidal(m_timer.GetTotalSeconds(), 3.0f, 100.0f, 10.0f));

    // Draw sprite
    m_deviceResources->PIXBeginEvent(L"Draw sprite");
    m_sprites->Begin();
    m_font->DrawString(m_sprites.get(), L"DirectXTK Demo Window", XMFLOAT2(10, 10), Colors::Yellow);
    m_font->DrawString(m_sprites.get(), (L"Sides: " + std::to_wstring(circleSides)).c_str(), XMFLOAT2(10, 50), Colors::Yellow);
    m_sprites->End();
    m_deviceResources->PIXEndEvent();
	

	//Set Rendering states. 
	context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
	context->OMSetDepthStencilState(m_states->DepthNone(), 0);
	context->RSSetState(m_states->CullClockwise());
//	context->RSSetState(m_states->Wireframe());

	m_batchEffect->Apply(context);

	context->IASetInputLayout(m_batchInputLayout.Get());

    DrawStar(-1, 0);

    DrawCircle(circleSides, 1.0f, 1.5f, 0.0f);

    // Show the new frame.
    m_deviceResources->Present();
}

void Game::DrawStar(float x, float y)
{
    const int vCount = 6;
    const int iCount = 6;
    uint16_t iArray[iCount];
    VertexPositionColor vArray[vCount];
    iArray[0] = 0;
    iArray[1] = 1;
    iArray[2] = 2;
    iArray[3] = 3;
    iArray[4] = 4;
    iArray[5] = 5;

    vArray[0] = VertexPositionColor(Vector3(x + 0.f, y + 0.5f, 0.5f), Colors::Yellow);
    vArray[1] = VertexPositionColor(Vector3(x + (-0.5f), y + (-0.5f), 0.5f), Colors::Yellow);
    vArray[2] = VertexPositionColor(Vector3(x + 0.5f, y + (-0.5f), 0.5f), Colors::Yellow);

    vArray[3] = VertexPositionColor(Vector3(x + 0.0f, y + (-0.85f), 0.5f), Colors::Yellow);
    vArray[4] = VertexPositionColor(Vector3(x + 0.5f, y + 0.15f, 0.5f), Colors::Yellow);
    vArray[5] = VertexPositionColor(Vector3(x + (-0.5f), y + 0.15f, 0.5f), Colors::Yellow);

    m_batch->Begin();
    m_batch->DrawIndexed(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST, &iArray[0], iCount, &vArray[0], vCount);
    m_batch->End();
}

void Game::DrawCircle(float sides, float radius, float x, float y)
{
    const int vCount = sides + 1;
    const int iCount = sides * 3;
    std::vector<uint16_t> iArray(iCount);
    std::vector <VertexPositionColor> vArray(vCount);

    vArray[0] = VertexPositionColor(Vector3(x, y, 1.0f), circleColours[0]); // centre

    for (float i = 1; i <= sides; i++)
    {
        // calculate the point
        float angle = degToRad((360.0f / sides) * i);
        float px = x + radius * cos(angle);
        float py = y + radius * sin(angle);

        // populate the vertex array
        vArray[i] = VertexPositionColor(Vector3(px, py, 1.0f), circleColours[0]);

        // populate the index array
        int iArrayBaseIndex = (i - 1) * 3;
        iArray[iArrayBaseIndex + 0] = 0;
        iArray[iArrayBaseIndex + 1] = i == sides ? sides : (i - 1) * 1 + 1;
        iArray[iArrayBaseIndex + 2] = i == sides ? 1 : (i - 1) * 1 + 2;
    }

    m_batch->Begin();
    m_batch->DrawIndexed(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST, &iArray[0], iCount, &vArray[0], vCount);
    m_batch->End();
}

void Game::PrintToDebug(float value)
{
    std::wstringstream ss;
    ss << value << std::endl;

    OutputDebugString(ss.str().c_str());
}

void Game::PrintToDebug(std::string value)
{
    std::wstringstream ss;
    ss << value.c_str() << std::endl;

    OutputDebugString(ss.str().c_str());
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    m_deviceResources->PIXBeginEvent(L"Clear");

    // Clear the views.
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, Colors::CornflowerBlue);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

    // Set the viewport.
    auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    m_deviceResources->PIXEndEvent();
}

#pragma endregion

#pragma region Message Handlers
// Message handlers
void Game::OnActivated()
{
}

void Game::OnDeactivated()
{
}

void Game::OnSuspending()
{
#ifdef DXTK_AUDIO
    m_audEngine->Suspend();
#endif
}

void Game::OnResuming()
{
    m_timer.ResetElapsedTime();

#ifdef DXTK_AUDIO
    m_audEngine->Resume();
#endif
}

void Game::OnWindowMoved()
{
    auto r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}

void Game::OnWindowSizeChanged(int width, int height)
{
    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    CreateWindowSizeDependentResources();
}

#ifdef DXTK_AUDIO
void Game::NewAudioDevice()
{
    if (m_audEngine && !m_audEngine->IsAudioDevicePresent())
    {
        // Setup a retry in 1 second
        m_audioTimerAcc = 1.f;
        m_retryDefault = true;
    }
}
#endif

// Properties
void Game::GetDefaultSize(int& width, int& height) const
{
    width = 800;
    height = 600;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Game::CreateDeviceDependentResources()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto device = m_deviceResources->GetD3DDevice();

    m_states = std::make_unique<CommonStates>(device);
	
    m_fxFactory = std::make_unique<EffectFactory>(device);

    m_sprites = std::make_unique<SpriteBatch>(context);

    m_batchEffect = std::make_unique<BasicEffect>(device);
    m_batchEffect->SetVertexColorEnabled(true);

    {
        void const* shaderByteCode;
        size_t byteCodeLength;

        m_batchEffect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);

        DX::ThrowIfFailed(
            device->CreateInputLayout(VertexPositionColor::InputElements,
                VertexPositionColor::InputElementCount,
                shaderByteCode, byteCodeLength,
                m_batchInputLayout.ReleaseAndGetAddressOf())
        );
    }

    m_font = std::make_unique<SpriteFont>(device, L"SegoeUI_18.spritefont");

	m_batch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(context);

}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    auto size = m_deviceResources->GetOutputSize();
    float aspectRatio = float(size.right) / float(size.bottom);
    float fovAngleY = 70.0f * XM_PI / 180.0f;

    // This is a simple example of change that can be made when the app is in
    // portrait or snapped view.
    if (aspectRatio < 1.0f)
    {
        fovAngleY *= 2.0f;
    }

    // This sample makes use of a right-handed coordinate system using row-major matrices.
    m_projection = Matrix::CreatePerspectiveFieldOfView(
        fovAngleY,
        aspectRatio,
        0.01f,
        100.0f
    );

    m_batchEffect->SetProjection(m_projection);
}

void Game::OnDeviceLost()
{
    m_states.reset();
    m_fxFactory.reset();
    m_sprites.reset();
    m_batchEffect.reset();
    m_font.reset();
	m_batch.reset();
    m_batchInputLayout.Reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
