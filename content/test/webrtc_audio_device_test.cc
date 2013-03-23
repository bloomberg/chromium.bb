// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/webrtc_audio_device_test.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "content/browser/renderer_host/media/audio_input_renderer_host.h"
#include "content/browser/renderer_host/media/audio_mirroring_manager.h"
#include "content/browser/renderer_host/media/audio_renderer_host.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/mock_media_observer.h"
#include "content/common/media/media_param_traits.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread.h"
#include "content/renderer/media/audio_input_message_filter.h"
#include "content/renderer/media/audio_message_filter.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/renderer_webkitplatformsupport_impl.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_hardware_config.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/voice_engine/include/voe_audio_processing.h"
#include "third_party/webrtc/voice_engine/include/voe_base.h"
#include "third_party/webrtc/voice_engine/include/voe_file.h"
#include "third_party/webrtc/voice_engine/include/voe_network.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "media/audio/audio_manager_base.h"
#endif

using media::AudioParameters;
using media::ChannelLayout;
using testing::_;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::StrEq;

namespace content {

// This class is a mock of the child process singleton which is needed
// to be able to create a RenderThread object.
class WebRTCMockRenderProcess : public RenderProcess {
 public:
  WebRTCMockRenderProcess() {}
  virtual ~WebRTCMockRenderProcess() {}

  // RenderProcess implementation.
  virtual skia::PlatformCanvas* GetDrawingCanvas(
      TransportDIB** memory, const gfx::Rect& rect) OVERRIDE {
    return NULL;
  }
  virtual void ReleaseTransportDIB(TransportDIB* memory) OVERRIDE {}
  virtual bool UseInProcessPlugins() const OVERRIDE { return false; }
  virtual void AddBindings(int bindings) OVERRIDE {}
  virtual int GetEnabledBindings() const OVERRIDE { return 0; }
  virtual TransportDIB* CreateTransportDIB(size_t size) OVERRIDE {
    return NULL;
  }
  virtual void FreeTransportDIB(TransportDIB*) OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRTCMockRenderProcess);
};

// Utility scoped class to replace the global content client's renderer for the
// duration of the test.
class ReplaceContentClientRenderer {
 public:
  explicit ReplaceContentClientRenderer(ContentRendererClient* new_renderer) {
    saved_renderer_ = GetContentClient()->renderer();
    GetContentClient()->set_renderer_for_testing(new_renderer);
  }
  ~ReplaceContentClientRenderer() {
    // Restore the original renderer.
    GetContentClient()->set_renderer_for_testing(saved_renderer_);
  }
 private:
  ContentRendererClient* saved_renderer_;
  DISALLOW_COPY_AND_ASSIGN(ReplaceContentClientRenderer);
};

class MockRTCResourceContext : public ResourceContext {
 public:
  MockRTCResourceContext() : test_request_context_(NULL) {}
  virtual ~MockRTCResourceContext() {}

  void set_request_context(net::URLRequestContext* request_context) {
    test_request_context_ = request_context;
  }

  // ResourceContext implementation:
  virtual net::HostResolver* GetHostResolver() OVERRIDE {
    return NULL;
  }
  virtual net::URLRequestContext* GetRequestContext() OVERRIDE {
    return test_request_context_;
  }

 private:
  net::URLRequestContext* test_request_context_;

  DISALLOW_COPY_AND_ASSIGN(MockRTCResourceContext);
};

