// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_DEMUXER_STREAM_PLAYER_PARAMS_H_
#define MEDIA_BASE_ANDROID_DEMUXER_STREAM_PLAYER_PARAMS_H_

#include <string>
#include <vector>

#include "media/base/audio_decoder_config.h"
#include "media/base/decrypt_config.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/video_decoder_config.h"
#include "ui/gfx/size.h"

namespace media {

struct MEDIA_EXPORT MediaPlayerHostMsg_DemuxerReady_Params {
  MediaPlayerHostMsg_DemuxerReady_Params();
  ~MediaPlayerHostMsg_DemuxerReady_Params();

  AudioCodec audio_codec;
  int audio_channels;
  int audio_sampling_rate;
  bool is_audio_encrypted;

  VideoCodec video_codec;
  gfx::Size video_size;
  bool is_video_encrypted;

  int duration_ms;
  std::string key_system;
};

struct MEDIA_EXPORT MediaPlayerHostMsg_ReadFromDemuxerAck_Params {
  struct MEDIA_EXPORT AccessUnit {
    AccessUnit();
    ~AccessUnit();

    DemuxerStream::Status status;
    bool end_of_stream;
    // TODO(ycheo): Use the shared memory to transfer the block data.
    std::vector<unsigned char> data;
    base::TimeDelta timestamp;
    std::vector<char> key_id;
    std::vector<char> iv;
    std::vector<media::SubsampleEntry> subsamples;
  };

  MediaPlayerHostMsg_ReadFromDemuxerAck_Params();
  ~MediaPlayerHostMsg_ReadFromDemuxerAck_Params();

  DemuxerStream::Type type;
  std::vector<AccessUnit> access_units;
};

};  // namespace media

#endif  // MEDIA_BASE_ANDROID_DEMUXER_STREAM_PLAYER_PARAMS_H_
