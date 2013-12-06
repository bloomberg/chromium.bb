// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mp2t/es_parser_adts.h"

#include <list>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/bit_reader.h"
#include "media/base/buffers.h"
#include "media/base/channel_layout.h"
#include "media/base/stream_parser_buffer.h"
#include "media/mp2t/mp2t_common.h"

// Adts header is at least 7 bytes (can be 9 bytes).
static const int kAdtsHeaderMinSize = 7;

static const int adts_frequency_table[16] = {
  96000,
  88200,
  64000,
  48000,
  44100,
  32000,
  24000,
  22050,
  16000,
  12000,
  11025,
  8000,
  7350,
  0,
  0,
  0,
};
static const int kMaxSupportedFrequencyIndex = 12;

static media::ChannelLayout adts_channel_layout[8] = {
  media::CHANNEL_LAYOUT_NONE,
  media::CHANNEL_LAYOUT_MONO,
  media::CHANNEL_LAYOUT_STEREO,
  media::CHANNEL_LAYOUT_SURROUND,
  media::CHANNEL_LAYOUT_4_0,
  media::CHANNEL_LAYOUT_5_0_BACK,
  media::CHANNEL_LAYOUT_5_1_BACK,
  media::CHANNEL_LAYOUT_7_1,
};

// Number of samples per frame.
static const int kNumberSamplesPerAACFrame = 1024;

static int ExtractAdtsFrameSize(const uint8* adts_header) {
  return ((static_cast<int>(adts_header[5]) >> 5) |
          (static_cast<int>(adts_header[4]) << 3) |
          ((static_cast<int>(adts_header[3]) & 0x3) << 11));
}

static int ExtractAdtsFrequencyIndex(const uint8* adts_header) {
  return ((adts_header[2] >> 2) & 0xf);
}

static int ExtractAdtsChannelConfig(const uint8* adts_header) {
  return (((adts_header[3] >> 6) & 0x3) |
          ((adts_header[2] & 0x1) << 2));
}

// Return true if buf corresponds to an ADTS syncword.
// |buf| size must be at least 2.
static bool isAdtsSyncWord(const uint8* buf) {
  return (buf[0] == 0xff) && ((buf[1] & 0xf6) == 0xf0);
}

// Look for an ADTS syncword.
// |new_pos| returns
// - either the byte position of the ADTS frame (if found)
// - or the byte position of 1st byte that was not processed (if not found).
// In every case, the returned value in |new_pos| is such that new_pos >= pos
// |frame_sz| returns the size of the ADTS frame (if found).
// Return whether a syncword was found.
static bool LookForSyncWord(const uint8* raw_es, int raw_es_size,
                            int pos,
                            int* new_pos, int* frame_sz) {
  DCHECK_GE(pos, 0);
  DCHECK_LE(pos, raw_es_size);

  int max_offset = raw_es_size - kAdtsHeaderMinSize;
  if (pos >= max_offset) {
    // Do not change the position if:
    // - max_offset < 0: not enough bytes to get a full header
    //   Since pos >= 0, this is a subcase of the next condition.
    // - pos >= max_offset: might be the case after reading one full frame,
    //   |pos| is then incremented by the frame size and might then point
    //   to the end of the buffer.
    *new_pos = pos;
    return false;
  }

  for (int offset = pos; offset < max_offset; offset++) {
    const uint8* cur_buf = &raw_es[offset];

    if (!isAdtsSyncWord(cur_buf))
      // The first 12 bits must be 1.
      // The layer field (2 bits) must be set to 0.
      continue;

    int frame_size = ExtractAdtsFrameSize(cur_buf);
    if (frame_size < kAdtsHeaderMinSize) {
      // Too short to be an ADTS frame.
      continue;
    }

    // Check whether there is another frame
    // |size| apart from the current one.
    int remaining_size = raw_es_size - offset;
    if (remaining_size >= frame_size + 2 &&
        !isAdtsSyncWord(&cur_buf[frame_size])) {
      continue;
    }

    *new_pos = offset;
    *frame_sz = frame_size;
    return true;
  }

  *new_pos = max_offset;
  return false;
}

