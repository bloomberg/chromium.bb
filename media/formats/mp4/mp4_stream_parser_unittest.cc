// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/mp4_stream_parser.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_switches.h"
#include "media/base/media_track.h"
#include "media/base/media_tracks.h"
#include "media/base/mock_media_log.h"
#include "media/base/stream_parser.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/base/text_track_config.h"
#include "media/base/video_decoder_config.h"
#include "media/formats/mp4/es_descriptor.h"
#include "media/formats/mp4/fourccs.h"
#include "media/media_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::StrictMock;
using base::TimeDelta;

namespace media {
namespace mp4 {

// Matchers for verifying common media log entry strings.
MATCHER(SampleEncryptionInfoUnavailableLog, "") {
  return CONTAINS_STRING(arg, "Sample encryption info is not available.");
}

MATCHER_P(ErrorLog, error_string, "") {
  return CONTAINS_STRING(arg, error_string);
}

class MP4StreamParserTest : public testing::Test {
 public:
  MP4StreamParserTest()
      : configs_received_(false),
        lower_bound_(
            DecodeTimestamp::FromPresentationTime(base::TimeDelta::Max())) {
    std::set<int> audio_object_types;
    audio_object_types.insert(kISO_14496_3);
    parser_.reset(new MP4StreamParser(audio_object_types, false, false));
  }

 protected:
  StrictMock<MockMediaLog> media_log_;
  std::unique_ptr<MP4StreamParser> parser_;
  bool configs_received_;
  std::unique_ptr<MediaTracks> media_tracks_;
  AudioDecoderConfig audio_decoder_config_;
  VideoDecoderConfig video_decoder_config_;
  DecodeTimestamp lower_bound_;
  StreamParser::TrackId audio_track_id_;
  StreamParser::TrackId video_track_id_;

  bool AppendData(const uint8_t* data, size_t length) {
    return parser_->Parse(data, length);
  }

  bool AppendDataInPieces(const uint8_t* data,
                          size_t length,
                          size_t piece_size) {
    const uint8_t* start = data;
    const uint8_t* end = data + length;
    while (start < end) {
      size_t append_size = std::min(piece_size,
                                    static_cast<size_t>(end - start));
      if (!AppendData(start, append_size))
        return false;
      start += append_size;
    }
    return true;
  }

  void InitF(const StreamParser::InitParameters& expected_params,
             const StreamParser::InitParameters& params) {
    DVLOG(1) << "InitF: dur=" << params.duration.InMicroseconds();
    EXPECT_EQ(expected_params.duration, params.duration);
    EXPECT_EQ(expected_params.timeline_offset, params.timeline_offset);
    EXPECT_EQ(expected_params.liveness, params.liveness);
    EXPECT_EQ(expected_params.detected_audio_track_count,
              params.detected_audio_track_count);
    EXPECT_EQ(expected_params.detected_video_track_count,
              params.detected_video_track_count);
    EXPECT_EQ(expected_params.detected_text_track_count,
              params.detected_text_track_count);
  }

  bool NewConfigF(std::unique_ptr<MediaTracks> tracks,
                  const StreamParser::TextTrackConfigMap& tc) {
    configs_received_ = true;
    CHECK(tracks.get());
    DVLOG(1) << "NewConfigF: got " << tracks->tracks().size() << " tracks";
    for (const auto& track : tracks->tracks()) {
      const auto& track_id = track->bytestream_track_id();
      if (track->type() == MediaTrack::Audio) {
        audio_track_id_ = track_id;
        audio_decoder_config_ = tracks->getAudioConfig(track_id);
        DVLOG(1) << "track_id=" << track_id << " audio config="
                 << (audio_decoder_config_.IsValidConfig()
                         ? audio_decoder_config_.AsHumanReadableString()
                         : "INVALID");
      } else if (track->type() == MediaTrack::Video) {
        video_track_id_ = track_id;
        video_decoder_config_ = tracks->getVideoConfig(track_id);
        DVLOG(1) << "track_id=" << track_id << " video config="
                 << (video_decoder_config_.IsValidConfig()
                         ? video_decoder_config_.AsHumanReadableString()
                         : "INVALID");
      }
    }
    media_tracks_ = std::move(tracks);
    return true;
  }

