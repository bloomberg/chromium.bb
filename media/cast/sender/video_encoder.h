// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_SENDER_VIDEO_ENCODER_H_
#define MEDIA_CAST_SENDER_VIDEO_ENCODER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"

namespace media {
namespace cast {

// All these functions are called from the main cast thread.
class VideoEncoder {
 public:
  typedef base::Callback<void(scoped_ptr<EncodedFrame>)> FrameEncodedCallback;

  virtual ~VideoEncoder() {}

  // If true is returned, the Encoder has accepted the request and will process
  // it asynchronously, running |frame_encoded_callback| on the MAIN
  // CastEnvironment thread with the result.  If false is returned, nothing
  // happens and the callback will not be run.
  virtual bool EncodeVideoFrame(
      const scoped_refptr<media::VideoFrame>& video_frame,
      const base::TimeTicks& reference_time,
      const FrameEncodedCallback& frame_encoded_callback) = 0;

  // Inform the encoder about the new target bit rate.
  virtual void SetBitRate(int new_bit_rate) = 0;

  // Inform the encoder to encode the next frame as a key frame.
  virtual void GenerateKeyFrame() = 0;

  // Inform the encoder to only reference frames older or equal to frame_id;
  virtual void LatestFrameIdToReference(uint32 frame_id) = 0;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_SENDER_VIDEO_ENCODER_H_