namespace media {
namespace mp2t {

EsParserAdts::EsParserAdts(
    const NewAudioConfigCB& new_audio_config_cb,
    const EmitBufferCB& emit_buffer_cb,
    bool sbr_in_mimetype)
  : new_audio_config_cb_(new_audio_config_cb),
    emit_buffer_cb_(emit_buffer_cb),
    sbr_in_mimetype_(sbr_in_mimetype) {
}

EsParserAdts::~EsParserAdts() {
}

bool EsParserAdts::Parse(const uint8* buf, int size,
                         base::TimeDelta pts,
                         base::TimeDelta dts) {
  int raw_es_size;
  const uint8* raw_es;

  // The incoming PTS applies to the access unit that comes just after
  // the beginning of |buf|.
  if (pts != kNoTimestamp()) {
    es_byte_queue_.Peek(&raw_es, &raw_es_size);
    pts_list_.push_back(EsPts(raw_es_size, pts));
  }

  // Copy the input data to the ES buffer.
  es_byte_queue_.Push(buf, size);
  es_byte_queue_.Peek(&raw_es, &raw_es_size);

  // Look for every ADTS frame in the ES buffer starting at offset = 0
  int es_position = 0;
  int frame_size;
  while (LookForSyncWord(raw_es, raw_es_size, es_position,
                         &es_position, &frame_size)) {
    DVLOG(LOG_LEVEL_ES)
        << "ADTS syncword @ pos=" << es_position
        << " frame_size=" << frame_size;
    DVLOG(LOG_LEVEL_ES)
        << "ADTS header: "
        << base::HexEncode(&raw_es[es_position], kAdtsHeaderMinSize);

    // Do not process the frame if this one is a partial frame.
    int remaining_size = raw_es_size - es_position;
    if (frame_size > remaining_size)
      break;

    // Update the audio configuration if needed.
    DCHECK_GE(frame_size, kAdtsHeaderMinSize);
    if (!UpdateAudioConfiguration(&raw_es[es_position]))
      return false;

    // Get the PTS & the duration of this access unit.
    while (!pts_list_.empty() &&
           pts_list_.front().first <= es_position) {
      audio_timestamp_helper_->SetBaseTimestamp(pts_list_.front().second);
      pts_list_.pop_front();
    }

    base::TimeDelta current_pts = audio_timestamp_helper_->GetTimestamp();
    base::TimeDelta frame_duration =
        audio_timestamp_helper_->GetFrameDuration(kNumberSamplesPerAACFrame);

    // Emit an audio frame.
    bool is_key_frame = true;
    scoped_refptr<StreamParserBuffer> stream_parser_buffer =
        StreamParserBuffer::CopyFrom(
            &raw_es[es_position],
            frame_size,
            is_key_frame);
    stream_parser_buffer->SetDecodeTimestamp(current_pts);
    stream_parser_buffer->set_timestamp(current_pts);
    stream_parser_buffer->set_duration(frame_duration);
    emit_buffer_cb_.Run(stream_parser_buffer);

    // Update the PTS of the next frame.
    audio_timestamp_helper_->AddFrames(kNumberSamplesPerAACFrame);

    // Skip the current frame.
    es_position += frame_size;
  }

  // Discard all the bytes that have been processed.
  DiscardEs(es_position);

  return true;
}

void EsParserAdts::Flush() {
}

void EsParserAdts::Reset() {
  es_byte_queue_.Reset();
  pts_list_.clear();
  last_audio_decoder_config_ = AudioDecoderConfig();
}

bool EsParserAdts::UpdateAudioConfiguration(const uint8* adts_header) {
  int frequency_index = ExtractAdtsFrequencyIndex(adts_header);
  if (frequency_index > kMaxSupportedFrequencyIndex) {
    // Frequency index 13 & 14 are reserved
    // while 15 means that the frequency is explicitly written
    // (not supported).
    return false;
  }

  int channel_configuration = ExtractAdtsChannelConfig(adts_header);
  if (channel_configuration == 0) {
    // TODO(damienv): Add support for inband channel configuration.
    return false;
  }

  // TODO(damienv): support HE-AAC frequency doubling (SBR)
  // based on the incoming ADTS profile.
  int samples_per_second = adts_frequency_table[frequency_index];
  int adts_profile = (adts_header[2] >> 6) & 0x3;

  // The following code is written according to ISO 14496 Part 3 Table 1.11 and
  // Table 1.22. (Table 1.11 refers to the capping to 48000, Table 1.22 refers
  // to SBR doubling the AAC sample rate.)
  // TODO(damienv) : Extend sample rate cap to 96kHz for Level 5 content.
  int extended_samples_per_second = sbr_in_mimetype_
      ? std::min(2 * samples_per_second, 48000)
      : samples_per_second;

  AudioDecoderConfig audio_decoder_config(
      kCodecAAC,
      kSampleFormatS16,
      adts_channel_layout[channel_configuration],
      extended_samples_per_second,
      NULL, 0,
      false);

  if (!audio_decoder_config.Matches(last_audio_decoder_config_)) {
    DVLOG(1) << "Sampling frequency: " << samples_per_second;
    DVLOG(1) << "Extended sampling frequency: " << extended_samples_per_second;
    DVLOG(1) << "Channel config: " << channel_configuration;
    DVLOG(1) << "Adts profile: " << adts_profile;
    // Reset the timestamp helper to use a new time scale.
    if (audio_timestamp_helper_) {
      base::TimeDelta base_timestamp = audio_timestamp_helper_->GetTimestamp();
      audio_timestamp_helper_.reset(
        new AudioTimestampHelper(samples_per_second));
      audio_timestamp_helper_->SetBaseTimestamp(base_timestamp);
    } else {
      audio_timestamp_helper_.reset(
          new AudioTimestampHelper(samples_per_second));
    }
    // Audio config notification.
    last_audio_decoder_config_ = audio_decoder_config;
    new_audio_config_cb_.Run(audio_decoder_config);
  }

  return true;
}

void EsParserAdts::DiscardEs(int nbytes) {
  DCHECK_GE(nbytes, 0);
  if (nbytes <= 0)
    return;

  // Adjust the ES position of each PTS.
  for (EsPtsList::iterator it = pts_list_.begin(); it != pts_list_.end(); ++it)
    it->first -= nbytes;

  // Discard |nbytes| of ES.
  es_byte_queue_.Pop(nbytes);
}

}  // namespace mp2t
}  // namespace media

