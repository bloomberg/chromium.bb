// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// VideoCaptureImpl represents a capture device in renderer process. It provides
// interfaces for clients to Start/Stop capture. It also communicates to clients
// when buffer is ready, state of capture device is changed.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_H_

#include <list>
#include <map>

#include "content/common/content_export.h"
#include "content/renderer/media/video_capture_message_filter.h"
#include "media/video/capture/video_capture.h"

namespace base {
class MessageLoopProxy;
}

class CONTENT_EXPORT VideoCaptureImpl
    : public media::VideoCapture, public VideoCaptureMessageFilter::Delegate {
 public:
  // media::VideoCapture interface.
  virtual void StartCapture(media::VideoCapture::EventHandler* handler,
                            const VideoCaptureCapability& capability) OVERRIDE;
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
  virtual void OnStateChanged(const media::VideoCapture::State& state) OVERRIDE;
  virtual void OnDeviceInfoReceived(
      const media::VideoCaptureParams& device_info) OVERRIDE;
  virtual void OnDelegateAdded(int32 device_id) OVERRIDE;

 private:
  friend class VideoCaptureImplManager;
  friend class VideoCaptureImplTest;
  friend class MockVideoCaptureImpl;

  struct DIBBuffer;

  VideoCaptureImpl(media::VideoCaptureSessionId id,
                   scoped_refptr<base::MessageLoopProxy> ml_proxy,
                   VideoCaptureMessageFilter* filter);
  virtual ~VideoCaptureImpl();

  void DoStartCapture(media::VideoCapture::EventHandler* handler,
                      const VideoCaptureCapability& capability);
  void DoStopCapture(media::VideoCapture::EventHandler* handler);
  void DoFeedBuffer(scoped_refptr<VideoFrameBuffer> buffer);

  void DoBufferCreated(base::SharedMemoryHandle handle,
                       int length, int buffer_id);
  void DoBufferReceived(int buffer_id, base::Time timestamp);
  void DoStateChanged(const media::VideoCapture::State& state);
  void DoDeviceInfoReceived(const media::VideoCaptureParams& device_info);
  void DoDelegateAdded(int32 device_id);

  void Init();
  void DeInit(base::Closure task);
  void StopDevice();
  void RestartCapture();
  void StartCaptureInternal();
  void AddDelegateOnIOThread();
  void RemoveDelegateOnIOThread(base::Closure task);
  virtual void Send(IPC::Message* message);

  // Helpers.
  bool ClientHasDIB();

  scoped_refptr<VideoCaptureMessageFilter> message_filter_;
  scoped_refptr<base::MessageLoopProxy> ml_proxy_;
  int device_id_;

  // Pool of DIBs.
  typedef std::map<int /* buffer_id */, DIBBuffer*> CachedDIB;
  CachedDIB cached_dibs_;

  typedef std::map<media::VideoCapture::EventHandler*, VideoCaptureCapability>
      ClientInfo;
  ClientInfo clients_;

  ClientInfo clients_pending_on_filter_;
  ClientInfo clients_pending_on_restart_;

  media::VideoFrame::Format video_type_;

  // The parameter is being used in current capture session. A capture session
  // starts with StartCapture and ends with StopCapture.
  media::VideoCaptureParams current_params_;

  // The information about the device sent from browser process side.
  media::VideoCaptureParams device_info_;
  bool device_info_available_;

  State state_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureImpl);
};

DISABLE_RUNNABLE_METHOD_REFCOUNT(VideoCaptureImpl);

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_H_
