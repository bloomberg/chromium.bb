// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/render_frame_audio_input_stream_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "content/browser/media/forwarding_audio_stream_factory.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_service_manager_context.h"
#include "content/public/test/test_utils.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_parameters.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "net/url_request/url_request_test_util.h"
#include "services/audio/public/cpp/fake_stream_factory.h"
#include "services/audio/public/mojom/constants.mojom.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class RenderFrameAudioInputStreamFactoryTest
    : public RenderViewHostTestHarness {
 public:
  RenderFrameAudioInputStreamFactoryTest()
      : RenderViewHostTestHarness(
            TestBrowserThreadBundle::Options::REAL_IO_THREAD),
        test_service_manager_context_(
            std::make_unique<TestServiceManagerContext>()),
        audio_manager_(std::make_unique<media::TestAudioThread>(true),
                       &log_factory_),
        audio_system_(media::AudioSystemImpl::CreateInstance()),
        media_stream_manager_(std::make_unique<MediaStreamManager>(
            audio_system_.get(),
            BrowserThread::GetTaskRunnerForThread(BrowserThread::UI))) {}

  ~RenderFrameAudioInputStreamFactoryTest() override {}

  BrowserContext* CreateBrowserContext() override {
    // Caller takes ownership.
    return new TestBrowserContextWithRealURLRequestContextGetter();
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    RenderFrameHostTester::For(main_rfh())->InitializeRenderFrameIfNeeded();

    // Set up the ForwardingAudioStreamFactory.
    service_manager::Connector::TestApi connector_test_api(
        ForwardingAudioStreamFactory::ForFrame(main_rfh())
            ->get_connector_for_testing());
    connector_test_api.OverrideBinderForTesting(
        service_manager::Identity(audio::mojom::kServiceName),
        audio::mojom::StreamFactory::Name_,
        base::BindRepeating(
            &RenderFrameAudioInputStreamFactoryTest::BindFactory,
            base::Unretained(this)));

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    audio_manager_.Shutdown();
    test_service_manager_context_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  void BindFactory(mojo::ScopedMessagePipeHandle factory_request) {
    audio_service_stream_factory_.binding_.Bind(
        audio::mojom::StreamFactoryRequest(std::move(factory_request)));
  }

  // TestBrowserContext has a URLRequestContextGetter which uses a
  // NullTaskRunner. This causes it to be destroyed on the wrong thread. This
  // BrowserContext instead uses the IO thread task runner for the
  // URLRequestContextGetter.
  class TestBrowserContextWithRealURLRequestContextGetter
      : public TestBrowserContext {
   public:
    TestBrowserContextWithRealURLRequestContextGetter() {
      request_context_ = base::MakeRefCounted<net::TestURLRequestContextGetter>(
          BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));
    }

    ~TestBrowserContextWithRealURLRequestContextGetter() override {}

    net::URLRequestContextGetter* CreateRequestContext(
        ProtocolHandlerMap* protocol_handlers,
        URLRequestInterceptorScopedVector request_interceptors) override {
      return request_context_.get();
    }

   private:
    scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  };

  class MockStreamFactory : public audio::FakeStreamFactory {
   public:
    MockStreamFactory() : binding_(this) {}
    ~MockStreamFactory() override {}

    void CreateInputStream(
        media::mojom::AudioInputStreamRequest stream_request,
        media::mojom::AudioInputStreamClientPtr client,
        media::mojom::AudioInputStreamObserverPtr observer,
        media::mojom::AudioLogPtr log,
        const std::string& device_id,
        const media::AudioParameters& params,
        uint32_t shared_memory_count,
        bool enable_agc,
        mojo::ScopedSharedBufferHandle key_press_count_buffer,
        CreateInputStreamCallback created_callback) override {
      /*EXPECT_EQ(device_id, kDeviceId);
      EXPECT_EQ(shared_memory_count, kSharedMemoryCount);
      EXPECT_EQ(kAGC, enable_agc);*/
      last_created_callback = std::move(created_callback);
    }

    CreateInputStreamCallback last_created_callback;

    mojo::Binding<audio::mojom::StreamFactory> binding_;
  };

  AudioInputDeviceManager* audio_input_device_manager() {
    return media_stream_manager_->audio_input_device_manager();
  }

  class StreamOpenedWaiter : public MediaStreamProviderListener {
   public:
    explicit StreamOpenedWaiter(scoped_refptr<AudioInputDeviceManager> aidm)
        : aidm_(aidm),
          event_(base::WaitableEvent::ResetPolicy::MANUAL,
                 base::WaitableEvent::InitialState::NOT_SIGNALED) {
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::BindOnce(&AudioInputDeviceManager::RegisterListener, aidm,
                         this));
    }

    ~StreamOpenedWaiter() override {
      DCHECK_CURRENTLY_ON(BrowserThread::IO);
      aidm_->UnregisterListener(this);
    }

    void Opened(MediaStreamType stream_type, int capture_session_id) override {
      event_.Signal();
    }
    void Closed(MediaStreamType stream_type, int capture_session_id) override {}
    void Aborted(MediaStreamType stream_type, int capture_session_id) override {
    }

    void Wait() { event_.Wait(); }

   private:
    scoped_refptr<AudioInputDeviceManager> aidm_;
    base::WaitableEvent event_;
  };

  void CallOpenWithTestDeviceAndStoreSessionIdOnIO(int* session_id) {
    *session_id = audio_input_device_manager()->Open(
        MediaStreamDevice(MEDIA_DEVICE_AUDIO_CAPTURE, kDeviceId, kDeviceName));
  }

  const media::AudioParameters kParams =
      media::AudioParameters::UnavailableDeviceParams();
  const std::string kDeviceId = "test id";
  const std::string kDeviceName = "test name";
  const bool kAGC = false;
  const uint32_t kSharedMemoryCount = 123;
  MockStreamFactory audio_service_stream_factory_;
  std::unique_ptr<TestServiceManagerContext> test_service_manager_context_;
  media::FakeAudioLogFactory log_factory_;
  media::FakeAudioManager audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
};

