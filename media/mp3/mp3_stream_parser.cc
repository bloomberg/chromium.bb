// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mp3/mp3_stream_parser.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/message_loop/message_loop.h"
#include "media/base/bit_reader.h"
#include "media/base/buffers.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/text_track_config.h"
#include "media/base/video_decoder_config.h"
#include "net/http/http_util.h"

namespace media {

static const uint32 kMP3StartCodeMask = 0xffe00000;
static const uint32 kICYStartCode = 0x49435920; // 'ICY '

// Arbitrary upper bound on the size of an IceCast header before it
// triggers an error.
static const int kMaxIcecastHeaderSize = 4096;

static const uint32 kID3StartCodeMask = 0xffffff00;
static const uint32 kID3v1StartCode = 0x54414700; // 'TAG\0'
static const int kID3v1Size = 128;
static const int kID3v1ExtendedSize = 227;
static const uint32 kID3v2StartCode = 0x49443300; // 'ID3\0'

// Map that determines which bitrate_index & channel_mode combinations
// are allowed.
// Derived from: http://mpgedit.org/mpgedit/mpeg_format/MP3Format.html
static const bool kIsAllowed[17][4] = {
  { true, true, true, true },      // free
  { true, false, false, false },   // 32
  { true, false, false, false },   // 48
  { true, false, false, false },   // 56
  { true, true, true, true },      // 64
  { true, false, false, false },   // 80
  { true, true, true, true },      // 96
  { true, true, true, true },      // 112
  { true, true, true, true },      // 128
  { true, true, true, true },      // 160
  { true, true, true, true },      // 192
  { false, true, true, true },     // 224
  { false, true, true, true },     // 256
  { false, true, true, true },     // 320
  { false, true, true, true },     // 384
  { false, false, false, false }   // bad
};

// Maps version and layer information in the frame header
// into an index for the |kBitrateMap|.
// Derived from: http://mpgedit.org/mpgedit/mpeg_format/MP3Format.html
static const int kVersionLayerMap[4][4] = {
  // { reserved, L3, L2, L1 }
  { 5, 4, 4, 3 },  // MPEG 2.5
  { 5, 5, 5, 5 },  // reserved
  { 5, 4, 4, 3 },  // MPEG 2
  { 5, 2, 1, 0 }   // MPEG 1
};

// Maps the bitrate index field in the header and an index
// from |kVersionLayerMap| to a frame bitrate.
// Derived from: http://mpgedit.org/mpgedit/mpeg_format/MP3Format.html
static const int kBitrateMap[16][6] = {
  // { V1L1, V1L2, V1L3, V2L1, V2L2 & V2L3, reserved }
  { 0, 0, 0, 0, 0, 0 },
  { 32, 32, 32, 32, 8, 0 },
  { 64, 48, 40, 48, 16, 0 },
  { 96, 56, 48, 56, 24, 0 },
  { 128, 64, 56, 64, 32, 0 },
  { 160, 80, 64, 80, 40, 0 },
  { 192, 96, 80, 96, 48, 0 },
  { 224, 112, 96, 112, 56, 0 },
  { 256, 128, 112, 128, 64, 0 },
  { 288, 160, 128, 144, 80, 0 },
  { 320, 192, 160, 160, 96, 0 },
  { 352, 224, 192, 176, 112, 0 },
  { 384, 256, 224, 192, 128, 0 },
  { 416, 320, 256, 224, 144, 0 },
  { 448, 384, 320, 256, 160, 0 },
  { 0, 0, 0, 0, 0}
};

// Maps the sample rate index and version fields from the frame header
// to a sample rate.
// Derived from: http://mpgedit.org/mpgedit/mpeg_format/MP3Format.html
static const int kSampleRateMap[4][4] = {
  // { V2.5, reserved, V2, V1 }
  { 11025, 0, 22050, 44100 },
  { 12000, 0, 24000, 48000 },
  { 8000, 0, 16000, 32000 },
  { 0, 0, 0, 0 }
};

// Frame header field constants.
static const int kVersion2 = 2;
static const int kVersionReserved = 1;
static const int kVersion2_5 = 0;
static const int kLayerReserved = 0;
static const int kLayer1 = 3;
static const int kLayer2 = 2;
static const int kLayer3 = 1;
static const int kBitrateFree = 0;
static const int kBitrateBad = 0xf;
static const int kSampleRateReserved = 3;

MP3StreamParser::MP3StreamParser()
    : state_(UNINITIALIZED),
      in_media_segment_(false) {
}

MP3StreamParser::~MP3StreamParser() {}

void MP3StreamParser::Init(const InitCB& init_cb,
                           const NewConfigCB& config_cb,
                           const NewBuffersCB& new_buffers_cb,
                           const NewTextBuffersCB& text_cb,
                           const NeedKeyCB& need_key_cb,
                           const NewMediaSegmentCB& new_segment_cb,
                           const base::Closure& end_of_segment_cb,
                           const LogCB& log_cb) {
  DVLOG(1) << __FUNCTION__;
  DCHECK_EQ(state_, UNINITIALIZED);
  init_cb_ = init_cb;
  config_cb_ = config_cb;
  new_buffers_cb_ = new_buffers_cb;
  new_segment_cb_ = new_segment_cb;
  end_of_segment_cb_ = end_of_segment_cb;
  log_cb_ = log_cb;

  ChangeState(INITIALIZED);
}

void MP3StreamParser::Flush() {
  DVLOG(1) << __FUNCTION__;
  DCHECK_NE(state_, UNINITIALIZED);
  queue_.Reset();
  timestamp_helper_->SetBaseTimestamp(base::TimeDelta());
  in_media_segment_ = false;
}

bool MP3StreamParser::Parse(const uint8* buf, int size) {
  DVLOG(1) << __FUNCTION__ << "(" << size << ")";
  DCHECK(buf);
  DCHECK_GT(size, 0);
  DCHECK_NE(state_, UNINITIALIZED);

  if (state_ == PARSE_ERROR)
    return false;

  DCHECK_EQ(state_, INITIALIZED);

  queue_.Push(buf, size);

  bool end_of_segment = true;
  BufferQueue buffers;
  for (;;) {
    const uint8* data;
    int data_size;
    queue_.Peek(&data, &data_size);

    if (data_size < 4)
      break;

    uint32 start_code = data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3];
    int bytes_read = 0;
    bool parsed_metadata = true;
    if ((start_code & kMP3StartCodeMask) == kMP3StartCodeMask) {
      bytes_read = ParseMP3Frame(data, data_size, &buffers);

      // Only allow the current segment to end if a full frame has been parsed.
      end_of_segment = bytes_read > 0;
      parsed_metadata = false;
    } else if (start_code == kICYStartCode) {
      bytes_read = ParseIcecastHeader(data, data_size);
    } else if ((start_code & kID3StartCodeMask) == kID3v1StartCode) {
      bytes_read = ParseID3v1(data, data_size);
    } else if ((start_code & kID3StartCodeMask) == kID3v2StartCode) {
      bytes_read = ParseID3v2(data, data_size);
    } else {
      bytes_read = FindNextValidStartCode(data, data_size);

      if (bytes_read > 0) {
        DVLOG(1) << "Unexpected start code 0x" << std::hex << start_code;
        DVLOG(1) << "SKIPPING " << bytes_read << " bytes of garbage.";
      }
    }

    CHECK_LE(bytes_read, data_size);

    if (bytes_read < 0) {
      ChangeState(PARSE_ERROR);
      return false;
    } else if (bytes_read == 0) {
      // Need more data.
      break;
    }

    // Send pending buffers if we have encountered metadata.
    if (parsed_metadata && !buffers.empty() && !SendBuffers(&buffers, true))
      return false;

    queue_.Pop(bytes_read);
    end_of_segment = true;
  }

