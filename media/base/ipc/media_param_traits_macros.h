// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_MACROS_H_
#define MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_MACROS_H_

#include "ipc/ipc_message_macros.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_parameters.h"
#include "media/base/buffering_state.h"
#include "media/base/channel_layout.h"
#include "media/base/decode_status.h"
#include "media/base/sample_format.h"
#include "media/base/subsample_entry.h"
#include "media/base/video_capture_types.h"
#include "media/base/video_codecs.h"
#include "media/base/video_types.h"

IPC_ENUM_TRAITS_MAX_VALUE(media::AudioCodec, media::AudioCodec::kAudioCodecMax)

IPC_ENUM_TRAITS_MAX_VALUE(media::AudioLatency::LatencyType,
                          media::AudioLatency::LATENCY_COUNT)

IPC_ENUM_TRAITS_MAX_VALUE(media::AudioParameters::Format,
                          media::AudioParameters::AUDIO_FORMAT_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(media::BufferingState,
                          media::BufferingState::BUFFERING_STATE_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::ChannelLayout, media::CHANNEL_LAYOUT_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::ColorSpace, media::COLOR_SPACE_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::DecodeStatus,
                          media::DecodeStatus::DECODE_STATUS_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::SampleFormat, media::kSampleFormatMax)

IPC_ENUM_TRAITS_MAX_VALUE(media::VideoCodec, media::kVideoCodecMax)

IPC_ENUM_TRAITS_MIN_MAX_VALUE(media::VideoCodecProfile,
                              media::VIDEO_CODEC_PROFILE_MIN,
                              media::VIDEO_CODEC_PROFILE_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::VideoPixelFormat, media::PIXEL_FORMAT_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::VideoPixelStorage, media::PIXEL_STORAGE_MAX)

IPC_STRUCT_TRAITS_BEGIN(media::SubsampleEntry)
  IPC_STRUCT_TRAITS_MEMBER(clear_bytes)
  IPC_STRUCT_TRAITS_MEMBER(cypher_bytes)
IPC_STRUCT_TRAITS_END()

#endif  // MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_MACROS_H_