  bool NewBuffersF(const StreamParser::BufferQueueMap& buffer_queue_map) {
    DecodeTimestamp lowest_end_dts = kNoDecodeTimestamp();
    for (const auto& it : buffer_queue_map) {
      DVLOG(3) << "Buffers for track_id=" << it.first;
      DCHECK(!it.second.empty());

      if (lowest_end_dts == kNoDecodeTimestamp() ||
          lowest_end_dts > it.second.back()->GetDecodeTimestamp())
        lowest_end_dts = it.second.back()->GetDecodeTimestamp();

      for (const auto& buf : it.second) {
        DVLOG(3) << "  track_id=" << buf->track_id()
                 << ", size=" << buf->data_size()
                 << ", pts=" << buf->timestamp().InSecondsF()
                 << ", dts=" << buf->GetDecodeTimestamp().InSecondsF()
                 << ", dur=" << buf->duration().InSecondsF();
        // Ensure that track ids are properly assigned on all emitted buffers.
        EXPECT_EQ(it.first, buf->track_id());
      }
    }

    EXPECT_NE(lowest_end_dts, kNoDecodeTimestamp());

    if (lower_bound_ != kNoDecodeTimestamp() && lowest_end_dts < lower_bound_) {
      return false;
    }

    lower_bound_ = lowest_end_dts;
    return true;
  }

  void KeyNeededF(EmeInitDataType type, const std::vector<uint8_t>& init_data) {
    DVLOG(1) << "KeyNeededF: " << init_data.size();
    EXPECT_EQ(EmeInitDataType::CENC, type);
    EXPECT_FALSE(init_data.empty());
  }

  void NewSegmentF() {
    DVLOG(1) << "NewSegmentF";
    lower_bound_ = kNoDecodeTimestamp();
  }

  void EndOfSegmentF() {
    DVLOG(1) << "EndOfSegmentF()";
    lower_bound_ =
        DecodeTimestamp::FromPresentationTime(base::TimeDelta::Max());
  }

  void InitializeParserWithInitParametersExpectations(
      StreamParser::InitParameters params) {
    parser_->Init(
        base::Bind(&MP4StreamParserTest::InitF, base::Unretained(this), params),
        base::Bind(&MP4StreamParserTest::NewConfigF, base::Unretained(this)),
        base::Bind(&MP4StreamParserTest::NewBuffersF, base::Unretained(this)),
        true,
        base::Bind(&MP4StreamParserTest::KeyNeededF, base::Unretained(this)),
        base::Bind(&MP4StreamParserTest::NewSegmentF, base::Unretained(this)),
        base::Bind(&MP4StreamParserTest::EndOfSegmentF, base::Unretained(this)),
        &media_log_);
  }

  StreamParser::InitParameters GetDefaultInitParametersExpectations() {
    // Most unencrypted test mp4 files have zero duration and are treated as
    // live streams.
    StreamParser::InitParameters params(kInfiniteDuration);
    params.liveness = DemuxerStream::LIVENESS_LIVE;
    params.detected_audio_track_count = 1;
    params.detected_video_track_count = 1;
    params.detected_text_track_count = 0;
    return params;
  }

  void InitializeParserAndExpectLiveness(DemuxerStream::Liveness liveness) {
    auto params = GetDefaultInitParametersExpectations();
    params.liveness = liveness;
    InitializeParserWithInitParametersExpectations(params);
  }

  void InitializeParser() {
    InitializeParserWithInitParametersExpectations(
        GetDefaultInitParametersExpectations());
  }

