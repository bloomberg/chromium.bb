// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media_recorder/h264_encoder.h"

#include <string>

#include "base/bind.h"
#include "base/threading/thread.h"
#include "base/trace_event/trace_event.h"
#include "media/base/video_frame.h"
#include "third_party/openh264/src/codec/api/svc/codec_app_def.h"
#include "third_party/openh264/src/codec/api/svc/codec_def.h"
#include "ui/gfx/geometry/size.h"

using media::VideoFrame;

namespace content {

void H264Encoder::ISVCEncoderDeleter::operator()(ISVCEncoder* codec) {
  if (!codec)
    return;
  const int uninit_ret = codec->Uninitialize();
  CHECK_EQ(cmResultSuccess, uninit_ret);
  WelsDestroySVCEncoder(codec);
}

// static
void H264Encoder::ShutdownEncoder(std::unique_ptr<base::Thread> encoding_thread,
                                  ScopedISVCEncoderPtr encoder) {
  DCHECK(encoding_thread->IsRunning());
  encoding_thread->Stop();
  // Both |encoding_thread| and |encoder| will be destroyed at end-of-scope.
}

H264Encoder::H264Encoder(
    const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_callback,
    int32_t bits_per_second)
    : Encoder(on_encoded_video_callback, bits_per_second) {
  DCHECK(encoding_thread_->IsRunning());
}

H264Encoder::~H264Encoder() {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&H264Encoder::ShutdownEncoder, base::Passed(&encoding_thread_),
                 base::Passed(&openh264_encoder_)));
}

void H264Encoder::EncodeOnEncodingTaskRunner(
    scoped_refptr<VideoFrame> frame,
    base::TimeTicks capture_timestamp) {
  TRACE_EVENT0("video", "H264Encoder::EncodeOnEncodingTaskRunner");
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());

  const gfx::Size frame_size = frame->visible_rect().size();
  if (!openh264_encoder_ || configured_size_ != frame_size) {
    ConfigureEncoderOnEncodingTaskRunner(frame_size);
    first_frame_timestamp_ = capture_timestamp;
  }

  SSourcePicture picture = {};
  picture.iPicWidth = frame_size.width();
  picture.iPicHeight = frame_size.height();
  picture.iColorFormat = EVideoFormatType::videoFormatI420;
  picture.uiTimeStamp =
      (capture_timestamp - first_frame_timestamp_).InMilliseconds();
  picture.iStride[0] = frame->stride(VideoFrame::kYPlane);
  picture.iStride[1] = frame->stride(VideoFrame::kUPlane);
  picture.iStride[2] = frame->stride(VideoFrame::kVPlane);
  picture.pData[0] = frame->visible_data(VideoFrame::kYPlane);
  picture.pData[1] = frame->visible_data(VideoFrame::kUPlane);
  picture.pData[2] = frame->visible_data(VideoFrame::kVPlane);

  SFrameBSInfo info = {};
  if (openh264_encoder_->EncodeFrame(&picture, &info) != cmResultSuccess) {
    NOTREACHED() << "OpenH264 encoding failed";
    return;
  }
  const media::WebmMuxer::VideoParameters video_params(frame);
  frame = nullptr;

  std::unique_ptr<std::string> data(new std::string);
  const uint8_t kNALStartCode[4] = {0, 0, 0, 1};
  for (int layer = 0; layer < info.iLayerNum; ++layer) {
    const SLayerBSInfo& layerInfo = info.sLayerInfo[layer];
    // Iterate NAL units making up this layer, noting fragments.
    size_t layer_len = 0;
    for (int nal = 0; nal < layerInfo.iNalCount; ++nal) {
      // The following DCHECKs make sure that the header of each NAL unit is OK.
      DCHECK_GE(layerInfo.pNalLengthInByte[nal], 4);
      DCHECK_EQ(kNALStartCode[0], layerInfo.pBsBuf[layer_len + 0]);
      DCHECK_EQ(kNALStartCode[1], layerInfo.pBsBuf[layer_len + 1]);
      DCHECK_EQ(kNALStartCode[2], layerInfo.pBsBuf[layer_len + 2]);
      DCHECK_EQ(kNALStartCode[3], layerInfo.pBsBuf[layer_len + 3]);

      layer_len += layerInfo.pNalLengthInByte[nal];
    }
    // Copy the entire layer's data (including NAL start codes).
    data->append(reinterpret_cast<char*>(layerInfo.pBsBuf), layer_len);
  }

  const bool is_key_frame = info.eFrameType == videoFrameTypeIDR;
  origin_task_runner_->PostTask(
      FROM_HERE, base::Bind(OnFrameEncodeCompleted, on_encoded_video_callback_,
                            video_params, base::Passed(&data), nullptr,
                            capture_timestamp, is_key_frame));
}

