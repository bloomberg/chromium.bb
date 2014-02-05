// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// VideoCaptureImplManager owns VideoCaptureImpl objects. Clients who
// want access to a video capture device call UseDevice() to get a handle
// to VideoCaptureImpl.
//
// THREADING
//
// VideoCaptureImplManager lives only on the render thread. All methods
// must be called on this thread.
//
// The handle returned by UseDevice() is thread-safe. It ensures
// destruction is handled on the render thread.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_MANAGER_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_MANAGER_H_

#include <map>

#include "base/callback.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "media/video/capture/video_capture.h"

namespace content {

class VideoCaptureImpl;
class VideoCaptureImplManager;
class VideoCaptureMessageFilter;

// Thread-safe wrapper for a media::VideoCapture object. During
// destruction |destruction_cb| is called. This mechanism is used
// by VideoCaptureImplManager to ensure de-initialization and
// destruction of the media::VideoCapture object happens on the render
// thread.
class CONTENT_EXPORT VideoCaptureHandle : media::VideoCapture {
 public:
  virtual ~VideoCaptureHandle();

  // media::VideoCapture implementations.
  virtual void StartCapture(
      EventHandler* handler,
      const media::VideoCaptureParams& params) OVERRIDE;
  virtual void StopCapture(EventHandler* handler) OVERRIDE;
  virtual bool CaptureStarted() OVERRIDE;
  virtual int CaptureFrameRate() OVERRIDE;
  virtual void GetDeviceSupportedFormats(
      const DeviceFormatsCallback& callback) OVERRIDE;
  virtual void GetDeviceFormatsInUse(
      const DeviceFormatsInUseCallback& callback) OVERRIDE;

 private:
  friend class VideoCaptureImplManager;

  VideoCaptureHandle(media::VideoCapture* impl,
                     base::Closure destruction_cb);

  media::VideoCapture* impl_;
  base::Closure destruction_cb_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureHandle);
};

class CONTENT_EXPORT VideoCaptureImplManager {
 public:
  VideoCaptureImplManager();
  virtual ~VideoCaptureImplManager();

  // Returns a video capture device referenced by |id|.
  scoped_ptr<VideoCaptureHandle> UseDevice(media::VideoCaptureSessionId id);

  // Make all existing VideoCaptureImpl instances stop/resume delivering
  // video frames to their clients, depends on flag |suspend|.
  void SuspendDevices(bool suspend);

  VideoCaptureMessageFilter* video_capture_message_filter() const {
    return filter_.get();
  }

 protected:
  // Used in tests to inject a mock VideoCaptureImpl.
  virtual VideoCaptureImpl* CreateVideoCaptureImpl(
      media::VideoCaptureSessionId id,
      VideoCaptureMessageFilter* filter) const;

 private:
  void UnrefDevice(media::VideoCaptureSessionId id);

  // The int is used to count clients of the corresponding VideoCaptureImpl.
  typedef std::map<media::VideoCaptureSessionId,
                   std::pair<int, linked_ptr<VideoCaptureImpl> > >
      VideoCaptureDeviceMap;
  VideoCaptureDeviceMap devices_;

  scoped_refptr<VideoCaptureMessageFilter> filter_;

  // Following two members are bound to the render thread.
  base::WeakPtrFactory<VideoCaptureImplManager> weak_factory_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureImplManager);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_MANAGER_H_
