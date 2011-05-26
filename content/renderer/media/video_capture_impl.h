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

#include "content/renderer/video_capture_message_filter.h"
#include "media/base/callback.h"
#include "media/video/capture/video_capture.h"
#include "ui/gfx/surface/transport_dib.h"

namespace base {
class MessageLoopProxy;
}

class VideoCaptureImpl
    : public media::VideoCapture,
      public VideoCaptureMessageFilter::Delegate {
 public:
  // media::VideoCapture interface.
  virtual void StartCapture(media::VideoCapture::EventHandler* handler,
                            const VideoCaptureCapability& capability);
  virtual void StopCapture(media::VideoCapture::EventHandler* handler);
  virtual bool CaptureStarted();
  virtual int CaptureWidth();
  virtual int CaptureHeight();
  virtual int CaptureFrameRate();

  // VideoCaptureMessageFilter::Delegate interface.
  virtual void OnBufferReceived(TransportDIB::Handle handle,
                                base::Time timestamp);
  virtual void OnStateChanged(const media::VideoCapture::State& state);
  virtual void OnDeviceInfoReceived(
      const media::VideoCaptureParams& device_info);
  virtual void OnDelegateAdded(int32 device_id);

  bool pending_start() {
    return (new_width_ > 0 && new_height_ > 0);
  }

 private:
  friend class VideoCaptureImplManager;
  friend class VideoCaptureImplTest;

  enum State {
    kStarted,
    kStopping,
    kStopped
  };

  struct DIBBuffer {
   public:
    DIBBuffer(TransportDIB* d, media::VideoCapture::VideoFrameBuffer* ptr);
    ~DIBBuffer();

    TransportDIB* dib;
    scoped_refptr<media::VideoCapture::VideoFrameBuffer> mapped_memory;
  };

  VideoCaptureImpl(media::VideoCaptureSessionId id,
                   scoped_refptr<base::MessageLoopProxy> ml_proxy,
                   VideoCaptureMessageFilter* filter);
  virtual ~VideoCaptureImpl();

  void Init();
  void DeInit(Task* task);
  void StopDevice();
  void RestartCapture();
  void StartCaptureInternal();
  void AddDelegateOnIOThread();
  void RemoveDelegateOnIOThread(Task* task);

  scoped_refptr<VideoCaptureMessageFilter> message_filter_;
  media::VideoCaptureSessionId session_id_;
  scoped_refptr<base::MessageLoopProxy> ml_proxy_;
  int device_id_;

  // Pool of DIBs.
  typedef std::list<DIBBuffer*> CachedDIB;
  CachedDIB cached_dibs_;

  typedef std::map<media::VideoCapture::EventHandler*, VideoCaptureCapability>
      ClientInfo;
  ClientInfo clients_;
  std::list<media::VideoCapture::EventHandler*> master_clients_;

  ClientInfo pending_clients_;

  int width_;
  int height_;
  int frame_rate_;
  media::VideoFrame::Format video_type_;

  int new_width_;
  int new_height_;
  State state_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureImpl);
};

DISABLE_RUNNABLE_METHOD_REFCOUNT(VideoCaptureImpl);

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_H_
