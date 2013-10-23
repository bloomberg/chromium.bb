// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/video_receiver/codecs/vp8/vp8_decoder.h"

#include "base/logging.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8dx.h"

namespace media {
namespace cast {

Vp8Decoder::Vp8Decoder(int number_of_cores) {
  decoder_.reset(new vpx_dec_ctx_t());
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

bool Vp8Decoder::Decode(const EncodedVideoFrame& input_image,
                        I420VideoFrame* decoded_frame) {
  VLOG(1) << "VP8 decode frame:" << static_cast<int>(input_image.frame_id)
          << " sized:" << input_image.data.size();

  if (input_image.data.empty()) return false;

  vpx_codec_iter_t iter = NULL;
  vpx_image_t* img;
  if (vpx_codec_decode(decoder_.get(),
                       input_image.data.data(),
                       static_cast<unsigned int>(input_image.data.size()),
                       0,
                       1 /* real time*/)) {
    VLOG(1) << "Failed to decode VP8 frame.";
    return false;
  }

  img = vpx_codec_get_frame(decoder_.get(), &iter);
  if (img == NULL) {
    VLOG(1) << "Skip rendering VP8 frame:"
            << static_cast<int>(input_image.frame_id);
    return false;
  }

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

  return true;
}

}  // namespace cast
}  // namespace media

