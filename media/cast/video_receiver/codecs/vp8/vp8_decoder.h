// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_VIDEO_RECEVIER_CODECS_VP8_VP8_DECODER_H_
#define MEDIA_CAST_VIDEO_RECEVIER_CODECS_VP8_VP8_DECODER_H_

#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_receiver.h"
#include "media/cast/video_receiver/software_video_decoder.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_decoder.h"

typedef struct vpx_codec_ctx vpx_dec_ctx_t;

// TODO(mikhal): Look into reusing VpxVideoDecoder.
namespace media {
namespace cast {

class Vp8Decoder : public SoftwareVideoDecoder {
 public:
  explicit Vp8Decoder(scoped_refptr<CastEnvironment> cast_environment);
  virtual ~Vp8Decoder();

  // SoftwareVideoDecoder implementations.
  virtual bool Decode(const transport::EncodedVideoFrame* encoded_frame,
                      const base::TimeTicks render_time,
                      const VideoFrameDecodedCallback& frame_decoded_cb)
      OVERRIDE;

 private:
  // Initialize the decoder.
  void InitDecoder();
  scoped_ptr<vpx_dec_ctx_t> decoder_;
  scoped_refptr<CastEnvironment> cast_environment_;

  DISALLOW_COPY_AND_ASSIGN(Vp8Decoder);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_VIDEO_RECEVIER_CODECS_VP8_VP8_DECODER_H_
