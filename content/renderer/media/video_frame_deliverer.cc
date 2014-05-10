// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_frame_deliverer.h"

#include "base/bind.h"
#include "base/location.h"

namespace content {
namespace {
void ResetCallback(scoped_ptr<VideoCaptureDeliverFrameCB> callback) {
  // |callback| will be deleted when this exits.
}
}  // namespace

VideoFrameDeliverer::VideoFrameDeliverer(
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop)
    : io_message_loop_(io_message_loop) {
  DCHECK(io_message_loop_);
}

VideoFrameDeliverer::~VideoFrameDeliverer() {
  DCHECK(callbacks_.empty());
}

void VideoFrameDeliverer::AddCallback(
    void* id,
    const VideoCaptureDeliverFrameCB& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  io_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&VideoFrameDeliverer::AddCallbackOnIO,
                 this, id, callback));
}

void VideoFrameDeliverer::AddCallbackOnIO(
    void* id,
    const VideoCaptureDeliverFrameCB& callback) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  callbacks_.push_back(std::make_pair(id, callback));
}

void VideoFrameDeliverer::RemoveCallback(void* id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  io_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&VideoFrameDeliverer::RemoveCallbackOnIO,
                 this, id, base::MessageLoopProxy::current()));
}

void VideoFrameDeliverer::RemoveCallbackOnIO(
    void* id, const scoped_refptr<base::MessageLoopProxy>& message_loop) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  std::vector<VideoIdCallbackPair>::iterator it = callbacks_.begin();
  for (; it != callbacks_.end(); ++it) {
    if (it->first == id) {
      // Callback is copied to heap and then deleted on the target thread.
      // The following code ensures that the callback is not referenced on
      // the stack.
      scoped_ptr<VideoCaptureDeliverFrameCB> callback;
      {
        callback.reset(new VideoCaptureDeliverFrameCB(it->second));
        callbacks_.erase(it);
      }
      message_loop->PostTask(
          FROM_HERE, base::Bind(&ResetCallback, base::Passed(&callback)));
      return;
    }
  }
}

void VideoFrameDeliverer::DeliverFrameOnIO(
    const scoped_refptr<media::VideoFrame>& frame,
    const media::VideoCaptureFormat& format) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  for (std::vector<VideoIdCallbackPair>::iterator it = callbacks_.begin();
       it != callbacks_.end(); ++it) {
    it->second.Run(frame, format);
  }
}

}  // namespace content
