// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_FRAME_DELIVERER_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_FRAME_DELIVERER_H_

#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/threading/thread_checker.h"
#include "content/common/media/video_capture.h"
#include "media/base/video_frame.h"

namespace content {

// VideoFrameDeliverer is a helper class used for registering
// VideoCaptureDeliverFrameCB on the main render thread to receive video frames
// on the IO-thread.
// Its used by MediaStreamVideoTrack.
class VideoFrameDeliverer
    : public base::RefCountedThreadSafe<VideoFrameDeliverer> {
 public:
  explicit VideoFrameDeliverer(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop);

  // Add |callback| to receive video frames on the IO-thread.
  // Must be called on the main render thread.
  void AddCallback(void* id, const VideoCaptureDeliverFrameCB& callback);

  // Removes |callback| associated with |id| from receiving video frames if |id|
  // has been added. It is ok to call RemoveCallback even if the |id| has not
  // been added. Note that the added callback will be reset on the main thread.
  // Must be called on the main render thread.
  void RemoveCallback(void* id);

  // Triggers all registered callbacks with |frame| and |format| as parameters.
  // Must be called on the IO-thread.
  virtual void DeliverFrameOnIO(
      const scoped_refptr<media::VideoFrame>& frame,
      const media::VideoCaptureFormat& format);

  const scoped_refptr<base::MessageLoopProxy>& io_message_loop() const {
    return io_message_loop_;
  }

 protected:
  void AddCallbackOnIO(void* id, const VideoCaptureDeliverFrameCB& callback);

  // Callback will be removed and then reset on the designated message loop.
  // It is ok to call RemoveCallback even if |id| has not been added.
  void RemoveCallbackOnIO(
      void* id, const scoped_refptr<base::MessageLoopProxy>& message_loop);

 protected:
  virtual ~VideoFrameDeliverer();
  const base::ThreadChecker& thread_checker() const {
    return thread_checker_;
  }

 private:
  friend class base::RefCountedThreadSafe<VideoFrameDeliverer>;

  // Used to DCHECK that AddCallback and RemoveCallback are called on the main
  // render thread.
  base::ThreadChecker thread_checker_;
  scoped_refptr<base::MessageLoopProxy> io_message_loop_;

  typedef std::pair<void*, VideoCaptureDeliverFrameCB> VideoIdCallbackPair;
  std::vector<VideoIdCallbackPair> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameDeliverer);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_FRAME_DELIVERER_H_