ACTION_P(QuitMessageLoop, loop_or_proxy) {
  loop_or_proxy->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

WebRTCAudioDeviceTest::WebRTCAudioDeviceTest()
    : render_thread_(NULL), audio_hardware_config_(NULL),
      has_input_devices_(false), has_output_devices_(false) {
}

WebRTCAudioDeviceTest::~WebRTCAudioDeviceTest() {}

void WebRTCAudioDeviceTest::SetUp() {
#if defined(OS_ANDROID)
    media::AudioManagerBase::RegisterAudioManager(
        base::android::AttachCurrentThread());
#endif

  // This part sets up a RenderThread environment to ensure that
  // RenderThread::current() (<=> TLS pointer) is valid.
  // Main parts are inspired by the RenderViewFakeResourcesTest.
  // Note that, the IPC part is not utilized in this test.
  saved_content_renderer_.reset(
      new ReplaceContentClientRenderer(&content_renderer_client_));
  mock_process_.reset(new WebRTCMockRenderProcess());
  ui_thread_.reset(new TestBrowserThread(BrowserThread::UI,
                                         MessageLoop::current()));

  // Construct the resource context on the UI thread.
  resource_context_.reset(new MockRTCResourceContext);

  static const char kThreadName[] = "RenderThread";
  ChildProcess::current()->io_message_loop()->PostTask(FROM_HERE,
      base::Bind(&WebRTCAudioDeviceTest::InitializeIOThread,
                 base::Unretained(this), kThreadName));
  WaitForIOThreadCompletion();

  sandbox_was_enabled_ =
      RendererWebKitPlatformSupportImpl::SetSandboxEnabledForTesting(false);
  render_thread_ = new RenderThreadImpl(kThreadName);
}

void WebRTCAudioDeviceTest::TearDown() {
  SetAudioHardwareConfig(NULL);

  // Run any pending cleanup tasks that may have been posted to the main thread.
  ChildProcess::current()->main_thread()->message_loop()->RunUntilIdle();

  // Kick of the cleanup process by closing the channel. This queues up
  // OnStreamClosed calls to be executed on the audio thread.
  ChildProcess::current()->io_message_loop()->PostTask(FROM_HERE,
      base::Bind(&WebRTCAudioDeviceTest::DestroyChannel,
                 base::Unretained(this)));
  WaitForIOThreadCompletion();

  // When audio [input] render hosts are notified that the channel has
  // been closed, they post tasks to the audio thread to close the
  // AudioOutputController and once that's completed, a task is posted back to
  // the IO thread to actually delete the AudioEntry for the audio stream. Only
  // then is the reference to the audio manager released, so we wait for the
  // whole thing to be torn down before we finally uninitialize the io thread.
  WaitForAudioManagerCompletion();

  ChildProcess::current()->io_message_loop()->PostTask(FROM_HERE,
      base::Bind(&WebRTCAudioDeviceTest::UninitializeIOThread,
                 base::Unretained((this))));
  WaitForIOThreadCompletion();
  mock_process_.reset();
  media_stream_manager_.reset();
  mirroring_manager_.reset();
  audio_manager_.reset();
  RendererWebKitPlatformSupportImpl::SetSandboxEnabledForTesting(
      sandbox_was_enabled_);
}

bool WebRTCAudioDeviceTest::Send(IPC::Message* message) {
  return channel_->Send(message);
}

void WebRTCAudioDeviceTest::SetAudioHardwareConfig(
    media::AudioHardwareConfig* hardware_config) {
  audio_hardware_config_ = hardware_config;
}

void WebRTCAudioDeviceTest::InitializeIOThread(const char* thread_name) {
#if defined(OS_WIN)
  // We initialize COM (STA) on our IO thread as is done in Chrome.
  // See BrowserProcessSubThread::Init.
  initialize_com_.reset(new base::win::ScopedCOMInitializer());
#endif

  // Set the current thread as the IO thread.
  io_thread_.reset(new TestBrowserThread(BrowserThread::IO,
                                         MessageLoop::current()));

  // Populate our resource context.
  test_request_context_.reset(new net::TestURLRequestContext());
  MockRTCResourceContext* resource_context =
      static_cast<MockRTCResourceContext*>(resource_context_.get());
  resource_context->set_request_context(test_request_context_.get());
  media_internals_.reset(new MockMediaInternals());

  // Create our own AudioManager, AudioMirroringManager and MediaStreamManager.
  audio_manager_.reset(media::AudioManager::Create());
  mirroring_manager_.reset(new AudioMirroringManager());
  media_stream_manager_.reset(new MediaStreamManager(audio_manager_.get()));

  has_input_devices_ = audio_manager_->HasAudioInputDevices();
  has_output_devices_ = audio_manager_->HasAudioOutputDevices();

  // Create an IPC channel that handles incoming messages on the IO thread.
  CreateChannel(thread_name);
}

void WebRTCAudioDeviceTest::UninitializeIOThread() {
  resource_context_.reset();

  test_request_context_.reset();

#if defined(OS_WIN)
  initialize_com_.reset();
#endif
}

void WebRTCAudioDeviceTest::CreateChannel(const char* name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  static const int kRenderProcessId = 1;
  audio_render_host_ = new AudioRendererHost(
      kRenderProcessId, audio_manager_.get(), mirroring_manager_.get(),
      media_internals_.get());
  audio_render_host_->OnChannelConnected(base::GetCurrentProcId());

  audio_input_renderer_host_ = new AudioInputRendererHost(
      audio_manager_.get(), media_stream_manager_.get());
  audio_input_renderer_host_->OnChannelConnected(base::GetCurrentProcId());

  channel_.reset(new IPC::Channel(name, IPC::Channel::MODE_SERVER, this));
  ASSERT_TRUE(channel_->Connect());

  audio_render_host_->OnFilterAdded(channel_.get());
  audio_input_renderer_host_->OnFilterAdded(channel_.get());
}

void WebRTCAudioDeviceTest::DestroyChannel() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  audio_render_host_->OnChannelClosing();
  audio_render_host_->OnFilterRemoved();
  audio_input_renderer_host_->OnChannelClosing();
  audio_input_renderer_host_->OnFilterRemoved();
  channel_.reset();
  audio_render_host_ = NULL;
  audio_input_renderer_host_ = NULL;
}