void H264Encoder::ConfigureEncoderOnEncodingTaskRunner(const gfx::Size& size) {
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());
  ISVCEncoder* temp_encoder = nullptr;
  if (WelsCreateSVCEncoder(&temp_encoder) != 0) {
    NOTREACHED() << "Failed to create OpenH264 encoder";
    return;
  }
  openh264_encoder_.reset(temp_encoder);
  configured_size_ = size;

#if DCHECK_IS_ON()
  int trace_level = WELS_LOG_INFO;
  openh264_encoder_->SetOption(ENCODER_OPTION_TRACE_LEVEL, &trace_level);
#endif

  SEncParamExt init_params;
  openh264_encoder_->GetDefaultParams(&init_params);
  init_params.iUsageType = CAMERA_VIDEO_REAL_TIME;

  DCHECK_EQ(AUTO_REF_PIC_COUNT, init_params.iNumRefFrame);
  DCHECK(!init_params.bSimulcastAVC);

  init_params.uiIntraPeriod = 100;  // Same as for VpxEncoder.
  init_params.iPicWidth = size.width();
  init_params.iPicHeight = size.height();

  DCHECK_EQ(RC_QUALITY_MODE, init_params.iRCMode);
  DCHECK_EQ(0, init_params.iPaddingFlag);
  DCHECK_EQ(UNSPECIFIED_BIT_RATE, init_params.iTargetBitrate);
  DCHECK_EQ(UNSPECIFIED_BIT_RATE, init_params.iMaxBitrate);
  if (bits_per_second_ > 0) {
    init_params.iRCMode = RC_BITRATE_MODE;
    init_params.iTargetBitrate = bits_per_second_;
  } else {
    init_params.iRCMode = RC_OFF_MODE;
  }

  // Threading model: Set to 1 due to https://crbug.com/583348.
  init_params.iMultipleThreadIdc = 1;

  // TODO(mcasas): consider reducing complexity if there are few CPUs available.
  init_params.iComplexityMode = MEDIUM_COMPLEXITY;
  DCHECK(!init_params.bEnableDenoise);
  DCHECK(init_params.bEnableFrameSkip);

  // The base spatial layer 0 is the only one we use.
  DCHECK_EQ(1, init_params.iSpatialLayerNum);
  init_params.sSpatialLayers[0].iVideoWidth = init_params.iPicWidth;
  init_params.sSpatialLayers[0].iVideoHeight = init_params.iPicHeight;
  init_params.sSpatialLayers[0].iSpatialBitrate = init_params.iTargetBitrate;

  // When uiSliceMode = SM_FIXEDSLCNUM_SLICE, uiSliceNum = 0 means auto design
  // it with cpu core number.
  // TODO(sprang): Set to 0 when we understand why the rate controller borks
  // when uiSliceNum > 1. See https://github.com/cisco/openh264/issues/2591
  init_params.sSpatialLayers[0].sSliceArgument.uiSliceNum = 1;
  init_params.sSpatialLayers[0].sSliceArgument.uiSliceMode =
      SM_FIXEDSLCNUM_SLICE;

  if (openh264_encoder_->InitializeExt(&init_params) != cmResultSuccess) {
    NOTREACHED() << "Failed to initialize OpenH264 encoder";
    return;
  }

  int pixel_format = EVideoFormatType::videoFormatI420;
  openh264_encoder_->SetOption(ENCODER_OPTION_DATAFORMAT, &pixel_format);
}

}  // namespace content