  if (buffers.empty())
    return true;

  // Send buffers collected in this append that haven't been sent yet.
  return SendBuffers(&buffers, end_of_segment);
}

void MP3StreamParser::ChangeState(State state) {
  DVLOG(1) << __FUNCTION__ << "() : " << state_ << " -> " << state;
  state_ = state;
}

int MP3StreamParser::ParseFrameHeader(const uint8* data, int size,
                                      int* frame_size,
                                      int* sample_rate,
                                      ChannelLayout* channel_layout,
                                      int* sample_count) const {
  DCHECK(data);
  DCHECK_GE(size, 0);
  DCHECK(frame_size);

  if (size < 4)
    return 0;

  BitReader reader(data, size);
  int sync;
  int version;
  int layer;
  int is_protected;
  int bitrate_index;
  int sample_rate_index;
  int has_padding;
  int is_private;
  int channel_mode;
  int other_flags;

  if (!reader.ReadBits(11, &sync) ||
      !reader.ReadBits(2, &version) ||
      !reader.ReadBits(2, &layer) ||
      !reader.ReadBits(1, &is_protected) ||
      !reader.ReadBits(4, &bitrate_index) ||
      !reader.ReadBits(2, &sample_rate_index) ||
      !reader.ReadBits(1, &has_padding) ||
      !reader.ReadBits(1, &is_private) ||
      !reader.ReadBits(2, &channel_mode) ||
      !reader.ReadBits(6, &other_flags)) {
    return -1;
  }

  DVLOG(2) << "Header data :" << std::hex
           << " sync 0x" << sync
           << " version 0x" << version
           << " layer 0x" << layer
           << " bitrate_index 0x" << bitrate_index
           << " sample_rate_index 0x" << sample_rate_index
           << " channel_mode 0x" << channel_mode;

  if (sync != 0x7ff ||
      version == kVersionReserved ||
      layer == kLayerReserved ||
      bitrate_index == kBitrateFree || bitrate_index == kBitrateBad ||
      sample_rate_index == kSampleRateReserved) {
    MEDIA_LOG(log_cb_) << "Invalid header data :" << std::hex
                       << " sync 0x" << sync
                       << " version 0x" << version
                       << " layer 0x" << layer
                       << " bitrate_index 0x" << bitrate_index
                       << " sample_rate_index 0x" << sample_rate_index
                       << " channel_mode 0x" << channel_mode;
    return -1;
  }

  if (layer == kLayer2 && kIsAllowed[bitrate_index][channel_mode]) {
    MEDIA_LOG(log_cb_) << "Invalid (bitrate_index, channel_mode) combination :"
                       << std::hex
                       << " bitrate_index " << bitrate_index
                       << " channel_mode " << channel_mode;
    return -1;
  }

  int bitrate = kBitrateMap[bitrate_index][kVersionLayerMap[version][layer]];

  if (bitrate == 0) {
    MEDIA_LOG(log_cb_) << "Invalid bitrate :" << std::hex
                       << " version " << version
                       << " layer " << layer
                       << " bitrate_index " << bitrate_index;
    return -1;
  }

  DVLOG(2) << " bitrate " << bitrate;

  int frame_sample_rate = kSampleRateMap[sample_rate_index][version];
  if (frame_sample_rate == 0) {
    MEDIA_LOG(log_cb_) << "Invalid sample rate :" << std::hex
                       << " version " << version
                       << " sample_rate_index " << sample_rate_index;
    return -1;
  }

  if (sample_rate)
    *sample_rate = frame_sample_rate;

  // http://teslabs.com/openplayer/docs/docs/specs/mp3_structure2.pdf
  // Table 2.1.5
  int samples_per_frame;
  switch (layer) {
    case kLayer1:
      samples_per_frame = 384;
      break;

    case kLayer2:
      samples_per_frame = 1152;
      break;

    case kLayer3:
      if (version == kVersion2 || version == kVersion2_5)
        samples_per_frame = 576;
      else
        samples_per_frame = 1152;
      break;

    default:
      return -1;
  }

  if (sample_count)
    *sample_count = samples_per_frame;

  // http://teslabs.com/openplayer/docs/docs/specs/mp3_structure2.pdf
  // Text just below Table 2.1.5.
  if (layer == kLayer1) {
    // This formulation is a slight variation on the equation below,
    // but has slightly different truncation characteristics to deal
    // with the fact that Layer 1 has 4 byte "slots" instead of single
    // byte ones.
    *frame_size = 4 * (12 * bitrate * 1000 / frame_sample_rate);
  } else {
    *frame_size =
        ((samples_per_frame / 8) * bitrate * 1000) / frame_sample_rate;
  }

  if (has_padding)
    *frame_size += (layer == kLayer1) ? 4 : 1;

  if (channel_layout) {
    // Map Stereo(0), Joint Stereo(1), and Dual Channel (2) to
    // CHANNEL_LAYOUT_STEREO and Single Channel (3) to CHANNEL_LAYOUT_MONO.
    *channel_layout =
        (channel_mode == 3) ? CHANNEL_LAYOUT_MONO : CHANNEL_LAYOUT_STEREO;
  }

  return 4;
}

