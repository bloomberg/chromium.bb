// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/demuxer_stream_player_params.h"

namespace media {

DemuxerConfigs::DemuxerConfigs()
    : audio_codec(kUnknownAudioCodec),
      audio_channels(0),
      audio_sampling_rate(0),
      is_audio_encrypted(false),
      audio_codec_delay_ns(-1),
      audio_seek_preroll_ns(-1),
      video_codec(kUnknownVideoCodec),
      is_video_encrypted(false) {}

DemuxerConfigs::~DemuxerConfigs() {}

AccessUnit::AccessUnit() : is_end_of_stream(false), is_key_frame(false) {}

AccessUnit::~AccessUnit() {}

DemuxerData::DemuxerData() : type(DemuxerStream::UNKNOWN) {}

DemuxerData::~DemuxerData() {}

}  // namespace media