  bool ParseMP4File(const std::string& filename, int append_bytes) {
    scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(filename);
    EXPECT_TRUE(AppendDataInPieces(buffer->data(),
                                   buffer->data_size(),
                                   append_bytes));
    return true;
  }
};

TEST_F(MP4StreamParserTest, UnalignedAppend) {
  // Test small, non-segment-aligned appends (small enough to exercise
  // incremental append system)
  InitializeParser();
  ParseMP4File("bear-1280x720-av_frag.mp4", 512);
}

TEST_F(MP4StreamParserTest, BytewiseAppend) {
  // Ensure no incremental errors occur when parsing
  InitializeParser();
  ParseMP4File("bear-1280x720-av_frag.mp4", 1);
}

TEST_F(MP4StreamParserTest, MultiFragmentAppend) {
  // Large size ensures multiple fragments are appended in one call (size is
  // larger than this particular test file)
  InitializeParser();
  ParseMP4File("bear-1280x720-av_frag.mp4", 768432);
}

TEST_F(MP4StreamParserTest, Flush) {
  // Flush while reading sample data, then start a new stream.
  InitializeParser();

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-1280x720-av_frag.mp4");
  EXPECT_TRUE(AppendDataInPieces(buffer->data(), 65536, 512));
  parser_->Flush();
  EXPECT_TRUE(AppendDataInPieces(buffer->data(),
                                 buffer->data_size(),
                                 512));
}

TEST_F(MP4StreamParserTest, Reinitialization) {
  InitializeParser();

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-1280x720-av_frag.mp4");
  EXPECT_TRUE(AppendDataInPieces(buffer->data(),
                                 buffer->data_size(),
                                 512));
  EXPECT_TRUE(AppendDataInPieces(buffer->data(),
                                 buffer->data_size(),
                                 512));
}

TEST_F(MP4StreamParserTest, UnknownDuration_V0_AllBitsSet) {
  InitializeParser();
  // 32 bit duration field in mvhd box, all bits set.
  ParseMP4File(
      "bear-1280x720-av_frag-initsegment-mvhd_version_0-mvhd_duration_bits_all_"
      "set.mp4",
      512);
}

TEST_F(MP4StreamParserTest, MPEG2_AAC_LC) {
  InSequence s;
  std::set<int> audio_object_types;
  audio_object_types.insert(kISO_13818_7_AAC_LC);
  parser_.reset(new MP4StreamParser(audio_object_types, false, false));
  auto params = GetDefaultInitParametersExpectations();
  params.detected_video_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);
  ParseMP4File("bear-mpeg2-aac-only_frag.mp4", 512);
}

// Test that a moov box is not always required after Flush() is called.
TEST_F(MP4StreamParserTest, NoMoovAfterFlush) {
  InitializeParser();

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-1280x720-av_frag.mp4");
  EXPECT_TRUE(AppendDataInPieces(buffer->data(),
                                 buffer->data_size(),
                                 512));
  parser_->Flush();

  const int kFirstMoofOffset = 1307;
  EXPECT_TRUE(AppendDataInPieces(buffer->data() + kFirstMoofOffset,
                                 buffer->data_size() - kFirstMoofOffset,
                                 512));
}

// Test an invalid file where there are encrypted samples, but
// SampleEncryptionBox (senc) and SampleAuxiliaryInformation{Sizes|Offsets}Box
// (saiz|saio) are missing.
// The parser should fail instead of crash. See http://crbug.com/361347
TEST_F(MP4StreamParserTest, MissingSampleEncryptionInfo) {
  InSequence s;

  // Encrypted test mp4 files have non-zero duration and are treated as
  // recorded streams.
  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::TimeDelta::FromMicroseconds(23219);
  params.liveness = DemuxerStream::LIVENESS_RECORDED;
  params.detected_video_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-1280x720-a_frag-cenc_missing-saiz-saio.mp4");
  EXPECT_MEDIA_LOG(SampleEncryptionInfoUnavailableLog());
  EXPECT_FALSE(AppendDataInPieces(buffer->data(), buffer->data_size(), 512));
}

// Test a file where all video samples start with an Access Unit
// Delimiter (AUD) NALU.
TEST_F(MP4StreamParserTest, VideoSamplesStartWithAUDs) {
  auto params = GetDefaultInitParametersExpectations();
  params.detected_audio_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);
  ParseMP4File("bear-1280x720-av_with-aud-nalus_frag.mp4", 512);
}

