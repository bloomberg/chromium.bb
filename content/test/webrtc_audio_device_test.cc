// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/webrtc_audio_device_test.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/signaling_task.h"
#include "base/test/test_timeouts.h"
#include "base/win/scoped_com_initializer.h"
#include "content/browser/renderer_host/media/audio_input_renderer_host.h"
#include "content/browser/renderer_host/media/audio_renderer_host.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/mock_media_observer.h"
#include "content/browser/resource_context.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_paths.h"
#include "content/renderer/media/audio_hardware.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread_impl.h"
#include "content/test/test_browser_thread.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/voice_engine/main/interface/voe_audio_processing.h"
#include "third_party/webrtc/voice_engine/main/interface/voe_base.h"
#include "third_party/webrtc/voice_engine/main/interface/voe_file.h"
#include "third_party/webrtc/voice_engine/main/interface/voe_network.h"

using base::win::ScopedCOMInitializer;
using testing::_;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::StrEq;

// This class is a mock of the child process singleton which is needed
// to be able to create a RenderThread object.
class WebRTCMockRenderProcess : public RenderProcess {
 public:
  WebRTCMockRenderProcess() {}
  virtual ~WebRTCMockRenderProcess() {}

  // RenderProcess implementation.
  virtual skia::PlatformCanvas* GetDrawingCanvas(TransportDIB** memory,
                                                 const gfx::Rect& rect) {
    return NULL;
  }
  virtual void ReleaseTransportDIB(TransportDIB* memory) {}
  virtual bool UseInProcessPlugins() const { return false; }
  virtual bool HasInitializedMediaLibrary() const { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRTCMockRenderProcess);
};

// Utility scoped class to replace the global content client's renderer for the
// duration of the test.
class ReplaceContentClientRenderer {
 public:
  explicit ReplaceContentClientRenderer(
      content::ContentRendererClient* new_renderer) {
    saved_renderer_ = content::GetContentClient()->renderer();
    content::GetContentClient()->set_renderer(new_renderer);
  }
  ~ReplaceContentClientRenderer() {
    // Restore the original renderer.
    content::GetContentClient()->set_renderer(saved_renderer_);
  }
 private:
  content::ContentRendererClient* saved_renderer_;
  DISALLOW_COPY_AND_ASSIGN(ReplaceContentClientRenderer);
};

namespace {

class WebRTCMockResourceContext : public content::ResourceContext {
 public:
  WebRTCMockResourceContext() {}
  virtual ~WebRTCMockResourceContext() {}
  virtual void EnsureInitialized() const OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRTCMockResourceContext);
};

ACTION_P(QuitMessageLoop, loop_or_proxy) {
  loop_or_proxy->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

}  // end namespace

WebRTCAudioDeviceTest::WebRTCAudioDeviceTest()
    : render_thread_(NULL), event_(false, false), audio_util_callback_(NULL) {
}

WebRTCAudioDeviceTest::~WebRTCAudioDeviceTest() {}

void WebRTCAudioDeviceTest::SetUp() {
  // This part sets up a RenderThread environment to ensure that
  // RenderThread::current() (<=> TLS pointer) is valid.
  // Main parts are inspired by the RenderViewFakeResourcesTest.
  // Note that, the IPC part is not utilized in this test.
  saved_content_renderer_.reset(
      new ReplaceContentClientRenderer(&mock_content_renderer_client_));
  mock_process_.reset(new WebRTCMockRenderProcess());
  ui_thread_.reset(new content::TestBrowserThread(content::BrowserThread::UI,
                                                  MessageLoop::current()));

  // Construct the resource context on the UI thread.
  resource_context_.reset(new WebRTCMockResourceContext());

  static const char kThreadName[] = "RenderThread";
  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&SetupTask::InitializeIOThread, new SetupTask(this),
                 kThreadName));
  WaitForIOThreadCompletion();

  render_thread_ = new RenderThreadImpl(kThreadName);
  mock_process_->set_main_thread(render_thread_);
}

void WebRTCAudioDeviceTest::TearDown() {
  SetAudioUtilCallback(NULL);

  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&SetupTask::UninitializeIOThread, new SetupTask(this)));
  EXPECT_TRUE(event_.TimedWait(
      base::TimeDelta::FromMilliseconds(TestTimeouts::action_timeout_ms())));
  mock_process_.reset();
}

