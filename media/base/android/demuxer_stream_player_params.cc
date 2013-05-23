// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/demuxer_stream_player_params.h"

namespace media {

MediaPlayerHostMsg_DemuxerReady_Params::
    MediaPlayerHostMsg_DemuxerReady_Params()
    : audio_channels(0),
      audio_sampling_rate(0),
      is_audio_encrypted(false),
      is_video_encrypted(false),
      duration_ms(0) {}

MediaPlayerHostMsg_DemuxerReady_Params::
    ~MediaPlayerHostMsg_DemuxerReady_Params() {}

MediaPlayerHostMsg_ReadFromDemuxerAck_Params::
    MediaPlayerHostMsg_ReadFromDemuxerAck_Params()
    : type(DemuxerStream::UNKNOWN) {}

MediaPlayerHostMsg_ReadFromDemuxerAck_Params::
    ~MediaPlayerHostMsg_ReadFromDemuxerAck_Params() {}

MediaPlayerHostMsg_ReadFromDemuxerAck_Params::AccessUnit::AccessUnit()
    : end_of_stream(false) {}

MediaPlayerHostMsg_ReadFromDemuxerAck_Params::AccessUnit::~AccessUnit() {}

}  // namespace media
