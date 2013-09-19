// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/video_receiver/video_decoder.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/cast/video_receiver/codecs/vp8/vp8_decoder.h"

namespace media {
namespace cast {

VideoDecoder::VideoDecoder(scoped_refptr<CastThread> cast_thread,
                           const VideoReceiverConfig& video_config)
    : cast_thread_(cast_thread),
      codec_(video_config.codec),
      vp8_decoder_() {
  switch (video_config.codec) {
    case kVp8:
      // Initializing to use one core.
      vp8_decoder_.reset(new Vp8Decoder(1));
      break;
    case kH264:
      NOTIMPLEMENTED();
      break;
    case kExternalVideo:
      DCHECK(false) << "Invalid codec";
      break;
  }
}

VideoDecoder::~VideoDecoder() {}

void VideoDecoder::DecodeVideoFrame(
    const EncodedVideoFrame* encoded_frame,
    const base::TimeTicks render_time,
    const VideoFrameDecodedCallback& frame_decoded_callback,
    base::Closure frame_release_callback) {
  DecodeFrame(encoded_frame, render_time, frame_decoded_callback);
  // Done with the frame -> release.
  cast_thread_->PostTask(CastThread::MAIN, FROM_HERE, frame_release_callback);
}

void VideoDecoder::DecodeFrame(
    const EncodedVideoFrame* encoded_frame,
    const base::TimeTicks render_time,
    const VideoFrameDecodedCallback& frame_decoded_callback) {
  DCHECK(encoded_frame->codec == codec_) << "Invalid codec";
  // TODO(mikhal): Allow the application to allocate this memory.
  scoped_ptr<I420VideoFrame> video_frame(new I420VideoFrame());

  if (encoded_frame->data.size() > 0) {
    bool success = vp8_decoder_->Decode(*encoded_frame, video_frame.get());
    // Frame decoded - return frame to the user via callback.
    if (success) {
      cast_thread_->PostTask(CastThread::MAIN, FROM_HERE,
          base::Bind(frame_decoded_callback,
                      base::Passed(&video_frame), render_time));
    }
  }
}

}  // namespace cast
}  // namespace media
