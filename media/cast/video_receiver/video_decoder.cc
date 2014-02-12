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

VideoDecoder::VideoDecoder(const VideoReceiverConfig& video_config,
                           scoped_refptr<CastEnvironment> cast_environment)
    : codec_(video_config.codec), vp8_decoder_() {
  switch (video_config.codec) {
    case transport::kVp8:
      vp8_decoder_.reset(new Vp8Decoder(cast_environment));
      break;
    case transport::kH264:
      NOTIMPLEMENTED();
      break;
  }
}

VideoDecoder::~VideoDecoder() {}

bool VideoDecoder::DecodeVideoFrame(
    const transport::EncodedVideoFrame* encoded_frame,
    const base::TimeTicks render_time,
    const VideoFrameDecodedCallback& frame_decoded_cb) {
  DCHECK(encoded_frame->codec == codec_) << "Invalid codec";
  DCHECK_GT(encoded_frame->data.size(), GG_UINT64_C(0)) << "Empty video frame";
  return vp8_decoder_->Decode(encoded_frame, render_time, frame_decoded_cb);
}

}  // namespace cast
}  // namespace media
