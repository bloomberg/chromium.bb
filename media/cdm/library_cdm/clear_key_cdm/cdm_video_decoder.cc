// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/library_cdm/clear_key_cdm/cdm_video_decoder.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/no_destructor.h"
#include "base/optional.h"
// Necessary to convert async media::VideoDecoder to sync CdmVideoDecoder.
// Typically not recommended for production code, but is ok here since
// ClearKeyCdm is only for testing.
#include "base/run_loop.h"
#include "media/base/decode_status.h"
#include "media/base/media_util.h"
#include "media/cdm/library_cdm/cdm_host_proxy.h"
#include "media/media_buildflags.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"

#if BUILDFLAG(ENABLE_FFMPEG)
#include "media/filters/ffmpeg_video_decoder.h"
#endif

#if BUILDFLAG(ENABLE_LIBVPX)
#include "media/filters/vpx_video_decoder.h"
#endif

namespace media {

namespace {

VideoCodec ToMediaVideoCodec(cdm::VideoCodec codec) {
  switch (codec) {
    case cdm::kUnknownVideoCodec:
      return kUnknownVideoCodec;
    case cdm::kCodecVp8:
      return kCodecVP8;
    case cdm::kCodecH264:
      return kCodecH264;
    case cdm::kCodecVp9:
      return kCodecVP9;
    case cdm::kCodecAv1:
      return kCodecAV1;
  }
}

VideoCodecProfile ToMediaVideoCodecProfile(cdm::VideoCodecProfile profile) {
  switch (profile) {
    case cdm::kUnknownVideoCodecProfile:
      return VIDEO_CODEC_PROFILE_UNKNOWN;
    case cdm::kProfileNotNeeded:
      // There's no corresponding value for "not needed". Given CdmAdapter only
      // converts VP8PROFILE_ANY to cdm::kProfileNotNeeded, and this code is
      // only used for testing, it's okay to convert it back to VP8PROFILE_ANY.
      return VP8PROFILE_ANY;
    case cdm::kVP9Profile0:
      return VP9PROFILE_PROFILE0;
    case cdm::kVP9Profile1:
      return VP9PROFILE_PROFILE1;
    case cdm::kVP9Profile2:
      return VP9PROFILE_PROFILE2;
    case cdm::kVP9Profile3:
      return VP9PROFILE_PROFILE3;
    case cdm::kH264ProfileBaseline:
      return H264PROFILE_BASELINE;
    case cdm::kH264ProfileMain:
      return H264PROFILE_MAIN;
    case cdm::kH264ProfileExtended:
      return H264PROFILE_EXTENDED;
    case cdm::kH264ProfileHigh:
      return H264PROFILE_HIGH;
    case cdm::kH264ProfileHigh10:
      return H264PROFILE_HIGH10PROFILE;
    case cdm::kH264ProfileHigh422:
      return H264PROFILE_HIGH422PROFILE;
    case cdm::kH264ProfileHigh444Predictive:
      return H264PROFILE_HIGH444PREDICTIVEPROFILE;
    case cdm::kAv1ProfileMain:
      return AV1PROFILE_PROFILE_MAIN;
    case cdm::kAv1ProfileHigh:
      return AV1PROFILE_PROFILE_HIGH;
    case cdm::kAv1ProfilePro:
      return AV1PROFILE_PROFILE_PRO;
  }
}

// TODO(xhwang): Support all media video formats.
VideoPixelFormat ToMediaVideoFormat(cdm::VideoFormat format) {
  switch (format) {
    case cdm::kYv12:
      return PIXEL_FORMAT_YV12;
    case cdm::kI420:
      return PIXEL_FORMAT_I420;
    default:
      DVLOG(1) << "Unsupported VideoFormat " << format;
      return PIXEL_FORMAT_UNKNOWN;
  }
}

gfx::ColorSpace::RangeID ToGfxColorRange(cdm::ColorRange range) {
  switch (range) {
    case cdm::ColorRange::kInvalid:
      return gfx::ColorSpace::RangeID::INVALID;
    case cdm::ColorRange::kLimited:
      return gfx::ColorSpace::RangeID::LIMITED;
    case cdm::ColorRange::kFull:
      return gfx::ColorSpace::RangeID::FULL;
    case cdm::ColorRange::kDerived:
      return gfx::ColorSpace::RangeID::DERIVED;
  }
}

VideoColorSpace ToMediaColorSpace(const cdm::ColorSpace& color_space) {
  return VideoColorSpace(color_space.primary_id, color_space.transfer_id,
                         color_space.matrix_id,
                         ToGfxColorRange(color_space.range));
}

media::VideoDecoderConfig ToClearMediaVideoDecoderConfig(
    const cdm::VideoDecoderConfig_3& config) {
  gfx::Size coded_size(config.coded_size.width, config.coded_size.width);

  VideoDecoderConfig media_config(
      ToMediaVideoCodec(config.codec), ToMediaVideoCodecProfile(config.profile),
      ToMediaVideoFormat(config.format), COLOR_SPACE_UNSPECIFIED,
      VideoRotation::VIDEO_ROTATION_0, coded_size, gfx::Rect(coded_size),
      coded_size,
      std::vector<uint8_t>(config.extra_data,
                           config.extra_data + config.extra_data_size),
      Unencrypted());

  media_config.set_color_space_info(ToMediaColorSpace(config.color_space));

  return media_config;
}

bool ToCdmVideoFrame(const VideoFrame& video_frame,
                     CdmHostProxy* cdm_host_proxy,
                     CdmVideoDecoder::CdmVideoFrame* cdm_video_frame) {
  DCHECK(cdm_video_frame);

  // TODO(xhwang): Support more pixel formats.
  if (!video_frame.IsMappable() ||
      video_frame.format() != media::PIXEL_FORMAT_I420) {
    DVLOG(1) << "VideoFrame is not mappable or has unsupported format";
    return false;
  }

  const int width = video_frame.coded_size().width();
  const int height = video_frame.coded_size().height();
  DCHECK_EQ(width % 2, 0);
  DCHECK_EQ(height % 2, 0);
  const int width_uv = width / 2;
  const int height_uv = height / 2;
  const int dst_stride_y = width;
  const int dst_stride_uv = width_uv;
  const int y_size = dst_stride_y * height;
  const int uv_size = dst_stride_uv * height_uv;
  const int space_required = y_size + (uv_size * 2);

  auto* buffer = cdm_host_proxy->Allocate(space_required);
  if (!buffer) {
    LOG(ERROR) << __func__ << ": Buffer allocation failed.";
    return false;
  }

  // Values from the source |video_frame|.
  const auto* src_y = video_frame.data(VideoFrame::kYPlane);
  const auto* src_u = video_frame.data(VideoFrame::kUPlane);
  const auto* src_v = video_frame.data(VideoFrame::kVPlane);
  auto src_stride_y = video_frame.stride(VideoFrame::kYPlane);
  auto src_stride_u = video_frame.stride(VideoFrame::kUPlane);
  auto src_stride_v = video_frame.stride(VideoFrame::kVPlane);

  // Values for the destinaton frame buffer.
  uint8_t* dst_y = buffer->Data();
  uint8_t* dst_u = dst_y + y_size;
  uint8_t* dst_v = dst_u + uv_size;

  // Actually copy video frame planes.
  using libyuv::CopyPlane;
  CopyPlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height);
  CopyPlane(src_u, src_stride_u, dst_u, dst_stride_uv, width_uv, height_uv);
  CopyPlane(src_v, src_stride_v, dst_v, dst_stride_uv, width_uv, height_uv);