int MP3StreamParser::ParseMP3Frame(const uint8* data,
                                   int size,
                                   BufferQueue* buffers) {
  DVLOG(2) << __FUNCTION__ << "(" << size << ")";

  int sample_rate;
  ChannelLayout channel_layout;
  int frame_size;
  int sample_count;
  int bytes_read = ParseFrameHeader(
      data, size, &frame_size, &sample_rate, &channel_layout, &sample_count);

  if (bytes_read <= 0)
    return bytes_read;

  // Make sure data contains the entire frame.
  if (size < frame_size)
    return 0;

  DVLOG(2) << " sample_rate " << sample_rate
           << " channel_layout " << channel_layout
           << " frame_size " << frame_size;

  if (config_.IsValidConfig() &&
      (config_.samples_per_second() != sample_rate ||
       config_.channel_layout() != channel_layout)) {
    // Clear config data so that a config change is initiated.
    config_ = AudioDecoderConfig();

    // Send all buffers associated with the previous config.
    if (!buffers->empty() && !SendBuffers(buffers, true))
      return -1;
  }

  if (!config_.IsValidConfig()) {
    config_.Initialize(kCodecMP3, kSampleFormatF32, channel_layout,
                       sample_rate, NULL, 0, false, false,
                       base::TimeDelta(), base::TimeDelta());

    base::TimeDelta base_timestamp;
    if (timestamp_helper_)
      base_timestamp = timestamp_helper_->GetTimestamp();

    timestamp_helper_.reset(new AudioTimestampHelper(sample_rate));
    timestamp_helper_->SetBaseTimestamp(base_timestamp);

    VideoDecoderConfig video_config;
    bool success = config_cb_.Run(config_, video_config, TextTrackConfigMap());

    if (!init_cb_.is_null())
      base::ResetAndReturn(&init_cb_).Run(success, kInfiniteDuration());

    if (!success)
      return -1;
  }

  scoped_refptr<StreamParserBuffer> buffer =
      StreamParserBuffer::CopyFrom(data, frame_size, true);
  buffer->set_timestamp(timestamp_helper_->GetTimestamp());
  buffer->set_duration(timestamp_helper_->GetFrameDuration(sample_count));
  buffers->push_back(buffer);

  timestamp_helper_->AddFrames(sample_count);

  return frame_size;
}