TEST_F(MP4StreamParserTest, HEVC_in_MP4_container) {
#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
  bool expect_success = true;
#else
  bool expect_success = false;
  EXPECT_MEDIA_LOG(ErrorLog("Unsupported VisualSampleEntry type hev1"));
#endif
  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::TimeDelta::FromMicroseconds(1002000);
  params.liveness = DemuxerStream::LIVENESS_RECORDED;
  params.detected_audio_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile("bear-hevc-frag.mp4");
  EXPECT_EQ(expect_success,
            AppendDataInPieces(buffer->data(), buffer->data_size(), 512));
#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
  EXPECT_EQ(kCodecHEVC, video_decoder_config_.codec());
  EXPECT_EQ(HEVCPROFILE_MAIN, video_decoder_config_.profile());
#endif
}

// Sample encryption information is stored as CencSampleAuxiliaryDataFormat
// (ISO/IEC 23001-7:2015 8) inside 'mdat' box. No SampleEncryption ('senc') box.
TEST_F(MP4StreamParserTest, CencWithEncryptionInfoStoredAsAuxDataInMdat) {
  // Encrypted test mp4 files have non-zero duration and are treated as
  // recorded streams.
  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::TimeDelta::FromMicroseconds(2736066);
  params.liveness = DemuxerStream::LIVENESS_RECORDED;
  params.detected_audio_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-1280x720-v_frag-cenc.mp4");
  EXPECT_TRUE(AppendDataInPieces(buffer->data(), buffer->data_size(), 512));
}

TEST_F(MP4StreamParserTest, CencWithSampleEncryptionBox) {
  // Encrypted test mp4 files have non-zero duration and are treated as
  // recorded streams.
  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::TimeDelta::FromMicroseconds(2736066);
  params.liveness = DemuxerStream::LIVENESS_RECORDED;
  params.detected_audio_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-640x360-v_frag-cenc-senc.mp4");
  EXPECT_TRUE(AppendDataInPieces(buffer->data(), buffer->data_size(), 512));
}

TEST_F(MP4StreamParserTest, NaturalSizeWithoutPASP) {
  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::TimeDelta::FromMicroseconds(1000966);
  params.liveness = DemuxerStream::LIVENESS_RECORDED;
  params.detected_audio_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-640x360-non_square_pixel-without_pasp.mp4");

  EXPECT_TRUE(AppendDataInPieces(buffer->data(), buffer->data_size(), 512));
  EXPECT_EQ(gfx::Size(639, 360), video_decoder_config_.natural_size());
}

TEST_F(MP4StreamParserTest, NaturalSizeWithPASP) {
  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::TimeDelta::FromMicroseconds(1000966);
  params.liveness = DemuxerStream::LIVENESS_RECORDED;
  params.detected_audio_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-640x360-non_square_pixel-with_pasp.mp4");

  EXPECT_TRUE(AppendDataInPieces(buffer->data(), buffer->data_size(), 512));
  EXPECT_EQ(gfx::Size(639, 360), video_decoder_config_.natural_size());
}

TEST_F(MP4StreamParserTest, DemuxingAC3) {
  std::set<int> audio_object_types;
  audio_object_types.insert(kAC3);
  parser_.reset(new MP4StreamParser(audio_object_types, false, false));

#if BUILDFLAG(ENABLE_AC3_EAC3_AUDIO_DEMUXING)
  bool expect_success = true;
#else
  bool expect_success = false;
  EXPECT_MEDIA_LOG(ErrorLog("Unsupported audio format 0x61632d33 in stsd box"));
#endif

  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::TimeDelta::FromMicroseconds(1045000);
  params.liveness = DemuxerStream::LIVENESS_RECORDED;
  params.detected_video_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-ac3-only-frag.mp4");
  EXPECT_EQ(expect_success,
            AppendDataInPieces(buffer->data(), buffer->data_size(), 512));
}

TEST_F(MP4StreamParserTest, DemuxingEAC3) {
  std::set<int> audio_object_types;
  audio_object_types.insert(kEAC3);
  parser_.reset(new MP4StreamParser(audio_object_types, false, false));

#if BUILDFLAG(ENABLE_AC3_EAC3_AUDIO_DEMUXING)
  bool expect_success = true;
#else
  bool expect_success = false;
  EXPECT_MEDIA_LOG(ErrorLog("Unsupported audio format 0x65632d33 in stsd box"));
#endif

  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::TimeDelta::FromMicroseconds(1045000);
  params.liveness = DemuxerStream::LIVENESS_RECORDED;
  params.detected_video_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-eac3-only-frag.mp4");
  EXPECT_EQ(expect_success,
            AppendDataInPieces(buffer->data(), buffer->data_size(), 512));
}

