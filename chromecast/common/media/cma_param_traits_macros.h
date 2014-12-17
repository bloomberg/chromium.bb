// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Singly or Multiply-included shared traits file depending on circumstances.
// This allows the use of IPC serialization macros in more than one IPC message
// file.
#ifndef CHROMECAST_COMMON_MEDIA_CMA_PARAM_TRAITS_MACROS_H_
#define CHROMECAST_COMMON_MEDIA_CMA_PARAM_TRAITS_MACROS_H_

#include "chromecast/media/cma/pipeline/load_type.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/param_traits_macros.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/buffering_state.h"
#include "media/base/channel_layout.h"
#include "media/base/pipeline_status.h"
#include "media/base/sample_format.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"

IPC_ENUM_TRAITS_MIN_MAX_VALUE(chromecast::media::LoadType,
                              chromecast::media::kLoadTypeURL,
                              chromecast::media::kLoadTypeMediaStream)

// TODO (gunsch): move media enum IPC code to a common location in media.
// See crbug.com/442594 for details.
IPC_ENUM_TRAITS_MIN_MAX_VALUE(media::AudioCodec,
                              media::AudioCodec::kUnknownAudioCodec,
                              media::AudioCodec::kAudioCodecMax)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(media::BufferingState,
                              media::BUFFERING_HAVE_NOTHING,
                              media::BUFFERING_HAVE_ENOUGH)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(media::SampleFormat,
                              media::SampleFormat::kUnknownSampleFormat,
                              media::SampleFormat::kSampleFormatMax)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(media::ChannelLayout,
                              media::ChannelLayout::CHANNEL_LAYOUT_NONE,
                              media::ChannelLayout::CHANNEL_LAYOUT_MAX)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(media::PipelineStatus,
                              media::PIPELINE_OK,
                              media::PIPELINE_STATUS_MAX)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(media::VideoCodec,
                              media::VideoCodec::kUnknownVideoCodec,
                              media::VideoCodec::kVideoCodecMax)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(media::VideoCodecProfile,
                              media::VIDEO_CODEC_PROFILE_MIN,
                              media::VIDEO_CODEC_PROFILE_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(media::VideoFrame::Format,
                          media::VideoFrame::FORMAT_MAX)

IPC_STRUCT_TRAITS_BEGIN(media::PipelineStatistics)
  IPC_STRUCT_TRAITS_MEMBER(audio_bytes_decoded)
  IPC_STRUCT_TRAITS_MEMBER(video_bytes_decoded)
  IPC_STRUCT_TRAITS_MEMBER(video_frames_decoded)
  IPC_STRUCT_TRAITS_MEMBER(video_frames_dropped)
IPC_STRUCT_TRAITS_END()

#endif  // CHROMECAST_COMMON_MEDIA_CMA_PARAM_TRAITS_MACROS_H_
