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
                            const VideoCaptureCapability& capability);
  virtual void StopCapture(media::VideoCapture::EventHandler* handler);
  virtual void FeedBuffer(scoped_refptr<VideoFrameBuffer> buffer);
  virtual bool CaptureStarted();
  virtual int CaptureWidth();
  virtual int CaptureHeight();
  virtual int CaptureFrameRate();

  // VideoCaptureMessageFilter::Delegate interface.
  virtual void OnBufferCreated(base::SharedMemoryHandle handle,
                               int length, int buffer_id);
  virtual void OnBufferReceived(int buffer_id, base::Time timestamp);
  virtual void OnStateChanged(const media::VideoCapture::State& state);
  virtual void OnDeviceInfoReceived(
      const media::VideoCaptureParams& device_info);
  virtual void OnDelegateAdded(int32 device_id);

  bool pending_start() {
    return (new_params_.width > 0 && new_params_.height > 0);
  }

 private:
  friend class VideoCaptureImplManager;
  friend class VideoCaptureImplTest;
  friend class MockVideoCaptureImpl;

  struct DIBBuffer {
   public:
    DIBBuffer(base::SharedMemory* d,
              media::VideoCapture::VideoFrameBuffer* ptr);
    ~DIBBuffer();

    base::SharedMemory* dib;
    scoped_refptr<media::VideoCapture::VideoFrameBuffer> mapped_memory;
  };

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
  void DeInit(Task* task);
  void StopDevice();
  void RestartCapture();
  void StartCaptureInternal();
  void AddDelegateOnIOThread();
  void RemoveDelegateOnIOThread(Task* task);
  virtual void Send(IPC::Message* message);

  scoped_refptr<VideoCaptureMessageFilter> message_filter_;
  scoped_refptr<base::MessageLoopProxy> ml_proxy_;
  int device_id_;

  // Pool of DIBs.
  typedef std::map<int, DIBBuffer*> CachedDIB;
  CachedDIB cached_dibs_;

  // DIBs at client side. The mapped value |int| means number of clients which
  // hold this dib.
  typedef std::map<media::VideoCapture::VideoFrameBuffer*, int> ClientSideDIB;
  ClientSideDIB client_side_dibs_;

  typedef std::map<media::VideoCapture::EventHandler*, VideoCaptureCapability>
      ClientInfo;
  ClientInfo clients_;
  std::list<media::VideoCapture::EventHandler*> master_clients_;

  ClientInfo pending_clients_;

  media::VideoFrame::Format video_type_;

  // The parameter is being used in current capture session. A capture session
  // starts with StartCapture and ends with StopCapture.
  media::VideoCaptureParams current_params_;

  // The information about the device sent from browser process side.
  media::VideoCaptureParams device_info_;
  bool device_info_available_;

  // The parameter will be used in next capture session.
  media::VideoCaptureParams new_params_;
  State state_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureImpl);
};

DISABLE_RUNNABLE_METHOD_REFCOUNT(VideoCaptureImpl);

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_H_
