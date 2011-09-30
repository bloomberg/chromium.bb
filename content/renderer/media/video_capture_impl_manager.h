// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// VideoCaptureImplManager manages video capture devices in renderer process.
// The video capture clients use AddDevice() to get a pointer to
// video capture device. VideoCaputreImplManager supports multiple clients
// accessing same device.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_MANAGER_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_MANAGER_H_

#include <list>
#include <map>

#include "base/threading/thread.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "media/video/capture/video_capture.h"

class VideoCaptureImpl;
class VideoCaptureMessageFilter;

class CONTENT_EXPORT VideoCaptureImplManager
    : public base::RefCountedThreadSafe<VideoCaptureImplManager> {
 public:
  VideoCaptureImplManager();
  virtual ~VideoCaptureImplManager();

  // Called by video capture client |handler| to add device referenced
  // by |id| to VideoCaptureImplManager's list of opened device list.
  // A pointer to VideoCapture is returned to client so that client can
  // operate on that pointer, such as StartCaptrue, StopCapture.
  virtual media::VideoCapture* AddDevice(
      media::VideoCaptureSessionId id,
      media::VideoCapture::EventHandler* handler);

  // Called by video capture client |handler| to remove device referenced
  // by |id| from VideoCaptureImplManager's list of opened device list.
  virtual void RemoveDevice(media::VideoCaptureSessionId id,
                            media::VideoCapture::EventHandler* handler);

  VideoCaptureMessageFilter* video_capture_message_filter() const {
    return filter_;
  }

 private:
  struct Device {
    Device(VideoCaptureImpl* device,
           media::VideoCapture::EventHandler* handler);
    ~Device();

    VideoCaptureImpl* vc;
    std::list<media::VideoCapture::EventHandler*> clients;
  };

  void FreeDevice(VideoCaptureImpl* vc);

  typedef std::map<media::VideoCaptureSessionId, Device*> Devices;
  Devices devices_;
  base::Lock lock_;
  scoped_refptr<VideoCaptureMessageFilter> filter_;
  base::Thread thread_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureImplManager);
};

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_MANAGER_H_
