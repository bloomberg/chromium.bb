// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// VideoCaptureImpl represents a capture device in renderer process. It provides
// interfaces for clients to Start/Stop capture. It also communicates to clients
// when buffer is ready, state of capture device is changed.

// VideoCaptureImpl is also a delegate of VideoCaptureMessageFilter which relays
// operation of a capture device to the browser process and receives responses
// from browser process.

// The media::VideoCapture and VideoCaptureMessageFilter::Delegate are
// asynchronous interfaces, which means callers can call those interfaces
// from any threads without worrying about thread safety.
// The |capture_message_loop_proxy_| is the working thread of VideoCaptureImpl.
// All non-const members are accessed only on that working thread.
//
// Implementation note: tasks are posted bound to Unretained(this) to both the
// I/O and Capture threads and this is safe (even though the I/O thread is
// scoped to the renderer process and the capture_message_loop_proxy_ thread is
// scoped to the VideoCaptureImplManager) because VideoCaptureImplManager only
// triggers deletion of its VideoCaptureImpl's by calling DeInit which detours
// through the capture & I/O threads, so as long as nobody posts tasks after the
// DeInit() call is made, it is guaranteed none of these Unretained posted tasks
// will dangle after the delete goes through.  The "as long as" is guaranteed by
// clients of VideoCaptureImplManager not using devices after they've
// RemoveDevice'd them.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_H_

#include <list>
#include <map>

#include "base/memory/weak_ptr.h"
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
      const media::VideoCaptureParams& params) OVERRIDE;
  virtual void StopCapture(media::VideoCapture::EventHandler* handler) OVERRIDE;
  virtual bool CaptureStarted() OVERRIDE;
  virtual int CaptureFrameRate() OVERRIDE;

  // VideoCaptureMessageFilter::Delegate interface.
  virtual void OnBufferCreated(base::SharedMemoryHandle handle,
                               int length,
                               int buffer_id) OVERRIDE;
  virtual void OnBufferDestroyed(int buffer_id) OVERRIDE;
  virtual void OnBufferReceived(
      int buffer_id,
      base::Time timestamp,
      const media::VideoCaptureFormat& format) OVERRIDE;
  virtual void OnStateChanged(VideoCaptureState state) OVERRIDE;
  virtual void OnDelegateAdded(int32 device_id) OVERRIDE;

  // Stop/resume delivering video frames to clients, based on flag |suspend|.
  virtual void SuspendCapture(bool suspend);

 private:
  friend class VideoCaptureImplManager;
  friend class VideoCaptureImplTest;
  friend class MockVideoCaptureImpl;

  class ClientBuffer;
  typedef std::map<media::VideoCapture::EventHandler*,
                   media::VideoCaptureParams> ClientInfo;

  VideoCaptureImpl(media::VideoCaptureSessionId session_id,
                   base::MessageLoopProxy* capture_message_loop_proxy,
                   VideoCaptureMessageFilter* filter);
  virtual ~VideoCaptureImpl();

  void DoStartCaptureOnCaptureThread(
      media::VideoCapture::EventHandler* handler,
      const media::VideoCaptureParams& params);
  void DoStopCaptureOnCaptureThread(media::VideoCapture::EventHandler* handler);
  void DoBufferCreatedOnCaptureThread(base::SharedMemoryHandle handle,
                                      int length,
                                      int buffer_id);
  void DoBufferDestroyedOnCaptureThread(int buffer_id);
  void DoBufferReceivedOnCaptureThread(
      int buffer_id,
      base::Time timestamp,
      const media::VideoCaptureFormat& format);
  void DoClientBufferFinishedOnCaptureThread(
      int buffer_id,
      const scoped_refptr<ClientBuffer>& buffer);
  void DoStateChangedOnCaptureThread(VideoCaptureState state);
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
  bool RemoveClient(media::VideoCapture::EventHandler* handler,
                    ClientInfo* clients);

  const scoped_refptr<VideoCaptureMessageFilter> message_filter_;
  const scoped_refptr<base::MessageLoopProxy> capture_message_loop_proxy_;
  const scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;
  int device_id_;
  const int session_id_;

  // Buffers available for sending to the client.
  typedef std::map<int32, scoped_refptr<ClientBuffer> > ClientBufferMap;
  ClientBufferMap client_buffers_;

  ClientInfo clients_;
  ClientInfo clients_pending_on_filter_;
  ClientInfo clients_pending_on_restart_;

  // Member params_ represents the video format requested by the
  // client to this class via DoStartCaptureOnCaptureThread.
  media::VideoCaptureParams params_;

  // The device's video capture format sent from browser process side.
  media::VideoCaptureFormat last_frame_format_;

  bool suspended_;
  VideoCaptureState state_;

  // WeakPtrFactory pointing back to |this| object, for use with
  // media::VideoFrames constructed in OnBufferReceived() from buffers cached
  // in |client_buffers_|.
  base::WeakPtrFactory<VideoCaptureImpl> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_H_
