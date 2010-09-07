// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_VIDEO_DECODE_CONTEXT_H_
#define MEDIA_VIDEO_VIDEO_DECODE_CONTEXT_H_

#include "base/callback.h"

namespace media {

class VideoFrame;

// A VideoDecodeContext provides resources like output video frame storage and
// hardware decoder handle to a VideoDecodeEngine, it hides all the platform and
// subsystem details from the decode engine.
class VideoDecodeContext {
 public:
  typedef Callback2<int, VideoFrame*[]>::Type AllocationCompleteCallback;
  typedef Callback0::Type DestructionCompleteCallback;

  virtual ~VideoDecodeContext() {};

  // Obtain a handle to the hardware video decoder device. The type of the
  // handle is a contract between the implementation of VideoDecodeContext and
  // VideoDecodeEngine.
  //
  // If a hardware device is not needed this method should return NULL.
  virtual void* GetDevice() = 0;

  // Allocate |n| video frames with dimension |width| and |height|. |callback|
  // is called when allocation has completed.
  virtual void AllocateVideoFrames(int n, size_t width, size_t height,
                                   AllocationCompleteCallback* callback) = 0;

  // Release video frames allocated by the context. After making this call
  // VideoDecodeEngine should not use the VideoFrame allocated because they
  // could be destroyed.
  virtual void ReleaseVideoFrames(int n, VideoFrame* frames) = 0;

  // Destroy this context asynchronously. When the operation is done |callback|
  // is called.
  //
  // ReleaseVideoFrames() need to be called with all the video frames allocated
  // before making this call.
  virtual void Destroy(DestructionCompleteCallback* callback) = 0;
};

}  // namespace media

#endif  // MEDIA_VIDEO_VIDEO_DECODE_CONTEXT_H_
