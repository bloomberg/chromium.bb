// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RTP_RECEVIER_CODECS_VP8_VP8_DECODER_H_
#define MEDIA_CAST_RTP_RECEVIER_CODECS_VP8_VP8_DECODER_H_

#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_receiver.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_decoder.h"

typedef struct vpx_codec_ctx vpx_dec_ctx_t;

// TODO(mikhal): Look into reusing VpxVideoDecoder.
namespace media {
namespace cast {

// This class is not thread safe; it's only called from the cast video decoder
// thread.
class Vp8Decoder : public base::NonThreadSafe {
 public:
  explicit Vp8Decoder(scoped_refptr<CastEnvironment> cast_environment);
  ~Vp8Decoder();

  // Decode frame - The decoded frame will be passed via the callback.
  // Will return false in case of error, and then it's up to the caller to
  // release the memory.
  // Ownership of the encoded_frame does not pass to the Vp8Decoder.
  bool Decode(const transport::EncodedVideoFrame* encoded_frame,
              const base::TimeTicks render_time,
              const VideoFrameDecodedCallback& frame_decoded_cb);

 private:
  // Initialize the decoder.
  void InitDecoder();
  scoped_ptr<vpx_dec_ctx_t> decoder_;
  scoped_refptr<CastEnvironment> cast_environment_;

  DISALLOW_COPY_AND_ASSIGN(Vp8Decoder);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RTP_RECEVIER_CODECS_VP8_VP8_DECODER_H_