  buffer->SetSize(space_required);
  cdm_video_frame->SetFrameBuffer(buffer);

  cdm_video_frame->SetFormat(cdm::kI420);
  cdm_video_frame->SetSize({width, height});
  cdm_video_frame->SetPlaneOffset(cdm::kYPlane, 0);
  cdm_video_frame->SetPlaneOffset(cdm::kUPlane, y_size);
  cdm_video_frame->SetPlaneOffset(cdm::kVPlane, y_size + uv_size);
  cdm_video_frame->SetStride(cdm::kYPlane, dst_stride_y);
  cdm_video_frame->SetStride(cdm::kUPlane, dst_stride_uv);
  cdm_video_frame->SetStride(cdm::kVPlane, dst_stride_uv);
  cdm_video_frame->SetTimestamp(video_frame.timestamp().InMicroseconds());

  return true;
}

// Media VideoDecoders typically assumes a global environment where a lot of
// things are already setup in the process, e.g. base::ThreadTaskRunnerHandle
// and base::CommandLine. These will be available in the component build because
// the CDM and the host is depending on the same base/ target. In static build,
// they will not be available and we have to setup it by ourselves.
void SetupGlobalEnvironmentIfNeeded() {
  // Creating a base::MessageLoop to setup base::ThreadTaskRunnerHandle.
  if (!base::MessageLoop::current()) {
    static base::NoDestructor<base::MessageLoop> message_loop;
  }

  if (!base::CommandLine::InitializedForCurrentProcess())
    base::CommandLine::Init(0, nullptr);
}

// Adapts a media::VideoDecoder to a CdmVideoDecoder. Media VideoDecoders
// operations are asynchronous, often posting callbacks to the task runner. The
// CdmVideoDecoder operations are synchronous. Therefore, after calling
// media::VideoDecoder, we need to run a RunLoop manually and wait for the
// asynchronous operation to finish. The RunLoop must be of type
// |kNestableTasksAllowed| because we could be running the RunLoop in a task,
// e.g. in component builds when we share the same task runner as the host. In
// a static build, this is not necessary.
class VideoDecoderAdapter : public CdmVideoDecoder {
 public:
  VideoDecoderAdapter(CdmHostProxy* cdm_host_proxy,
                      std::unique_ptr<VideoDecoder> video_decoder)
      : cdm_host_proxy_(cdm_host_proxy),
        video_decoder_(std::move(video_decoder)),
        weak_factory_(this) {
    DCHECK(cdm_host_proxy_);
  }

  ~VideoDecoderAdapter() final = default;

