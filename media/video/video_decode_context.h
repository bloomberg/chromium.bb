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
// 3. Upload a device specific buffer to some common VideoFrame storage types.
//    In many cases a VideoDecodeEngine provides its own buffer, these buffer
//    are usually device specific and a conversion step is needed. Instead of
//    handling these many cases in the renderer a VideoDecodeContext is used
//    to convert the device specific buffer to a common storage format, e.g.
//    GL textures or system memory. This way we keep the device specific code
//    in the VideoDecodeEngine and VideoDecodeContext pair.
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

  // Upload a device specific buffer to a video frame. The video frame was
  // allocated via AllocateVideoFrames().
  // This method is used if a VideoDecodeEngine cannot write directly to a
  // VideoFrame, e.g. upload should be done on a different thread, the subsystem
  // require some special treatment to generate a VideoFrame. The goal is to
  // keep VideoDecodeEngine a reusable component and also adapt to different
  // system by having a different VideoDecodeContext.
  //
  // |frame| is a VideoFrame allocated via AllocateVideoFrames().
  //
  // |buffer| is of type void*, it is of an internal type in VideoDecodeEngine
  // that points to the buffer that contains the video frame.
  // Implementor should know how to handle it.
  //
  // |task| is executed if the operation was completed successfully.
  // TODO(hclam): Rename this to ConvertToVideoFrame().
  virtual void UploadToVideoFrame(void* buffer, scoped_refptr<VideoFrame> frame,
                                  Task* task) = 0;

  // Destroy this context asynchronously. When the operation is done |task|
  // is called.
  //
  // ReleaseVideoFrames() need to be called with all the video frames allocated
 // before making this call.
  virtual void Destroy(Task* task) = 0;
};

}  // namespace media

#endif  // MEDIA_VIDEO_VIDEO_DECODE_CONTEXT_H_