TEST_F(RenderFrameAudioInputStreamFactoryTest, ConstructDestruct) {
  mojom::RendererAudioInputStreamFactoryPtr factory_ptr;
  RenderFrameAudioInputStreamFactory factory(mojo::MakeRequest(&factory_ptr),
                                             audio_input_device_manager(),
                                             main_rfh());
}

TEST_F(RenderFrameAudioInputStreamFactoryTest,
       CreateOpenedStream_ForwardsCall) {
  mojom::RendererAudioInputStreamFactoryPtr factory_ptr;
  RenderFrameAudioInputStreamFactory factory(mojo::MakeRequest(&factory_ptr),
                                             audio_input_device_manager(),
                                             main_rfh());

  int session_id;
  {
    std::unique_ptr<StreamOpenedWaiter, BrowserThread::DeleteOnIOThread> waiter(
        new StreamOpenedWaiter(audio_input_device_manager()));
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&RenderFrameAudioInputStreamFactoryTest::
                           CallOpenWithTestDeviceAndStoreSessionIdOnIO,
                       base::Unretained(this), &session_id));
    waiter->Wait();
  }

  mojom::RendererAudioInputStreamFactoryClientPtr client;
  mojo::MakeRequest(&client);
  factory_ptr->CreateStream(std::move(client), session_id, kParams, kAGC,
                            kSharedMemoryCount);

  RunAllTasksUntilIdle();

  EXPECT_TRUE(!!audio_service_stream_factory_.last_created_callback);
}

TEST_F(RenderFrameAudioInputStreamFactoryTest,
       CreateStreamWithoutValidSessionId_Fails) {
  mojom::RendererAudioInputStreamFactoryPtr factory_ptr;
  RenderFrameAudioInputStreamFactory factory(mojo::MakeRequest(&factory_ptr),
                                             audio_input_device_manager(),
                                             main_rfh());

  int session_id = 123;

  mojom::RendererAudioInputStreamFactoryClientPtr client;
  mojo::MakeRequest(&client);
  factory_ptr->CreateStream(std::move(client), session_id, kParams, kAGC,
                            kSharedMemoryCount);

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(!!audio_service_stream_factory_.last_created_callback);
}

}  // namespace content
