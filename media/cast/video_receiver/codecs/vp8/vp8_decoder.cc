// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/video_receiver/codecs/vp8/vp8_decoder.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8dx.h"

namespace media {
namespace cast {

Vp8Decoder::Vp8Decoder(int number_of_cores,
                       scoped_refptr<CastEnvironment> cast_environment)
    : decoder_(new vpx_dec_ctx_t()),
      cast_environment_(cast_environment) {
  InitDecode(number_of_cores);
}

Vp8Decoder::~Vp8Decoder() {}

void Vp8Decoder::InitDecode(int number_of_cores) {
  vpx_codec_dec_cfg_t  cfg;
  cfg.threads = number_of_cores;
  vpx_codec_flags_t flags = VPX_CODEC_USE_POSTPROC;

  if (vpx_codec_dec_init(decoder_.get(), vpx_codec_vp8_dx(), &cfg, flags)) {
    DCHECK(false) << "VP8 decode error";
  }
}

bool Vp8Decoder::Decode(const EncodedVideoFrame* encoded_frame,
                        const base::TimeTicks render_time,
                        const VideoFrameDecodedCallback& frame_decoded_cb) {
  const int frame_id_int = static_cast<int>(encoded_frame->frame_id);
  VLOG(1) << "VP8 decode frame:" << frame_id_int
          << " sized:" << encoded_frame->data.size();

  if (encoded_frame->data.empty()) return false;

  vpx_codec_iter_t iter = NULL;
  vpx_image_t* img;
  if (vpx_codec_decode(
      decoder_.get(),
      reinterpret_cast<const uint8*>(encoded_frame->data.data()),
      static_cast<unsigned int>(encoded_frame->data.size()),
      0,
      1 /* real time*/)) {
    VLOG(1) << "Failed to decode VP8 frame.";
    return false;
  }

  img = vpx_codec_get_frame(decoder_.get(), &iter);
  if (img == NULL) {
    VLOG(1) << "Skip rendering VP8 frame:" << frame_id_int;
    return false;
  }

  scoped_ptr<I420VideoFrame> decoded_frame(new I420VideoFrame());

  // The img is only valid until the next call to vpx_codec_decode.
  // Populate the decoded image.
  decoded_frame->width = img->d_w;
  decoded_frame->height = img->d_h;

  decoded_frame->y_plane.stride = img->stride[VPX_PLANE_Y];
  decoded_frame->y_plane.length = img->stride[VPX_PLANE_Y] * img->d_h;
  decoded_frame->y_plane.data = new uint8[decoded_frame->y_plane.length];
  memcpy(decoded_frame->y_plane.data, img->planes[VPX_PLANE_Y],
         decoded_frame->y_plane.length);

  decoded_frame->u_plane.stride = img->stride[VPX_PLANE_U];
  decoded_frame->u_plane.length = img->stride[VPX_PLANE_U] * (img->d_h + 1) / 2;
  decoded_frame->u_plane.data = new uint8[decoded_frame->u_plane.length];
  memcpy(decoded_frame->u_plane.data, img->planes[VPX_PLANE_U],
         decoded_frame->u_plane.length);

  decoded_frame->v_plane.stride = img->stride[VPX_PLANE_V];
  decoded_frame->v_plane.length = img->stride[VPX_PLANE_V] * (img->d_h + 1) / 2;
  decoded_frame->v_plane.data = new uint8[decoded_frame->v_plane.length];

  memcpy(decoded_frame->v_plane.data, img->planes[VPX_PLANE_V],
         decoded_frame->v_plane.length);

  // Return frame.
  VLOG(1) << "Decoded frame " << frame_id_int;
  // Frame decoded - return frame to the user via callback.
  cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE,
      base::Bind(frame_decoded_cb, base::Passed(&decoded_frame), render_time));

  return true;
}

}  // namespace cast
}  // namespace media

