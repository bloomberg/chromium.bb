// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_tracks.h"
#include "media/base/mock_media_log.h"
#include "media/base/test_data_util.h"
#include "media/base/text_track_config.h"
#include "media/formats/webm/webm_stream_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace media {

class WebMStreamParserTest : public testing::Test {
 public:
  WebMStreamParserTest()
      : media_log_(new testing::StrictMock<MockMediaLog>()) {}

 protected:
  void ParseWebMFile(const std::string& filename) {
    scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(filename);
    parser_.reset(new WebMStreamParser());
    Demuxer::EncryptedMediaInitDataCB encrypted_media_init_data_cb =
        base::Bind(&WebMStreamParserTest::OnEncryptedMediaInitData,
                   base::Unretained(this));

    EXPECT_CALL(*this, InitCB(_));
    EXPECT_CALL(*this, NewMediaSegmentCB()).Times(testing::AnyNumber());
    EXPECT_CALL(*this, EndMediaSegmentCB()).Times(testing::AnyNumber());
    EXPECT_CALL(*this, NewBuffersCB(_, _, _))
        .Times(testing::AnyNumber())
        .WillRepeatedly(testing::Return(true));
    parser_->Init(
        base::Bind(&WebMStreamParserTest::InitCB, base::Unretained(this)),
        base::Bind(&WebMStreamParserTest::NewConfigCB, base::Unretained(this)),
        base::Bind(&WebMStreamParserTest::NewBuffersCB, base::Unretained(this)),
        true,  // ignore_text_track
        encrypted_media_init_data_cb,
        base::Bind(&WebMStreamParserTest::NewMediaSegmentCB,
                   base::Unretained(this)),
        base::Bind(&WebMStreamParserTest::EndMediaSegmentCB,
                   base::Unretained(this)),
        media_log_);
    bool result = parser_->Parse(buffer->data(), buffer->data_size());
    EXPECT_TRUE(result);
  }

  MOCK_METHOD1(InitCB, void(const StreamParser::InitParameters& params));

  bool NewConfigCB(scoped_ptr<MediaTracks> tracks,
                   const StreamParser::TextTrackConfigMap& text_track_map) {
    DCHECK(tracks.get());
    media_tracks_ = std::move(tracks);
    return true;
  }

  MOCK_METHOD3(NewBuffersCB,
               bool(const StreamParser::BufferQueue&,
                    const StreamParser::BufferQueue&,
                    const StreamParser::TextBufferQueueMap&));
  MOCK_METHOD2(OnEncryptedMediaInitData,
               void(EmeInitDataType init_data_type,
                    const std::vector<uint8_t>& init_data));
  MOCK_METHOD0(NewMediaSegmentCB, void());
  MOCK_METHOD0(EndMediaSegmentCB, void());

  scoped_refptr<testing::StrictMock<MockMediaLog>> media_log_;
  scoped_ptr<WebMStreamParser> parser_;
  scoped_ptr<MediaTracks> media_tracks_;
};

TEST_F(WebMStreamParserTest, VerifyMediaTrackMetadata) {
  EXPECT_MEDIA_LOG(testing::HasSubstr("Estimating WebM block duration"))
      .Times(testing::AnyNumber());
  ParseWebMFile("bear.webm");
  EXPECT_NE(media_tracks_.get(), nullptr);

  EXPECT_EQ(media_tracks_->tracks().size(), 2u);

  const MediaTrack& video_track = *(media_tracks_->tracks()[0]);
  EXPECT_EQ(video_track.type(), MediaTrack::Video);
  EXPECT_EQ(video_track.id(), "1");
  EXPECT_EQ(video_track.kind(), "main");
  EXPECT_EQ(video_track.label(), "");
  EXPECT_EQ(video_track.language(), "und");

  const MediaTrack& audio_track = *(media_tracks_->tracks()[1]);
  EXPECT_EQ(audio_track.type(), MediaTrack::Audio);
  EXPECT_EQ(audio_track.id(), "2");
  EXPECT_EQ(audio_track.kind(), "main");
  EXPECT_EQ(audio_track.label(), "");
  EXPECT_EQ(audio_track.language(), "und");
}

}  // namespace media
