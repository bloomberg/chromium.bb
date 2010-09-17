// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_VIDEO_DECODE_CONTEXT_H_
#define MEDIA_VIDEO_VIDEO_DECODE_CONTEXT_H_

#include <vector>

#include "base/task.h"
#include "media/base/video_frame.h"

namespace media {

class VideoFrame;

// A VideoDecodeContext is used by VideoDecodeEngine to provide the following
// functions:
//
// 1. Provides access to hardware video decoding device.
// 2. Allocate VideoFrame objects that are used to carry the decoded video
//    frames.
class VideoDecodeContext {
 public:
  virtual ~VideoDecodeContext() {};

  // Obtain a handle to the hardware video decoder device. The type of the
  // handle is a contract between the implementation of VideoDecodeContext and
  // VideoDecodeEngine.
  //
  // If a hardware device is not needed this method should return NULL.
  virtual void* GetDevice() = 0;

  // Allocate |n| video frames with dimension |width| and |height|. |task|
  // is called when allocation has completed.
  //
  // |frames| is the output parameter for VideFrame(s) allocated.
  virtual void AllocateVideoFrames(
      int n, size_t width, size_t height, VideoFrame::Format format,
      std::vector<scoped_refptr<VideoFrame> >* frames,
      Task* task) = 0;

  // Release all video frames allocated by the context. After making this call
  // VideoDecodeEngine should not use the VideoFrame allocated because they
  // could be destroyed.
  virtual void ReleaseAllVideoFrames() = 0;

  // Destroy this context asynchronously. When the operation is done |task|
  // is called.
  //
  // ReleaseVideoFrames() need to be called with all the video frames allocated
  // before making this call.
  virtual void Destroy(Task* task) = 0;
};

}  // namespace media

#endif  // MEDIA_VIDEO_VIDEO_DECODE_CONTEXT_H_