int MP3StreamParser::ParseIcecastHeader(const uint8* data, int size) {
  DVLOG(1) << __FUNCTION__ << "(" << size << ")";

  if (size < 4)
    return 0;

  if (memcmp("ICY ", data, 4))
    return -1;

  int locate_size = std::min(size, kMaxIcecastHeaderSize);
  int offset = net::HttpUtil::LocateEndOfHeaders(
      reinterpret_cast<const char*>(data), locate_size, 4);
  if (offset < 0) {
    if (locate_size == kMaxIcecastHeaderSize) {
      MEDIA_LOG(log_cb_) << "Icecast header is too large.";
      return -1;
    }

    return 0;
  }

  return offset;
}

int MP3StreamParser::ParseID3v1(const uint8* data, int size) {
  DVLOG(1) << __FUNCTION__ << "(" << size << ")";

  if (size < kID3v1Size)
    return 0;

  // TODO(acolwell): Add code to actually validate ID3v1 data and
  // expose it as a metadata text track.
  return !memcmp(data, "TAG+", 4) ? kID3v1ExtendedSize : kID3v1Size;
}

int MP3StreamParser::ParseID3v2(const uint8* data, int size) {
  DVLOG(1) << __FUNCTION__ << "(" << size << ")";

  if (size < 10)
    return 0;

  BitReader reader(data, size);
  int32 id;
  int version;
  uint8 flags;
  int32 id3_size;

  if (!reader.ReadBits(24, &id) ||
      !reader.ReadBits(16, &version) ||
      !reader.ReadBits(8, &flags) ||
      !ParseSyncSafeInt(&reader, &id3_size)) {
    return -1;
  }

  int32 actual_tag_size = 10 + id3_size;

  // Increment size if 'Footer present' flag is set.
  if (flags & 0x10)
    actual_tag_size += 10;

  // Make sure we have the entire tag.
  if (size < actual_tag_size)
    return 0;

  // TODO(acolwell): Add code to actually validate ID3v2 data and
  // expose it as a metadata text track.
  return actual_tag_size;
}