bool WebRTCAudioDeviceTest::Send(IPC::Message* message) {
  return channel_->Send(message);
}

void WebRTCAudioDeviceTest::SetAudioUtilCallback(AudioUtilInterface* callback) {
  // Invalidate any potentially cached values since the new callback should
  // be used for those queries.
  audio_hardware::ResetCache();
  audio_util_callback_ = callback;
}

void WebRTCAudioDeviceTest::InitializeIOThread(const char* thread_name) {
  // We initialize COM (STA) on our IO thread as is done in Chrome.
  // See BrowserProcessSubThread::Init.
  initialize_com_.reset(new ScopedCOMInitializer());

  // Set the current thread as the IO thread.
  io_thread_.reset(new content::TestBrowserThread(content::BrowserThread::IO,
                                                  MessageLoop::current()));
  test_request_context_ = new TestURLRequestContext();
  resource_context_->set_request_context(test_request_context_.get());
  media_observer_.reset(new MockMediaObserver());
  resource_context_->set_media_observer(media_observer_.get());
  media_stream_manager_.reset(new media_stream::MediaStreamManager());
  resource_context_->set_media_stream_manager(media_stream_manager_.get());

  CreateChannel(thread_name, resource_context_.get());
}

void WebRTCAudioDeviceTest::UninitializeIOThread() {
  DestroyChannel();
  resource_context_.reset();
  media_stream_manager_.reset();
  test_request_context_ = NULL;
  initialize_com_.reset();
  event_.Signal();
}

void WebRTCAudioDeviceTest::CreateChannel(
    const char* name,
    content::ResourceContext* resource_context) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  audio_render_host_ = new AudioRendererHost(resource_context);
  audio_render_host_->OnChannelConnected(base::GetCurrentProcId());

  audio_input_renderer_host_ = new AudioInputRendererHost(resource_context);
  audio_input_renderer_host_->OnChannelConnected(base::GetCurrentProcId());

  channel_.reset(new IPC::Channel(name, IPC::Channel::MODE_SERVER, this));
  ASSERT_TRUE(channel_->Connect());

  audio_render_host_->OnFilterAdded(channel_.get());
  audio_input_renderer_host_->OnFilterAdded(channel_.get());
}

void WebRTCAudioDeviceTest::DestroyChannel() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  audio_render_host_->OnChannelClosing();
  audio_render_host_->OnFilterRemoved();
  audio_input_renderer_host_->OnChannelClosing();
  audio_input_renderer_host_->OnFilterRemoved();
  channel_.reset();
  audio_render_host_ = NULL;
  audio_input_renderer_host_ = NULL;
}

void WebRTCAudioDeviceTest::OnGetHardwareSampleRate(double* sample_rate) {
  EXPECT_TRUE(audio_util_callback_);
  *sample_rate = audio_util_callback_ ?
      audio_util_callback_->GetAudioHardwareSampleRate() : 0.0;
}

void WebRTCAudioDeviceTest::OnGetHardwareInputSampleRate(double* sample_rate) {
  EXPECT_TRUE(audio_util_callback_);
  *sample_rate = audio_util_callback_ ?
      audio_util_callback_->GetAudioInputHardwareSampleRate() : 0.0;
}

// IPC::Channel::Listener implementation.
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

  bool handled = true;
  bool message_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(WebRTCAudioDeviceTest, message, message_is_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetHardwareSampleRate,
                        OnGetHardwareSampleRate)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetHardwareInputSampleRate,
                        OnGetHardwareInputSampleRate)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  EXPECT_TRUE(message_is_ok);

  return true;
}

// Posts a final task to the IO message loop and waits for completion.
void WebRTCAudioDeviceTest::WaitForIOThreadCompletion() {
  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE, new base::SignalingTask(&event_));
  EXPECT_TRUE(event_.TimedWait(
      base::TimeDelta::FromMilliseconds(TestTimeouts::action_timeout_ms())));
}

std::string WebRTCAudioDeviceTest::GetTestDataPath(
    const FilePath::StringType& file_name) {
  FilePath path;
  EXPECT_TRUE(PathService::Get(content::DIR_TEST_DATA, &path));
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
