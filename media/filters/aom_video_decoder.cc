// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/aom_video_decoder.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/video_util.h"
#include "third_party/libyuv/include/libyuv/convert.h"

// Include libaom header files.
extern "C" {
#include "third_party/libaom/source/libaom/aom/aom_decoder.h"
#include "third_party/libaom/source/libaom/aom/aom_frame_buffer.h"
#include "third_party/libaom/source/libaom/aom/aomdx.h"
}

namespace media {

// Returns the number of threads.
static int GetAomVideoDecoderThreadCount(const VideoDecoderConfig& config) {
  // Always try to use at least two threads for video decoding. There is little
  // reason not to since current day CPUs tend to be multi-core and we measured
  // performance benefits on older machines such as P4s with hyperthreading.
  constexpr int kDecodeThreads = 2;
  int decode_threads = kDecodeThreads;

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  std::string threads(cmd_line->GetSwitchValueASCII(switches::kVideoThreads));
  if (threads.empty() || !base::StringToInt(threads, &decode_threads)) {
    // For AOM decode when using the default thread count, increase the number
    // of decode threads to equal the maximum number of tiles possible for
    // higher resolution streams.
    decode_threads = std::min(config.coded_size().width() / 256,
                              base::SysInfo::NumberOfProcessors());
  }

  constexpr int kMaxDecodeThreads = 32;
  return std::min(std::max(decode_threads, 0), kMaxDecodeThreads);
}

static VideoPixelFormat AomImgFmtToVideoPixelFormat(const aom_image_t* img) {
  switch (img->fmt) {
    case AOM_IMG_FMT_I420:
      return PIXEL_FORMAT_I420;
    case AOM_IMG_FMT_I422:
      return PIXEL_FORMAT_I422;
    case AOM_IMG_FMT_I444:
      return PIXEL_FORMAT_I444;

    case AOM_IMG_FMT_I42016:
      switch (img->bit_depth) {
        case 8:
          // libaom compiled in high bit depth mode returns 8 bit content in a
          // 16-bit type. This should be handled by the caller instead of using
          // this function.
          NOTREACHED();
          return PIXEL_FORMAT_UNKNOWN;
        case 10:
          return PIXEL_FORMAT_YUV420P10;
        case 12:
          return PIXEL_FORMAT_YUV420P12;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << img->bit_depth;
          return PIXEL_FORMAT_UNKNOWN;
      }

    case AOM_IMG_FMT_I42216:
      switch (img->bit_depth) {
        case 10:
          return PIXEL_FORMAT_YUV422P10;
        case 12:
          return PIXEL_FORMAT_YUV422P12;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << img->bit_depth;
          return PIXEL_FORMAT_UNKNOWN;
      }

    case AOM_IMG_FMT_I44416:
      switch (img->bit_depth) {
        case 10:
          return PIXEL_FORMAT_YUV444P10;
        case 12:
          return PIXEL_FORMAT_YUV444P12;
          break;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << img->bit_depth;
          return PIXEL_FORMAT_UNKNOWN;
      }

    default:
      break;
  }

  DLOG(ERROR) << "Unsupported pixel format: " << img->fmt;
  return PIXEL_FORMAT_UNKNOWN;
}

static void SetColorSpaceForFrame(const aom_image_t* img,
                                  const VideoDecoderConfig& config,
                                  VideoFrame* frame) {

  gfx::ColorSpace::RangeID range = img->range == AOM_CR_FULL_RANGE
                                       ? gfx::ColorSpace::RangeID::FULL
                                       : gfx::ColorSpace::RangeID::LIMITED;

  // AOM color space defines match ISO 23001-8:2016 via ISO/IEC 23091-4/ITU-T
  // H.273.
  // http://av1-spec.argondesign.com/av1-spec/av1-spec.html#color-config-semantics
  media::VideoColorSpace color_space(img->cp, img->tc, img->mc, range);

  // If the bitstream doesn't specify a color space, use the one
  // from the container.
  if (!color_space.IsSpecified())
    color_space = config.color_space_info();

  frame->set_color_space(color_space.ToGfxColorSpace());
}

// Copies plane of 8-bit pixels out of a 16-bit values.
static_assert(AOM_PLANE_Y == VideoFrame::kYPlane, "Y plane must match AOM");
static_assert(AOM_PLANE_U == VideoFrame::kUPlane, "U plane must match AOM");
static_assert(AOM_PLANE_V == VideoFrame::kVPlane, "V plane must match AOM");
static void PackPlane(int plane, const aom_image_t* img, VideoFrame* frame) {
  const uint8_t* in_plane = img->planes[plane];
  uint8_t* out_plane = frame->visible_data(plane);

  const int in_stride = img->stride[plane];
  const int out_stride = frame->stride(plane);
  const int rows = frame->rows(plane);
  const int cols =
      VideoFrame::Columns(plane, frame->format(), frame->coded_size().width());

  for (int row = 0; row < rows; ++row) {
    const uint8_t* in_line = in_plane + (row * in_stride);
    uint8_t* out_line = out_plane + (row * out_stride);
    for (int col = 0; col < cols; ++col)
      out_line[col] = in_line[col * 2];
  }
}

AomVideoDecoder::AomVideoDecoder(MediaLog* media_log) : media_log_(media_log) {
  DETACH_FROM_THREAD(thread_checker_);
}

AomVideoDecoder::~AomVideoDecoder() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CloseDecoder();
}