TEST_F(MP4StreamParserTest, Flac) {
  parser_.reset(new MP4StreamParser(std::set<int>(), false, true));

  auto params = GetDefaultInitParametersExpectations();
  params.detected_video_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile("bear-flac_frag.mp4");
  EXPECT_TRUE(AppendDataInPieces(buffer->data(), buffer->data_size(), 512));
}

TEST_F(MP4StreamParserTest, Flac192kHz) {
  parser_.reset(new MP4StreamParser(std::set<int>(), false, true));

  auto params = GetDefaultInitParametersExpectations();
  params.detected_video_track_count = 0;

  // 192kHz exceeds the range of AudioSampleEntry samplerate. The correct
  // samplerate should be applied from the dfLa STREAMINFO metadata block.
  EXPECT_MEDIA_LOG(FlacAudioSampleRateOverriddenByStreaminfo("0", "192000"));
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-flac-192kHz_frag.mp4");
  EXPECT_TRUE(AppendDataInPieces(buffer->data(), buffer->data_size(), 512));
}

TEST_F(MP4StreamParserTest, FourCCToString) {
  // A real FOURCC should print.
  EXPECT_EQ("mvex", FourCCToString(FOURCC_MVEX));

  // Invalid FOURCC should also print whenever ASCII values are printable.
  EXPECT_EQ("fake", FourCCToString(static_cast<FourCC>(0x66616b65)));

  // Invalid FORCC with non-printable values should not give error message.
  EXPECT_EQ("0x66616b00", FourCCToString(static_cast<FourCC>(0x66616b00)));
}

TEST_F(MP4StreamParserTest, MediaTrackInfoSourcing) {
  InitializeParser();
  ParseMP4File("bear-1280x720-av_frag.mp4", 4096);

  EXPECT_EQ(media_tracks_->tracks().size(), 2u);
  const MediaTrack& video_track = *(media_tracks_->tracks()[0]);
  EXPECT_EQ(video_track.type(), MediaTrack::Video);
  EXPECT_EQ(video_track.bytestream_track_id(), 1);
  EXPECT_EQ(video_track.kind(), "main");
  EXPECT_EQ(video_track.label(), "VideoHandler");
  EXPECT_EQ(video_track.language(), "und");

  const MediaTrack& audio_track = *(media_tracks_->tracks()[1]);
  EXPECT_EQ(audio_track.type(), MediaTrack::Audio);
  EXPECT_EQ(audio_track.bytestream_track_id(), 2);
  EXPECT_EQ(audio_track.kind(), "main");
  EXPECT_EQ(audio_track.label(), "SoundHandler");
  EXPECT_EQ(audio_track.language(), "und");
}

TEST_F(MP4StreamParserTest, TextTrackDetection) {
  auto params = GetDefaultInitParametersExpectations();
  params.detected_text_track_count = 1;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-1280x720-avt_subt_frag.mp4");

  EXPECT_TRUE(AppendDataInPieces(buffer->data(), buffer->data_size(), 512));
}

