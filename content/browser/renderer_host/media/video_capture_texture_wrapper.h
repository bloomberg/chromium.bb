// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_TEXTURE_WRAPPER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_TEXTURE_WRAPPER_H_

#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "content/browser/renderer_host/media/video_capture_device_client.h"
#include "content/common/content_export.h"
#include "media/video/capture/video_capture_device.h"

namespace content {

// VideoCaptureDeviceClient derived class for copying incoming memory-backed
// video data into a GpuMemoryBuffer (in turn into a VideoFrame), and sending it
// to |controller_|. Construction happens on IO Thread, whereas calls to
// OnIncomingCapturedData() come from wherever capture happens which might be,
// and often is, an OS thread different from Device Thread.
//
// It has an internal ref counted TextureWrapperDelegate class used to do all
// the work. This class creates and manages the necessary interactions with the
// GPU process for the texture uploading. This delegate is needed since the
// frame producer needs to own us, but we need to launch PostTask()s.
//
// TODO(mcasas): ctor |frame_format| is used for early GpuMemoryBuffer pool
// allocation, but VideoCaptureDevices might provide a different resolution when
// calling OnIncomingCapturedData(). What should we do then...?

class CONTENT_EXPORT VideoCaptureTextureWrapper final
    : public VideoCaptureDeviceClient {
 public:
  VideoCaptureTextureWrapper(
      const base::WeakPtr<VideoCaptureController>& controller,
      const scoped_refptr<VideoCaptureBufferPool>& buffer_pool,
      const scoped_refptr<base::SingleThreadTaskRunner>& capture_task_runner,
      const media::VideoCaptureFormat& capture_format);
  ~VideoCaptureTextureWrapper() override;

  // Single media::VideoCaptureDevice::Client overriden method.
  void OnIncomingCapturedData(const uint8* data,
                              int length,
                              const media::VideoCaptureFormat& frame_format,
                              int clockwise_rotation,
                              const base::TimeTicks& timestamp) override;

 private:
  class TextureWrapperDelegate;
  // Internal delegate doing most of the work.
  scoped_refptr<TextureWrapperDelegate> wrapper_delegate_;
  // Reference to Capture Thread task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureTextureWrapper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_TEXTURE_WRAPPER_H_