std::string AomVideoDecoder::GetDisplayName() const {
  return "AomVideoDecoder";
}

void AomVideoDecoder::Initialize(
    const VideoDecoderConfig& config,
    bool /* low_delay */,
    CdmContext* /* cdm_context */,
    const InitCB& init_cb,
    const OutputCB& output_cb,
    const WaitingForDecryptionKeyCB& /* waiting_for_decryption_key_cb */) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(config.IsValidConfig());

  InitCB bound_init_cb = BindToCurrentLoop(init_cb);
  if (config.is_encrypted() || config.codec() != kCodecAV1) {
    bound_init_cb.Run(false);
    return;
  }

  // Clear any previously initialized decoder.
  CloseDecoder();

  aom_codec_dec_cfg_t aom_config = {0};
  aom_config.w = config.coded_size().width();
  aom_config.h = config.coded_size().height();
  aom_config.threads = GetAomVideoDecoderThreadCount(config);

  // TODO(dalecurtis): Refactor the MemoryPool and OffloadTaskRunner out of
  // VpxVideoDecoder so that they can be used here for zero copy decoding off
  // of the media thread.

  std::unique_ptr<aom_codec_ctx> context = std::make_unique<aom_codec_ctx>();
  if (aom_codec_dec_init(context.get(), aom_codec_av1_dx(), &aom_config,
                         0 /* flags */) != AOM_CODEC_OK) {
    MEDIA_LOG(ERROR, media_log_) << "aom_codec_dec_init() failed: "
                                 << aom_codec_error(aom_decoder_.get());
    bound_init_cb.Run(false);
    return;
  }

  config_ = config;
  state_ = DecoderState::kNormal;
  output_cb_ = BindToCurrentLoop(output_cb);
  aom_decoder_ = std::move(context);
  bound_init_cb.Run(true);
}

void AomVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                             const DecodeCB& decode_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(buffer);
  DCHECK(!decode_cb.is_null());
  DCHECK_NE(state_, DecoderState::kUninitialized)
      << "Called Decode() before successful Initialize()";

  DecodeCB bound_decode_cb = BindToCurrentLoop(decode_cb);

  if (state_ == DecoderState::kError) {
    bound_decode_cb.Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  // No need to flush since we retrieve all available frames after a packet is
  // provided.
  if (buffer->end_of_stream()) {
    DCHECK_EQ(state_, DecoderState::kNormal);
    state_ = DecoderState::kDecodeFinished;
    bound_decode_cb.Run(DecodeStatus::OK);
    return;
  }

  if (!DecodeBuffer(buffer.get())) {
    state_ = DecoderState::kError;
    bound_decode_cb.Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  // VideoDecoderShim expects |decode_cb| call after |output_cb_|.
  bound_decode_cb.Run(DecodeStatus::OK);
}

void AomVideoDecoder::Reset(const base::Closure& reset_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  state_ = DecoderState::kNormal;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, reset_cb);
}