bool MP3StreamParser::ParseSyncSafeInt(BitReader* reader, int32* value) {
  *value = 0;
  for (int i = 0; i < 4; ++i) {
    uint8 tmp;
    if (!reader->ReadBits(1, &tmp) || tmp != 0) {
      MEDIA_LOG(log_cb_) << "ID3 syncsafe integer byte MSb is not 0!";
      return false;
    }

    if (!reader->ReadBits(7, &tmp))
      return false;

    *value <<= 7;
    *value += tmp;
  }

  return true;
}

int MP3StreamParser::FindNextValidStartCode(const uint8* data, int size) const {
  const uint8* start = data;
  const uint8* end = data + size;

  while (start < end) {
    int bytes_left = end - start;
    const uint8* candidate_start_code =
        static_cast<const uint8*>(memchr(start, 0xff, bytes_left));

    if (!candidate_start_code)
      return 0;

    bool parse_header_failed = false;
    const uint8* sync = candidate_start_code;
    // Try to find 3 valid frames in a row. 3 was selected to decrease
    // the probability of false positives.
    for (int i = 0; i < 3; ++i) {
      int sync_size = end - sync;
      int frame_size;
      int sync_bytes = ParseFrameHeader(
          sync, sync_size, &frame_size, NULL, NULL, NULL);

      if (sync_bytes == 0)
        return 0;

      if (sync_bytes > 0) {
        DCHECK_LT(sync_bytes, sync_size);

        // Skip over this frame so we can check the next one.
        sync += frame_size;

        // Make sure the next frame starts inside the buffer.
        if (sync >= end)
          return 0;
      } else {
        DVLOG(1) << "ParseFrameHeader() " << i << " failed @" << (sync - data);
        parse_header_failed = true;
        break;
      }
    }

    if (parse_header_failed) {
      // One of the frame header parses failed so |candidate_start_code|
      // did not point to the start of a real frame. Move |start| forward
      // so we can find the next candidate.
      start = candidate_start_code + 1;
      continue;
    }

    return candidate_start_code - data;
  }

  return 0;
}

bool MP3StreamParser::SendBuffers(BufferQueue* buffers, bool end_of_segment) {
  DCHECK(!buffers->empty());

  if (!in_media_segment_) {
    in_media_segment_ = true;
    new_segment_cb_.Run();
  }

  BufferQueue empty_video_buffers;
  if (!new_buffers_cb_.Run(*buffers, empty_video_buffers))
    return false;
  buffers->clear();

  if (end_of_segment) {
    in_media_segment_ = false;
    end_of_segment_cb_.Run();
  }

  return true;
}

}  // namespace media
