// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/video_receiver/codecs/vp8/vp8_decoder.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8dx.h"
#include "ui/gfx/size.h"

namespace media {
namespace cast {

void LogFrameDecodedEvent(CastEnvironment* const cast_environment,
                          uint32 frame_id) {
// TODO(mikhal): Sort out passing of rtp_timestamp.
// cast_environment->Logging()->InsertFrameEvent(kVideoFrameDecoded,
//      0, frame_id);
}

Vp8Decoder::Vp8Decoder(scoped_refptr<CastEnvironment> cast_environment)
    : decoder_(new vpx_dec_ctx_t()),
      cast_environment_(cast_environment) {
  // Make sure that we initialize the decoder from the correct thread.
  cast_environment_->PostTask(CastEnvironment::VIDEO_DECODER, FROM_HERE,
        base::Bind(&Vp8Decoder::InitDecoder, base::Unretained(this)));
}

Vp8Decoder::~Vp8Decoder() {}

void Vp8Decoder::InitDecoder() {
  vpx_codec_dec_cfg_t cfg;
  // Initializing to use one core.
  cfg.threads = 1;
  vpx_codec_flags_t flags = VPX_CODEC_USE_POSTPROC;

  if (vpx_codec_dec_init(decoder_.get(), vpx_codec_vp8_dx(), &cfg, flags)) {
    DCHECK(false) << "VP8 decode error";
  }
}

bool Vp8Decoder::Decode(const EncodedVideoFrame* encoded_frame,
                        const base::TimeTicks render_time,
                        const VideoFrameDecodedCallback& frame_decoded_cb) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::VIDEO_DECODER));
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

  gfx::Size visible_size(img->d_w, img->d_h);
  gfx::Size full_size(img->stride[VPX_PLANE_Y], img->d_h);
  DCHECK(VideoFrame::IsValidConfig(VideoFrame::I420, visible_size,
                                   gfx::Rect(visible_size), full_size));
  // Temp timing setting - will sort out timing in a follow up cl.
  scoped_refptr<VideoFrame> decoded_frame =
      VideoFrame::CreateFrame(VideoFrame::I420, visible_size,
      gfx::Rect(visible_size), full_size, base::TimeDelta());

  // Copy each plane individually (need to account for stride).
  // TODO(mikhal): Eliminate copy once http://crbug.com/321856 is resolved.
  CopyPlane(VideoFrame::kYPlane, img->planes[VPX_PLANE_Y],
            img->stride[VPX_PLANE_Y], img->d_h, decoded_frame.get());
  CopyPlane(VideoFrame::kUPlane, img->planes[VPX_PLANE_U],
            img->stride[VPX_PLANE_U], (img->d_h + 1) / 2, decoded_frame.get());
  CopyPlane(VideoFrame::kVPlane, img->planes[VPX_PLANE_V],
            img->stride[VPX_PLANE_V], (img->d_h + 1) / 2, decoded_frame.get());

  // Log:: Decoding complete (should be called from the main thread).
  cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE, base::Bind(
      LogFrameDecodedEvent, cast_environment_,encoded_frame->frame_id));

  VLOG(1) << "Decoded frame " << frame_id_int;
  // Frame decoded - return frame to the user via callback.
  cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE,
      base::Bind(frame_decoded_cb, decoded_frame, render_time));

  return true;
}

}  // namespace cast
}  // namespace media

