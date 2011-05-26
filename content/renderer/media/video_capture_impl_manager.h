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

#include "base/message_loop_proxy.h"
#include "base/synchronization/lock.h"
#include "content/renderer/media/video_capture_impl.h"
#include "content/renderer/video_capture_message_filter.h"
#include "media/base/callback.h"
#include "media/base/message_loop_factory.h"
#include "media/video/capture/video_capture.h"

class VideoCaptureImplManager {
 public:
  VideoCaptureImplManager();
  ~VideoCaptureImplManager();

  // Called by video capture client |handler| to add device referenced
  // by |id| to VideoCaptureImplManager's list of opened device list.
  // A pointer to VideoCapture is returned to client so that client can
  // operate on that pointer, such as StartCaptrue, StopCapture.
  static media::VideoCapture* AddDevice(
      media::VideoCaptureSessionId id,
      media::VideoCapture::EventHandler* handler);

  // Called by video capture client |handler| to remove device referenced
  // by |id| from VideoCaptureImplManager's list of opened device list.
  static void RemoveDevice(media::VideoCaptureSessionId id,
                           media::VideoCapture::EventHandler* handler);

  static VideoCaptureImplManager* GetInstance();

 private:
  struct Device {
    Device();
    Device(VideoCaptureImpl* device,
           media::VideoCapture::EventHandler* handler);
    ~Device();

    VideoCaptureImpl* vc;
    std::list<media::VideoCapture::EventHandler*> clients;
  };

  void FreeDevice(VideoCaptureImpl* vc);

  typedef std::map<media::VideoCaptureSessionId, Device> Devices;
  Devices devices_;
  base::Lock lock_;
  scoped_refptr<base::MessageLoopProxy> ml_proxy_;
  scoped_ptr<media::MessageLoopFactory> ml_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureImplManager);
};

DISABLE_RUNNABLE_METHOD_REFCOUNT(VideoCaptureImplManager);

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_MANAGER_H_
