// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/test_data_factory.h"

#include "base/strings/stringprintf.h"
#include "media/base/android/demuxer_stream_player_params.h"
#include "media/base/decoder_buffer.h"
#include "media/base/test_data_util.h"

namespace media {

DemuxerConfigs TestDataFactory::CreateAudioConfigs(
    AudioCodec audio_codec,
    const base::TimeDelta& duration) {
  DemuxerConfigs configs;
  configs.audio_codec = audio_codec;
  configs.audio_channels = 2;
  configs.is_audio_encrypted = false;
  configs.duration = duration;

  switch (audio_codec) {
    case kCodecVorbis: {
      configs.audio_sampling_rate = 44100;
      scoped_refptr<DecoderBuffer> buffer =
          ReadTestDataFile("vorbis-extradata");
      configs.audio_extra_data = std::vector<uint8>(
          buffer->data(), buffer->data() + buffer->data_size());
    } break;

    case kCodecAAC: {
      configs.audio_sampling_rate = 48000;
      uint8 aac_extra_data[] = {0x13, 0x10};
      configs.audio_extra_data =
          std::vector<uint8>(aac_extra_data, aac_extra_data + 2);
    } break;

    default:
      // Other codecs are not supported by this helper.
      NOTREACHED();
      break;
  }

  return configs;
}

DemuxerConfigs TestDataFactory::CreateVideoConfigs(
    VideoCodec video_codec,
    const base::TimeDelta& duration,
    const gfx::Size& video_size) {
  DemuxerConfigs configs;
  configs.video_codec = video_codec;
  configs.video_size = video_size;
  configs.is_video_encrypted = false;
  configs.duration = duration;

  return configs;
}

TestDataFactory::TestDataFactory(const char* file_name_template,
                                 const base::TimeDelta& duration,
                                 const base::TimeDelta& frame_period)
    : duration_(duration), frame_period_(frame_period) {
  LoadPackets(file_name_template);
}

TestDataFactory::~TestDataFactory() {}

void TestDataFactory::CreateChunk(DemuxerData* chunk, base::TimeDelta* delay) {
  DCHECK(chunk);
  DCHECK(delay);

  *delay = base::TimeDelta();

  for (int i = 0; i < 4; ++i) {
    chunk->access_units.push_back(AccessUnit());
    AccessUnit& unit = chunk->access_units.back();
    unit.status = DemuxerStream::kOk;

    unit.timestamp = regular_pts_;
    regular_pts_ += frame_period_;

    if (unit.timestamp > duration_) {
      unit.is_end_of_stream = true;
      break;  // EOS units have no data.
    }

    unit.data = packet_[i];

    // Allow for modification by subclasses.
    ModifyAccessUnit(i, &unit);

    // Maintain last PTS. FillAccessUnit can modify unit's PTS.
    if (last_pts_ < unit.timestamp)
      last_pts_ = unit.timestamp;
  }
}

void TestDataFactory::LoadPackets(const char* file_name_template) {
  for (int i = 0; i < 4; ++i) {
    scoped_refptr<DecoderBuffer> buffer =
        ReadTestDataFile(base::StringPrintf(file_name_template, i));
    packet_[i] = std::vector<uint8>(buffer->data(),
                                    buffer->data() + buffer->data_size());
  }
}

}  // namespace media