void WebRTCAudioDeviceTest::OnGetAudioHardwareConfig(
    AudioParameters* input_params, AudioParameters* output_params) {
  ASSERT_TRUE(audio_hardware_config_);
  *input_params = audio_hardware_config_->GetInputConfig();
  *output_params = audio_hardware_config_->GetOutputConfig();
}

// IPC::Listener implementation.
bool WebRTCAudioDeviceTest::OnMessageReceived(const IPC::Message& message) {
  if (render_thread_) {
    IPC::ChannelProxy::MessageFilter* filter =
        render_thread_->audio_input_message_filter();
    if (filter->OnMessageReceived(message))
      return true;

    filter = render_thread_->audio_message_filter();
    if (filter->OnMessageReceived(message))
      return true;
  }

  if (audio_render_host_.get()) {
    bool message_was_ok = false;
    if (audio_render_host_->OnMessageReceived(message, &message_was_ok))
      return true;
  }

  if (audio_input_renderer_host_.get()) {
    bool message_was_ok = false;
    if (audio_input_renderer_host_->OnMessageReceived(message, &message_was_ok))
      return true;
  }

  bool handled ALLOW_UNUSED = true;
  bool message_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(WebRTCAudioDeviceTest, message, message_is_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetAudioHardwareConfig,
                        OnGetAudioHardwareConfig)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  EXPECT_TRUE(message_is_ok);

  return true;
}

// Posts a final task to the IO message loop and waits for completion.
void WebRTCAudioDeviceTest::WaitForIOThreadCompletion() {
  WaitForMessageLoopCompletion(
      ChildProcess::current()->io_message_loop()->message_loop_proxy());
}

void WebRTCAudioDeviceTest::WaitForAudioManagerCompletion() {
  if (audio_manager_.get())
    WaitForMessageLoopCompletion(audio_manager_->GetMessageLoop());
}

void WebRTCAudioDeviceTest::WaitForMessageLoopCompletion(
    base::MessageLoopProxy* loop) {
  base::WaitableEvent* event = new base::WaitableEvent(false, false);
  loop->PostTask(FROM_HERE, base::Bind(&base::WaitableEvent::Signal,
                 base::Unretained(event)));
  if (event->TimedWait(TestTimeouts::action_max_timeout())) {
    delete event;
  } else {
    // Don't delete the event object in case the message ever gets processed.
    // If we do, we will crash the test process.
    ADD_FAILURE() << "Failed to wait for message loop";
  }
}

std::string WebRTCAudioDeviceTest::GetTestDataPath(
    const base::FilePath::StringType& file_name) {
  base::FilePath path;
  EXPECT_TRUE(PathService::Get(DIR_TEST_DATA, &path));
  path = path.Append(file_name);
  EXPECT_TRUE(file_util::PathExists(path));
#ifdef OS_WIN
  return WideToUTF8(path.value());
#else
  return path.value();
#endif
}

WebRTCTransportImpl::WebRTCTransportImpl(webrtc::VoENetwork* network)
    : network_(network) {
}

WebRTCTransportImpl::~WebRTCTransportImpl() {}

int WebRTCTransportImpl::SendPacket(int channel, const void* data, int len) {
  return network_->ReceivedRTPPacket(channel, data, len);
}

int WebRTCTransportImpl::SendRTCPPacket(int channel, const void* data,
                                        int len) {
  return network_->ReceivedRTCPPacket(channel, data, len);
}

}  // namespace content
