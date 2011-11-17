// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_WEBRTC_AUDIO_DEVICE_TEST_H_
#define CONTENT_TEST_WEBRTC_AUDIO_DEVICE_TEST_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "content/browser/renderer_host/media/mock_media_observer.h"
#include "content/renderer/mock_content_renderer_client.h"
#include "ipc/ipc_channel.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/common_types.h"

class AudioInputRendererHost;
class AudioRendererHost;
class RenderThreadImpl;
class WebRTCMockRenderProcess;

namespace base {
namespace win {
class ScopedCOMInitializer;
}
}

namespace content {
class ContentRendererClient;
class ResourceContext;
class TestBrowserThread;
}

namespace media_stream {
class MediaStreamManager;
}

namespace net {
class URLRequestContext;
}

namespace webrtc {
class VoENetwork;
}

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

// Individual tests can provide an implementation (or mock) of this interface
// when the audio code queries for hardware capabilities on the IO thread.
class AudioUtilInterface {
 public:
  virtual ~AudioUtilInterface() {}
  virtual double GetAudioHardwareSampleRate() = 0;
  virtual double GetAudioInputHardwareSampleRate() = 0;
};

// Implemented and defined in the cc file.
class ReplaceContentClientRenderer;

class WebRTCAudioDeviceTest
    : public ::testing::Test,
      public IPC::Channel::Listener {
 public:
  class SetupTask : public base::RefCountedThreadSafe<SetupTask> {
   public:
    explicit SetupTask(WebRTCAudioDeviceTest* test) : test_(test) {
      DCHECK(test);  // Catch this early since we dereference much later.
    }
    void InitializeIOThread(const char* thread_name) {
      test_->InitializeIOThread(thread_name);
    }
    void UninitializeIOThread() { test_->UninitializeIOThread(); }
   protected:
    WebRTCAudioDeviceTest* test_;
  };

  WebRTCAudioDeviceTest();
  virtual ~WebRTCAudioDeviceTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // Sends an IPC message to the IO thread channel.
  bool Send(IPC::Message* message);

  void set_audio_util_callback(AudioUtilInterface* callback) {
    audio_util_callback_ = callback;
  }

 protected:
  void InitializeIOThread(const char* thread_name);
  void UninitializeIOThread();
  void CreateChannel(const char* name,
                     content::ResourceContext* resource_context);
  void DestroyChannel();

  void OnGetHardwareSampleRate(double* sample_rate);
  void OnGetHardwareInputSampleRate(double* sample_rate);

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Posts a final task to the IO message loop and waits for completion.
  void WaitForIOThreadCompletion();

  // Convenience getter for gmock.
  MockMediaObserver& media_observer() const {
    return *media_observer_.get();
  }

  std::string GetTestDataPath(const FilePath::StringType& file_name);

  scoped_ptr<ReplaceContentClientRenderer> saved_content_renderer_;
  MessageLoopForUI message_loop_;
  content::MockContentRendererClient mock_content_renderer_client_;
  RenderThreadImpl* render_thread_;  // Owned by mock_process_.
  scoped_ptr<WebRTCMockRenderProcess> mock_process_;
  base::WaitableEvent event_;
  scoped_ptr<MockMediaObserver> media_observer_;
  scoped_ptr<media_stream::MediaStreamManager> media_stream_manager_;
  scoped_ptr<content::ResourceContext> resource_context_;
  scoped_refptr<net::URLRequestContext> test_request_context_;
  scoped_ptr<IPC::Channel> channel_;
  scoped_refptr<AudioRendererHost> audio_render_host_;
  scoped_refptr<AudioInputRendererHost> audio_input_renderer_host_;

  AudioUtilInterface* audio_util_callback_;  // Weak reference.

  // Initialized on the main test thread that we mark as the UI thread.
  scoped_ptr<content::TestBrowserThread> ui_thread_;
  // Initialized on our IO thread to satisfy BrowserThread::IO checks.
  scoped_ptr<content::TestBrowserThread> io_thread_;
  // COM initialization on the IO thread for Windows.
  scoped_ptr<base::win::ScopedCOMInitializer> initialize_com_;
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

#endif  // CONTENT_TEST_WEBRTC_AUDIO_DEVICE_TEST_H_
