// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// VideoCaptureImpl represents a capture device in renderer process. It provides
// interfaces for clients to Start/Stop capture. It also communicates to clients
// when buffer is ready, state of capture device is changed.

// VideoCaptureImpl is also a delegate of VideoCaptureMessageFilter which
// relays operation of capture device to browser process and receives response
// from browser process.

// The media::VideoCapture and VideoCaptureMessageFilter::Delegate are
// asynchronous interfaces, which means callers can call those interfaces
// from any threads without worrying about thread safety.
// The |capture_message_loop_proxy_| is the working thread of VideoCaptureImpl.
// All non-const members are accessed only on that working thread.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_H_

#include <list>
#include <map>

#include "content/common/content_export.h"
#include "content/common/media/video_capture.h"
#include "content/renderer/media/video_capture_message_filter.h"
#include "media/video/capture/video_capture.h"
#include "media/video/capture/video_capture_types.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

class CONTENT_EXPORT VideoCaptureImpl
    : public media::VideoCapture, public VideoCaptureMessageFilter::Delegate {
 public:
  // media::VideoCapture interface.
  virtual void StartCapture(
      media::VideoCapture::EventHandler* handler,
      const media::VideoCaptureCapability& capability) OVERRIDE;
  virtual void StopCapture(media::VideoCapture::EventHandler* handler) OVERRIDE;
  virtual void FeedBuffer(scoped_refptr<VideoFrameBuffer> buffer) OVERRIDE;
  virtual bool CaptureStarted() OVERRIDE;
  virtual int CaptureWidth() OVERRIDE;
  virtual int CaptureHeight() OVERRIDE;
  virtual int CaptureFrameRate() OVERRIDE;

  // VideoCaptureMessageFilter::Delegate interface.
  virtual void OnBufferCreated(base::SharedMemoryHandle handle,
                               int length, int buffer_id) OVERRIDE;
  virtual void OnBufferReceived(int buffer_id, base::Time timestamp) OVERRIDE;
  virtual void OnStateChanged(VideoCaptureState state) OVERRIDE;
  virtual void OnDeviceInfoReceived(
      const media::VideoCaptureParams& device_info) OVERRIDE;
  virtual void OnDelegateAdded(int32 device_id) OVERRIDE;

  // Stop/resume delivering video frames to clients, based on flag |suspend|.
  virtual void SuspendCapture(bool suspend);

 private:
  friend class VideoCaptureImplManager;
  friend class VideoCaptureImplTest;
  friend class MockVideoCaptureImpl;

  struct DIBBuffer;
  typedef std::map<media::VideoCapture::EventHandler*,
      media::VideoCaptureCapability> ClientInfo;

  VideoCaptureImpl(media::VideoCaptureSessionId id,
                   base::MessageLoopProxy* capture_message_loop_proxy,
                   VideoCaptureMessageFilter* filter);
  virtual ~VideoCaptureImpl();

  void DoStartCaptureOnCaptureThread(
      media::VideoCapture::EventHandler* handler,
      const media::VideoCaptureCapability& capability);
  void DoStopCaptureOnCaptureThread(media::VideoCapture::EventHandler* handler);
  void DoFeedBufferOnCaptureThread(scoped_refptr<VideoFrameBuffer> buffer);

  void DoBufferCreatedOnCaptureThread(base::SharedMemoryHandle handle,
                                      int length, int buffer_id);
  void DoBufferReceivedOnCaptureThread(int buffer_id, base::Time timestamp);
  void DoStateChangedOnCaptureThread(VideoCaptureState state);
  void DoDeviceInfoReceivedOnCaptureThread(
      const media::VideoCaptureParams& device_info);
  void DoDelegateAddedOnCaptureThread(int32 device_id);

  void DoSuspendCaptureOnCaptureThread(bool suspend);

  void Init();
  void DeInit(base::Closure task);
  void DoDeInitOnCaptureThread(base::Closure task);
  void StopDevice();
  void RestartCapture();
  void StartCaptureInternal();
  void AddDelegateOnIOThread();
  void RemoveDelegateOnIOThread(base::Closure task);
  virtual void Send(IPC::Message* message);

  // Helpers.
  bool ClientHasDIB() const;
  bool RemoveClient(media::VideoCapture::EventHandler* handler,
                    ClientInfo* clients);

  const scoped_refptr<VideoCaptureMessageFilter> message_filter_;
  const scoped_refptr<base::MessageLoopProxy> capture_message_loop_proxy_;
  const scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;
  int device_id_;

  // Pool of DIBs. The key is buffer_id.
  typedef std::map<int, DIBBuffer*> CachedDIB;
  CachedDIB cached_dibs_;

  ClientInfo clients_;

  ClientInfo clients_pending_on_filter_;
  ClientInfo clients_pending_on_restart_;

  media::VideoCaptureCapability::Format video_type_;

  // The parameter is being used in current capture session. A capture session
  // starts with StartCapture and ends with StopCapture.
  media::VideoCaptureParams current_params_;

  // The information about the device sent from browser process side.
  media::VideoCaptureParams device_info_;
  bool device_info_available_;

  bool suspended_;
  VideoCaptureState state_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_H_