void AomVideoDecoder::CloseDecoder() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!aom_decoder_)
    return;
  aom_codec_destroy(aom_decoder_.get());
  aom_decoder_.reset();
}

bool AomVideoDecoder::DecodeBuffer(const DecoderBuffer* buffer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!buffer->end_of_stream());

  if (aom_codec_decode(
          aom_decoder_.get(), buffer->data(), buffer->data_size(),
          reinterpret_cast<void*>(buffer->timestamp().InMicroseconds())) !=
      AOM_CODEC_OK) {
    const char* detail = aom_codec_error_detail(aom_decoder_.get());
    MEDIA_LOG(ERROR, media_log_)
        << "aom_codec_decode() failed: " << aom_codec_error(aom_decoder_.get())
        << (detail ? ", " : "") << (detail ? detail : "")
        << ", input: " << buffer->AsHumanReadableString();
    return false;
  }

  aom_codec_iter_t iter = nullptr;
  while (aom_image_t* img = aom_codec_get_frame(aom_decoder_.get(), &iter)) {
    auto frame = CopyImageToVideoFrame(img);
    if (!frame) {
      MEDIA_LOG(DEBUG, media_log_)
          << "Failed to produce video frame from aom_image_t.";
      return false;
    }

    frame->set_timestamp(base::TimeDelta::FromMicroseconds(
        reinterpret_cast<int64_t>(img->user_priv)));

    // TODO(dalecurtis): Is this true even for low resolutions?
    frame->metadata()->SetBoolean(VideoFrameMetadata::POWER_EFFICIENT, false);

    SetColorSpaceForFrame(img, config_, frame.get());
    output_cb_.Run(std::move(frame));
  }

  return true;
}

scoped_refptr<VideoFrame> AomVideoDecoder::CopyImageToVideoFrame(
    const struct aom_image* img) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // TODO(dalecurtis): This is silly, but if we want to get to zero copy we'll
  // need to add support for 8-bit within a 16-bit container.
  // https://bugs.chromium.org/p/aomedia/issues/detail?id=999
  bool needs_packing = false;
  VideoPixelFormat pixel_format;
  if (img->fmt == AOM_IMG_FMT_I42016 && img->bit_depth == 8) {
    needs_packing = true;
    pixel_format = PIXEL_FORMAT_I420;
  } else {
    pixel_format = AomImgFmtToVideoPixelFormat(img);
    if (pixel_format == PIXEL_FORMAT_UNKNOWN)
      return nullptr;
  }

  // Since we're making a copy, only copy the visible area.
  const gfx::Rect visible_rect(img->d_w, img->d_h);
  auto frame = frame_pool_.CreateFrame(
      pixel_format, visible_rect.size(), visible_rect,
      GetNaturalSize(visible_rect, config_.GetPixelAspectRatio()),
      kNoTimestamp);
  if (!frame)
    return nullptr;

  if (needs_packing) {
    // Condense 16-bit values into 8-bit.
    DCHECK_EQ(pixel_format, PIXEL_FORMAT_I420);
    PackPlane(VideoFrame::kYPlane, img, frame.get());
    PackPlane(VideoFrame::kUPlane, img, frame.get());
    PackPlane(VideoFrame::kVPlane, img, frame.get());
  } else {
    // Despite having I420 in the name this will copy any YUV format.
    libyuv::I420Copy(img->planes[AOM_PLANE_Y], img->stride[AOM_PLANE_Y],
                     img->planes[AOM_PLANE_U], img->stride[AOM_PLANE_U],
                     img->planes[AOM_PLANE_V], img->stride[AOM_PLANE_V],
                     frame->visible_data(VideoFrame::kYPlane),
                     frame->stride(VideoFrame::kYPlane),
                     frame->visible_data(VideoFrame::kUPlane),
                     frame->stride(VideoFrame::kUPlane),
                     frame->visible_data(VideoFrame::kVPlane),
                     frame->stride(VideoFrame::kVPlane), visible_rect.width(),
                     visible_rect.height());
  }

  return frame;
}

}  // namespace media