TEST_F(MP4StreamParserTest, MultiTrackFile) {
  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::TimeDelta::FromMilliseconds(4248);
  params.liveness = DemuxerStream::LIVENESS_RECORDED;
  params.detected_audio_track_count = 2;
  params.detected_video_track_count = 2;
  InitializeParserWithInitParametersExpectations(params);
  ParseMP4File("bbb-320x240-2video-2audio.mp4", 4096);

  EXPECT_EQ(media_tracks_->tracks().size(), 4u);

  const MediaTrack& video_track1 = *(media_tracks_->tracks()[0]);
  EXPECT_EQ(video_track1.type(), MediaTrack::Video);
  EXPECT_EQ(video_track1.bytestream_track_id(), 1);
  EXPECT_EQ(video_track1.kind(), "main");
  EXPECT_EQ(video_track1.label(), "VideoHandler");
  EXPECT_EQ(video_track1.language(), "und");

  const MediaTrack& audio_track1 = *(media_tracks_->tracks()[1]);
  EXPECT_EQ(audio_track1.type(), MediaTrack::Audio);
  EXPECT_EQ(audio_track1.bytestream_track_id(), 2);
  EXPECT_EQ(audio_track1.kind(), "main");
  EXPECT_EQ(audio_track1.label(), "SoundHandler");
  EXPECT_EQ(audio_track1.language(), "und");

  const MediaTrack& video_track2 = *(media_tracks_->tracks()[2]);
  EXPECT_EQ(video_track2.type(), MediaTrack::Video);
  EXPECT_EQ(video_track2.bytestream_track_id(), 3);
  EXPECT_EQ(video_track2.kind(), "");
  EXPECT_EQ(video_track2.label(), "VideoHandler");
  EXPECT_EQ(video_track2.language(), "und");

  const MediaTrack& audio_track2 = *(media_tracks_->tracks()[3]);
  EXPECT_EQ(audio_track2.type(), MediaTrack::Audio);
  EXPECT_EQ(audio_track2.bytestream_track_id(), 4);
  EXPECT_EQ(audio_track2.kind(), "");
  EXPECT_EQ(audio_track2.label(), "SoundHandler");
  EXPECT_EQ(audio_track2.language(), "und");
}

// <cos(θ), sin(θ), θ expressed as a rotation Enum>
using MatrixRotationTestCaseParam = std::tuple<double, double, VideoRotation>;

class MP4StreamParserRotationMatrixEvaluatorTest
    : public ::testing::TestWithParam<MatrixRotationTestCaseParam> {
 public:
  MP4StreamParserRotationMatrixEvaluatorTest() {
    std::set<int> audio_object_types;
    audio_object_types.insert(kISO_14496_3);
    parser_.reset(new MP4StreamParser(audio_object_types, false, false));
  }

 protected:
  std::unique_ptr<MP4StreamParser> parser_;
};

TEST_P(MP4StreamParserRotationMatrixEvaluatorTest, RotationCalculation) {
  TrackHeader track_header;
  MovieHeader movie_header;

  // Identity matrix, with 16.16 and 2.30 fixed points.
  uint32_t identity_matrix[9] = {1 << 16, 0, 0, 0, 1 << 16, 0, 0, 0, 1 << 30};

  memcpy(movie_header.display_matrix, identity_matrix, sizeof(identity_matrix));
  memcpy(track_header.display_matrix, identity_matrix, sizeof(identity_matrix));

  MatrixRotationTestCaseParam data = GetParam();

  // Insert fixed point decimal data into the rotation matrix.
  track_header.display_matrix[0] = std::get<0>(data) * (1 << 16);
  track_header.display_matrix[4] = std::get<0>(data) * (1 << 16);
  track_header.display_matrix[1] = -(std::get<1>(data) * (1 << 16));
  track_header.display_matrix[3] = std::get<1>(data) * (1 << 16);

  EXPECT_EQ(parser_->CalculateRotation(track_header, movie_header),
            std::get<2>(data));
}

MatrixRotationTestCaseParam rotation_test_cases[6] = {
    {1, 0, VIDEO_ROTATION_0},     // cos(0)  = 1, sin(0)  = 0
    {0, -1, VIDEO_ROTATION_90},   // cos(90) = 0, sin(90) =-1
    {-1, 0, VIDEO_ROTATION_180},  // cos(180)=-1, sin(180)= 0
    {0, 1, VIDEO_ROTATION_270},   // cos(270)= 0, sin(270)= 1
    {1, 1, VIDEO_ROTATION_0},     // Error case
    {5, 5, VIDEO_ROTATION_0},     // Error case
};
INSTANTIATE_TEST_CASE_P(CheckMath,
                        MP4StreamParserRotationMatrixEvaluatorTest,
                        testing::ValuesIn(rotation_test_cases));

}  // namespace mp4
}  // namespace media