  // CdmVideoDecoder implementation.
  bool Initialize(const cdm::VideoDecoderConfig_3& config) final {
    auto clear_config = ToClearMediaVideoDecoderConfig(config);
    DVLOG(1) << __func__ << ": " << clear_config.AsHumanReadableString();
    DCHECK(!last_init_result_.has_value());

    // Initialize |video_decoder_| and wait for completion.
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    video_decoder_->Initialize(
        clear_config,
        /* low_delay = */ false,
        /* cdm_context = */ nullptr,
        base::BindRepeating(&VideoDecoderAdapter::OnInitialized,
                            weak_factory_.GetWeakPtr(), run_loop.QuitClosure()),
        base::BindRepeating(&VideoDecoderAdapter::OnVideoFrameReady,
                            weak_factory_.GetWeakPtr()),
        /* waiting_for_decryption_key_cb = */ base::DoNothing());
    run_loop.Run();

    auto result = last_init_result_.value();
    last_init_result_.reset();

    return result;
  }

  void Deinitialize() final {
    // Do nothing since |video_decoder_| supports reinitialization without
    // the need to deinitialize first.
  }

  void Reset() final {
    DVLOG(2) << __func__;

    // Reset |video_decoder_| and wait for completion.
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    video_decoder_->Reset(base::BindRepeating(&VideoDecoderAdapter::OnReset,
                                              weak_factory_.GetWeakPtr(),
                                              run_loop.QuitClosure()));
    run_loop.Run();
  }

  cdm::Status Decode(scoped_refptr<DecoderBuffer> buffer,
                     CdmVideoFrame* decoded_frame) final {
    DVLOG(3) << __func__;
    DCHECK(!last_decode_status_.has_value());

    // Call |video_decoder_| Decode() and wait for completion.
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    video_decoder_->Decode(std::move(buffer),
                           base::BindRepeating(&VideoDecoderAdapter::OnDecoded,
                                               weak_factory_.GetWeakPtr(),
                                               run_loop.QuitClosure()));
    run_loop.Run();

    auto decode_status = last_decode_status_.value();
    last_decode_status_.reset();

    if (decode_status == DecodeStatus::DECODE_ERROR)
      return cdm::kDecodeError;

    // "ABORTED" shouldn't happen during a sync decode, so treat it as an error.
    DCHECK_EQ(decode_status, DecodeStatus::OK);

    if (decoded_video_frames_.empty())
      return cdm::kNeedMoreData;

    auto video_frame = decoded_video_frames_.front();
    decoded_video_frames_.pop();

    return ToCdmVideoFrame(*video_frame, cdm_host_proxy_, decoded_frame)
               ? cdm::kSuccess
               : cdm::kDecodeError;
  }

 private:
  void OnInitialized(base::OnceClosure quit_closure, bool success) {
    DVLOG(1) << __func__ << " success = " << success;
    DCHECK(!last_init_result_.has_value());
    last_init_result_ = success;
    std::move(quit_closure).Run();
  }

  void OnVideoFrameReady(const scoped_refptr<VideoFrame>& video_frame) {
    // Do not queue EOS frames, which is not needed.
    if (video_frame->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM))
      return;

    decoded_video_frames_.push(video_frame);
  }

  void OnReset(base::OnceClosure quit_closure) {
    VideoFrameQueue empty_queue;
    std::swap(decoded_video_frames_, empty_queue);
    std::move(quit_closure).Run();
  }

  void OnDecoded(base::OnceClosure quit_closure, DecodeStatus decode_status) {
    DCHECK(!last_decode_status_.has_value());
    last_decode_status_ = decode_status;
    std::move(quit_closure).Run();
  }

  CdmHostProxy* const cdm_host_proxy_;
  std::unique_ptr<VideoDecoder> video_decoder_;

  // Results of |video_decoder_| operations. Set iff the callback of the
  // operation has been called.
  base::Optional<bool> last_init_result_;
  base::Optional<DecodeStatus> last_decode_status_;

  // Queue of decoded video frames.
  using VideoFrameQueue = base::queue<scoped_refptr<VideoFrame>>;
  VideoFrameQueue decoded_video_frames_;

  base::WeakPtrFactory<VideoDecoderAdapter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecoderAdapter);
};

}  // namespace

std::unique_ptr<CdmVideoDecoder> CreateVideoDecoder(
    CdmHostProxy* cdm_host_proxy,
    const cdm::VideoDecoderConfig_3& config) {
  SetupGlobalEnvironmentIfNeeded();

  static base::NoDestructor<media::NullMediaLog> null_media_log;
  std::unique_ptr<VideoDecoder> video_decoder;

#if BUILDFLAG(ENABLE_LIBVPX)
  if (config.codec == cdm::kCodecVp8 || config.codec == cdm::kCodecVp9)
    video_decoder.reset(new VpxVideoDecoder());
#endif

#if BUILDFLAG(ENABLE_FFMPEG)
  if (!video_decoder)
    video_decoder.reset(new FFmpegVideoDecoder(null_media_log.get()));
#endif

  if (!video_decoder)
    return nullptr;

  return std::make_unique<VideoDecoderAdapter>(cdm_host_proxy,
                                               std::move(video_decoder));
}

}  // namespace media
