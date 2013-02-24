// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_WEBRTC_AUDIO_DEVICE_TEST_H_
#define CONTENT_TEST_WEBRTC_AUDIO_DEVICE_TEST_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "content/browser/renderer_host/media/mock_media_observer.h"
#include "content/public/renderer/content_renderer_client.h"
#include "ipc/ipc_listener.h"
#include "media/base/audio_hardware_config.h"
#include "media/base/channel_layout.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/common_types.h"

namespace IPC {
class Channel;
}

namespace media {
class AudioManager;
}

namespace net {
class URLRequestContext;
}

namespace webrtc {
class VoENetwork;
}

#if defined(OS_WIN)
namespace base {
namespace win {
class ScopedCOMInitializer;
}
}
#endif

namespace content {

class AudioInputRendererHost;
class AudioMirroringManager;
class AudioRendererHost;
class ContentRendererClient;
class MediaStreamManager;
class MockResourceContext;
class RenderThreadImpl;
class ResourceContext;
class TestBrowserThread;
class WebRTCMockRenderProcess;

// Scoped class for WebRTC interfaces.  Fetches the wrapped interface
// in the constructor via WebRTC's GetInterface mechanism and then releases
// the reference in the destructor.
template<typename T>
class ScopedWebRTCPtr {
 public:
  template<typename Engine>
  explicit ScopedWebRTCPtr(Engine* e)
      : ptr_(T::GetInterface(e)) {}
  explicit ScopedWebRTCPtr(T* p) : ptr_(p) {}
  ~ScopedWebRTCPtr() { reset(); }
  T* operator->() const { return ptr_; }
  T* get() const { return ptr_; }

  // Releases the current pointer.
  void reset() {
    if (ptr_) {
      ptr_->Release();
      ptr_ = NULL;
    }
  }

  bool valid() const { return ptr_ != NULL; }

 private:
  T* ptr_;
};

// Wrapper to automatically calling T::Delete in the destructor.
// This is useful for some WebRTC objects that have their own Create/Delete
// methods and we can't use our our scoped_* classes.
template <typename T>
class WebRTCAutoDelete {
 public:
  WebRTCAutoDelete() : ptr_(NULL) {}
  explicit WebRTCAutoDelete(T* ptr) : ptr_(ptr) {}
  ~WebRTCAutoDelete() { reset(); }

  void reset() {
    if (ptr_) {
      T::Delete(ptr_);
      ptr_ = NULL;
    }
  }

  T* operator->() { return ptr_; }
  T* get() const { return ptr_; }

  bool valid() const { return ptr_ != NULL; }

 protected:
  T* ptr_;
};

// Implemented and defined in the cc file.
class ReplaceContentClientRenderer;

class WebRTCAudioDeviceTest : public ::testing::Test, public IPC::Listener {
 public:
  WebRTCAudioDeviceTest();
  virtual ~WebRTCAudioDeviceTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // Sends an IPC message to the IO thread channel.
  bool Send(IPC::Message* message);

  void SetAudioHardwareConfig(media::AudioHardwareConfig* hardware_config);

 protected:
  void InitializeIOThread(const char* thread_name);
  void UninitializeIOThread();
  void CreateChannel(const char* name);
  void DestroyChannel();

  void OnGetAudioHardwareConfig(int* output_buffer_size,
                                int* output_sample_rate, int* input_sample_rate,
                                media::ChannelLayout* input_channel_layout);

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Posts a final task to the IO message loop and waits for completion.
  void WaitForIOThreadCompletion();
  void WaitForAudioManagerCompletion();
  void WaitForMessageLoopCompletion(base::MessageLoopProxy* loop);

  // Convenience getter for gmock.
  MockMediaInternals& media_observer() const {
    return *media_internals_.get();
  }

  std::string GetTestDataPath(const base::FilePath::StringType& file_name);

  scoped_ptr<ReplaceContentClientRenderer> saved_content_renderer_;
  MessageLoopForUI message_loop_;
  ContentRendererClient content_renderer_client_;
  RenderThreadImpl* render_thread_;  // Owned by mock_process_.
  scoped_ptr<WebRTCMockRenderProcess> mock_process_;
  scoped_ptr<MockMediaInternals> media_internals_;
  scoped_ptr<MediaStreamManager> media_stream_manager_;
  scoped_ptr<media::AudioManager> audio_manager_;
  scoped_ptr<AudioMirroringManager> mirroring_manager_;
  scoped_ptr<net::URLRequestContext> test_request_context_;
  scoped_ptr<ResourceContext> resource_context_;
  scoped_ptr<IPC::Channel> channel_;
  scoped_refptr<AudioRendererHost> audio_render_host_;
  scoped_refptr<AudioInputRendererHost> audio_input_renderer_host_;

  media::AudioHardwareConfig* audio_hardware_config_;  // Weak reference.

  // Initialized on the main test thread that we mark as the UI thread.
  scoped_ptr<TestBrowserThread> ui_thread_;
  // Initialized on our IO thread to satisfy BrowserThread::IO checks.
  scoped_ptr<TestBrowserThread> io_thread_;

#if defined(OS_WIN)
  // COM initialization on the IO thread.
  scoped_ptr<base::win::ScopedCOMInitializer> initialize_com_;
#endif

  // These are initialized when we set up our IO thread.
  bool has_input_devices_;
  bool has_output_devices_;

  // The previous state for whether sandbox support was enabled in
  // RenderViewWebKitPlatformSupportImpl.
  bool sandbox_was_enabled_;
};

// A very basic implementation of webrtc::Transport that acts as a transport
// but just forwards all calls to a local webrtc::VoENetwork implementation.
// Ownership of the VoENetwork object lies outside the class.
class WebRTCTransportImpl : public webrtc::Transport {
 public:
  explicit WebRTCTransportImpl(webrtc::VoENetwork* network);
  virtual ~WebRTCTransportImpl();

  virtual int SendPacket(int channel, const void* data, int len) OVERRIDE;
  virtual int SendRTCPPacket(int channel, const void* data, int len) OVERRIDE;

 private:
  webrtc::VoENetwork* network_;
};

}  // namespace content

#endif  // CONTENT_TEST_WEBRTC_AUDIO_DEVICE_TEST_H_
