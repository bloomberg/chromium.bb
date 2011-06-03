// Copyright (c) 2011 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "media/omx/omx_configurator.h"

#include "base/logging.h"

namespace media {

static std::string GetCodecName(OmxConfigurator::Codec codec) {
  switch (codec) {
    case OmxConfigurator::kCodecH264:
      return "avc";
    case OmxConfigurator::kCodecH263:
      return "h263";
    case OmxConfigurator::kCodecMpeg4:
      return "mpeg4";
    case OmxConfigurator::kCodecVc1:
      return "vc1";
    default:
      break;
  }
  NOTREACHED();
  return "";
}

std::string OmxDecoderConfigurator::GetRoleName() const {
  return "video_decoder." + GetCodecName(input_format().codec);
}

bool OmxDecoderConfigurator::ConfigureIOPorts(
    OMX_COMPONENTTYPE* component,
    OMX_PARAM_PORTDEFINITIONTYPE* input_port_def,
    OMX_PARAM_PORTDEFINITIONTYPE* output_port_def) const {
  // Configure the input port.
  if (input_format().codec == kCodecNone) {
    LOG(ERROR) << "Unsupported codec " << input_format().codec;
    return false;
  }
  if (input_format().codec == kCodecH264)
    input_port_def->format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
  else if (input_format().codec == kCodecMpeg4)
    input_port_def->format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
  else if (input_format().codec == kCodecH263)
    input_port_def->format.video.eCompressionFormat = OMX_VIDEO_CodingH263;
  else if (input_format().codec == kCodecVc1)
    input_port_def->format.video.eCompressionFormat = OMX_VIDEO_CodingWMV;

  // Assumes 480P.
  input_port_def->format.video.nFrameWidth  = 720;
  input_port_def->format.video.nFrameHeight = 480;
  OMX_ERRORTYPE omxresult = OMX_ErrorNone;
  omxresult = OMX_SetParameter(component,
                               OMX_IndexParamPortDefinition,
                               input_port_def);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "SetParameter(OMX_IndexParamPortDefinition) "
                  "for input port failed";
    return false;
  }
  return true;
}

std::string OmxEncoderConfigurator::GetRoleName() const {
  return "video_encoder." + GetCodecName(output_format().codec);
}

bool OmxEncoderConfigurator::ConfigureIOPorts(
    OMX_COMPONENTTYPE* component,
    OMX_PARAM_PORTDEFINITIONTYPE* input_port_def,
    OMX_PARAM_PORTDEFINITIONTYPE* output_port_def) const {
  // TODO(jiesun): Add support for other format than MPEG4.
  DCHECK_EQ(kCodecMpeg4, output_format().codec);
  // Configure the input port.
  input_port_def->format.video.nFrameWidth =
      input_format().video_header.width;
  input_port_def->format.video.nFrameHeight =
      input_format().video_header.height;
  OMX_ERRORTYPE omxresult = OMX_ErrorNone;
  omxresult = OMX_SetParameter(component,
                               OMX_IndexParamPortDefinition,
                               input_port_def);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "SetParameter(OMX_IndexParamPortDefinition) "
                  "for input port failed";
    return false;
  }

  // Configure the output port.
  output_port_def->format.video.nFrameWidth =
      input_format().video_header.width;
  output_port_def->format.video.nFrameHeight =
      input_format().video_header.height;
  omxresult = OMX_SetParameter(component,
                               OMX_IndexParamPortDefinition,
                               output_port_def);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "SetParameter(OMX_IndexParamPortDefinition) "
                  "for output port failed";
    return false;
  }

  if (output_format().codec == kCodecMpeg4) {
    OMX_VIDEO_PARAM_MPEG4TYPE mp4_type;
    omxresult = OMX_GetParameter(component,
                                 OMX_IndexParamVideoMpeg4,
                                 &mp4_type);
    if (omxresult != OMX_ErrorNone) {
      LOG(ERROR) << "GetParameter(OMX_IndexParamVideoMpeg4) failed";
      return false;
    }
    // TODO(jiesun): verify if other vendors had the same definition.
    // Specify the frame rate.
    mp4_type.nTimeIncRes = output_format().video_header.frame_rate * 2;
    // Specify how many P frames between adjacent intra frames.
    mp4_type.nPFrames = output_format().video_header.i_dist - 1;
    omxresult = OMX_SetParameter(component,
                                 OMX_IndexParamVideoMpeg4,
                                 &mp4_type);
    if (omxresult != OMX_ErrorNone) {
      LOG(ERROR) << "SetParameter(OMX_IndexParamVideoMpeg4) failed";
      return false;
    }
  }

  OMX_VIDEO_PARAM_BITRATETYPE bitrate;
  omxresult = OMX_GetParameter(component,
                               OMX_IndexParamVideoBitrate,
                               &bitrate);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "GetParameter(OMX_IndexParamVideoBitrate) failed";
    return false;
  }

  // TODO(jiesun): expose other rate control method that matters.
  bitrate.eControlRate = OMX_Video_ControlRateConstant;
  bitrate.nTargetBitrate = output_format().video_header.bit_rate;
  omxresult = OMX_SetParameter(component,
                               OMX_IndexParamVideoBitrate,
                               &bitrate);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "SetParameter(OMX_IndexParamVideoBitrate) failed";
    return false;
  }

  OMX_CONFIG_FRAMERATETYPE framerate;
  omxresult = OMX_GetConfig(component,
                            OMX_IndexConfigVideoFramerate,
                            &framerate);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "GetParameter(OMX_IndexConfigVideoFramerate) failed";
    return false;
  }

  framerate.xEncodeFramerate =
      output_format().video_header.frame_rate << 16;  // Q16 format.
  omxresult = OMX_SetConfig(component,
                            OMX_IndexConfigVideoFramerate,
                            &framerate);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "SetParameter(OMX_IndexConfigVideoFramerate) failed";
    return false;
  }
  return true;
}

}  // namespace media
