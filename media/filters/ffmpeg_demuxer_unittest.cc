// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <deque>
#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/threading/thread.h"
#include "media/base/decrypt_config.h"
#include "media/base/media_log.h"
#include "media/base/mock_demuxer_host.h"
#include "media/base/test_helpers.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/file_data_source.h"
#include "media/formats/mp4/avc.h"
#include "media/formats/webm/webm_crypto_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Exactly;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using ::testing::StrictMock;
using ::testing::WithArgs;
using ::testing::_;

namespace media {

MATCHER(IsEndOfStreamBuffer,
        std::string(negation ? "isn't" : "is") + " end of stream") {
  return arg->end_of_stream();
}

static void EosOnReadDone(bool* got_eos_buffer,
                          DemuxerStream::Status status,
                          const scoped_refptr<DecoderBuffer>& buffer) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());

  EXPECT_EQ(status, DemuxerStream::kOk);
  if (buffer->end_of_stream()) {
    *got_eos_buffer = true;
    return;
  }

  EXPECT_TRUE(buffer->data());
  EXPECT_GT(buffer->data_size(), 0);
  *got_eos_buffer = false;
};


// Fixture class to facilitate writing tests.  Takes care of setting up the
// FFmpeg, pipeline and filter host mocks.
class FFmpegDemuxerTest : public testing::Test {
 protected:
  FFmpegDemuxerTest() {}

  virtual ~FFmpegDemuxerTest() {
    if (demuxer_) {
      WaitableMessageLoopEvent event;
      demuxer_->Stop(event.GetClosure());
      event.RunAndWait();
    }
  }

  void CreateDemuxer(const std::string& name) {
    CHECK(!demuxer_);

    EXPECT_CALL(host_, AddBufferedTimeRange(_, _)).Times(AnyNumber());

    CreateDataSource(name);

    Demuxer::NeedKeyCB need_key_cb =
        base::Bind(&FFmpegDemuxerTest::NeedKeyCB, base::Unretained(this));

    demuxer_.reset(new FFmpegDemuxer(message_loop_.message_loop_proxy(),
                                     data_source_.get(),
                                     need_key_cb,
                                     new MediaLog()));
  }

  MOCK_METHOD1(CheckPoint, void(int v));

  void InitializeDemuxerWithTimelineOffset(bool enable_text,
                                           base::Time timeline_offset) {
    EXPECT_CALL(host_, SetDuration(_));
    WaitableMessageLoopEvent event;
    demuxer_->Initialize(&host_, event.GetPipelineStatusCB(), enable_text);
    demuxer_->timeline_offset_ = timeline_offset;
    event.RunAndWaitForStatus(PIPELINE_OK);
  }

  void InitializeDemuxerText(bool enable_text) {
    InitializeDemuxerWithTimelineOffset(enable_text, base::Time());
  }

  void InitializeDemuxer() {
    InitializeDemuxerText(false);
  }

  MOCK_METHOD2(OnReadDoneCalled, void(int, int64));

  // Verifies that |buffer| has a specific |size| and |timestamp|.
  // |location| simply indicates where the call to this function was made.
  // This makes it easier to track down where test failures occur.
  void OnReadDone(const tracked_objects::Location& location,
                  int size,
                  int64 timestamp_us,
                  base::TimeDelta discard_front_padding,
                  DemuxerStream::Status status,
                  const scoped_refptr<DecoderBuffer>& buffer) {
    std::string location_str;
    location.Write(true, false, &location_str);
    location_str += "\n";
    SCOPED_TRACE(location_str);
    EXPECT_EQ(status, DemuxerStream::kOk);
    OnReadDoneCalled(size, timestamp_us);
    EXPECT_TRUE(buffer.get() != NULL);
    EXPECT_EQ(size, buffer->data_size());
    EXPECT_EQ(timestamp_us, buffer->timestamp().InMicroseconds());
    EXPECT_EQ(discard_front_padding, buffer->discard_padding().first);
    DCHECK_EQ(&message_loop_, base::MessageLoop::current());
    message_loop_.PostTask(FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
  }

  DemuxerStream::ReadCB NewReadCB(const tracked_objects::Location& location,
                                  int size,
                                  int64 timestamp_us) {
    EXPECT_CALL(*this, OnReadDoneCalled(size, timestamp_us));
    return base::Bind(&FFmpegDemuxerTest::OnReadDone,
                      base::Unretained(this),
                      location,
                      size,
                      timestamp_us,
                      base::TimeDelta());
  }

  DemuxerStream::ReadCB NewReadCBWithCheckedDiscard(
      const tracked_objects::Location& location,
      int size,
      int64 timestamp_us,
      base::TimeDelta discard_front_padding) {
    EXPECT_CALL(*this, OnReadDoneCalled(size, timestamp_us));
    return base::Bind(&FFmpegDemuxerTest::OnReadDone,
                      base::Unretained(this),
                      location,
                      size,
                      timestamp_us,
                      discard_front_padding);
  }

  // TODO(xhwang): This is a workaround of the issue that move-only parameters
  // are not supported in mocked methods. Remove this when the issue is fixed
  // (http://code.google.com/p/googletest/issues/detail?id=395) or when we use
  // std::string instead of scoped_ptr<uint8[]> (http://crbug.com/130689).
  MOCK_METHOD3(NeedKeyCBMock, void(const std::string& type,
                                   const uint8* init_data, int init_data_size));
  void NeedKeyCB(const std::string& type,
                 const std::vector<uint8>& init_data) {
    const uint8* init_data_ptr = init_data.empty() ? NULL : &init_data[0];
    NeedKeyCBMock(type, init_data_ptr, init_data.size());
  }

  // Accessor to demuxer internals.
  void set_duration_known(bool duration_known) {
    demuxer_->duration_known_ = duration_known;
  }

  bool IsStreamStopped(DemuxerStream::Type type) {
    DemuxerStream* stream = demuxer_->GetStream(type);
    CHECK(stream);
    return !static_cast<FFmpegDemuxerStream*>(stream)->demuxer_;
  }

  // Fixture members.
  scoped_ptr<FileDataSource> data_source_;
  scoped_ptr<FFmpegDemuxer> demuxer_;
  StrictMock<MockDemuxerHost> host_;
  base::MessageLoop message_loop_;

  AVFormatContext* format_context() {
    return demuxer_->glue_->format_context();
  }

  int preferred_seeking_stream_index() const {
    return demuxer_->preferred_stream_for_seeking_.first;
  }

  void ReadUntilEndOfStream(DemuxerStream* stream) {
    bool got_eos_buffer = false;
    const int kMaxBuffers = 170;
    for (int i = 0; !got_eos_buffer && i < kMaxBuffers; i++) {
      stream->Read(base::Bind(&EosOnReadDone, &got_eos_buffer));
      message_loop_.Run();
    }

    EXPECT_TRUE(got_eos_buffer);
  }

 private:
  void CreateDataSource(const std::string& name) {
    CHECK(!data_source_);

    base::FilePath file_path;
    EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &file_path));

    file_path = file_path.Append(FILE_PATH_LITERAL("media"))
        .Append(FILE_PATH_LITERAL("test"))
        .Append(FILE_PATH_LITERAL("data"))
        .AppendASCII(name);

    data_source_.reset(new FileDataSource());
    EXPECT_TRUE(data_source_->Initialize(file_path));
  }

  DISALLOW_COPY_AND_ASSIGN(FFmpegDemuxerTest);
};

TEST_F(FFmpegDemuxerTest, Initialize_OpenFails) {
  // Simulate avformat_open_input() failing.
  CreateDemuxer("ten_byte_file");
  WaitableMessageLoopEvent event;
  demuxer_->Initialize(&host_, event.GetPipelineStatusCB(), true);
  event.RunAndWaitForStatus(DEMUXER_ERROR_COULD_NOT_OPEN);
}

// TODO(acolwell): Uncomment this test when we discover a file that passes
// avformat_open_input(), but has avformat_find_stream_info() fail.
//
//TEST_F(FFmpegDemuxerTest, Initialize_ParseFails) {
//  ("find_stream_info_fail.webm");
//  demuxer_->Initialize(
//      &host_, NewExpectedStatusCB(DEMUXER_ERROR_COULD_NOT_PARSE));
//  message_loop_.RunUntilIdle();
//}

TEST_F(FFmpegDemuxerTest, Initialize_NoStreams) {
  // Open a file with no streams whatsoever.
  CreateDemuxer("no_streams.webm");
  WaitableMessageLoopEvent event;
  demuxer_->Initialize(&host_, event.GetPipelineStatusCB(), true);
  event.RunAndWaitForStatus(DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
}

TEST_F(FFmpegDemuxerTest, Initialize_NoAudioVideo) {
  // Open a file containing streams but none of which are audio/video streams.
  CreateDemuxer("no_audio_video.webm");
  WaitableMessageLoopEvent event;
  demuxer_->Initialize(&host_, event.GetPipelineStatusCB(), true);
  event.RunAndWaitForStatus(DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
}

TEST_F(FFmpegDemuxerTest, Initialize_Successful) {
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();

  // Video stream should be present.
  DemuxerStream* stream = demuxer_->GetStream(DemuxerStream::VIDEO);
  ASSERT_TRUE(stream);
  EXPECT_EQ(DemuxerStream::VIDEO, stream->type());

  const VideoDecoderConfig& video_config = stream->video_decoder_config();
  EXPECT_EQ(kCodecVP8, video_config.codec());
  EXPECT_EQ(VideoFrame::YV12, video_config.format());
  EXPECT_EQ(320, video_config.coded_size().width());
  EXPECT_EQ(240, video_config.coded_size().height());
  EXPECT_EQ(0, video_config.visible_rect().x());
  EXPECT_EQ(0, video_config.visible_rect().y());
  EXPECT_EQ(320, video_config.visible_rect().width());
  EXPECT_EQ(240, video_config.visible_rect().height());
  EXPECT_EQ(320, video_config.natural_size().width());
  EXPECT_EQ(240, video_config.natural_size().height());
  EXPECT_FALSE(video_config.extra_data());
  EXPECT_EQ(0u, video_config.extra_data_size());

  // Audio stream should be present.
  stream = demuxer_->GetStream(DemuxerStream::AUDIO);
  ASSERT_TRUE(stream);
  EXPECT_EQ(DemuxerStream::AUDIO, stream->type());

  const AudioDecoderConfig& audio_config = stream->audio_decoder_config();
  EXPECT_EQ(kCodecVorbis, audio_config.codec());
  EXPECT_EQ(32, audio_config.bits_per_channel());
  EXPECT_EQ(CHANNEL_LAYOUT_STEREO, audio_config.channel_layout());
  EXPECT_EQ(44100, audio_config.samples_per_second());
  EXPECT_EQ(kSampleFormatPlanarF32, audio_config.sample_format());
  EXPECT_TRUE(audio_config.extra_data());
  EXPECT_GT(audio_config.extra_data_size(), 0u);

  // Unknown stream should never be present.
  EXPECT_FALSE(demuxer_->GetStream(DemuxerStream::UNKNOWN));
}

TEST_F(FFmpegDemuxerTest, Initialize_Multitrack) {
  // Open a file containing the following streams:
  //   Stream #0: Video (VP8)
  //   Stream #1: Audio (Vorbis)
  //   Stream #2: Subtitles (SRT)
  //   Stream #3: Video (Theora)
  //   Stream #4: Audio (16-bit signed little endian PCM)
  //
  // We should only pick the first audio/video streams we come across.
  CreateDemuxer("bear-320x240-multitrack.webm");
  InitializeDemuxer();

  // Video stream should be VP8.
  DemuxerStream* stream = demuxer_->GetStream(DemuxerStream::VIDEO);
  ASSERT_TRUE(stream);
  EXPECT_EQ(DemuxerStream::VIDEO, stream->type());
  EXPECT_EQ(kCodecVP8, stream->video_decoder_config().codec());

  // Audio stream should be Vorbis.
  stream = demuxer_->GetStream(DemuxerStream::AUDIO);
  ASSERT_TRUE(stream);
  EXPECT_EQ(DemuxerStream::AUDIO, stream->type());
  EXPECT_EQ(kCodecVorbis, stream->audio_decoder_config().codec());

  // Unknown stream should never be present.
  EXPECT_FALSE(demuxer_->GetStream(DemuxerStream::UNKNOWN));
}

TEST_F(FFmpegDemuxerTest, Initialize_MultitrackText) {
  // Open a file containing the following streams:
  //   Stream #0: Video (VP8)
  //   Stream #1: Audio (Vorbis)
  //   Stream #2: Text (WebVTT)

  CreateDemuxer("bear-vp8-webvtt.webm");
  DemuxerStream* text_stream = NULL;
  EXPECT_CALL(host_, AddTextStream(_, _))
      .WillOnce(SaveArg<0>(&text_stream));
  InitializeDemuxerText(true);
  ASSERT_TRUE(text_stream);
  EXPECT_EQ(DemuxerStream::TEXT, text_stream->type());

  // Video stream should be VP8.
  DemuxerStream* stream = demuxer_->GetStream(DemuxerStream::VIDEO);
  ASSERT_TRUE(stream);
  EXPECT_EQ(DemuxerStream::VIDEO, stream->type());
  EXPECT_EQ(kCodecVP8, stream->video_decoder_config().codec());

  // Audio stream should be Vorbis.
  stream = demuxer_->GetStream(DemuxerStream::AUDIO);
  ASSERT_TRUE(stream);
  EXPECT_EQ(DemuxerStream::AUDIO, stream->type());
  EXPECT_EQ(kCodecVorbis, stream->audio_decoder_config().codec());

  // Unknown stream should never be present.
  EXPECT_FALSE(demuxer_->GetStream(DemuxerStream::UNKNOWN));
}

TEST_F(FFmpegDemuxerTest, Initialize_Encrypted) {
  EXPECT_CALL(*this, NeedKeyCBMock(kWebMEncryptInitDataType, NotNull(),
                                   DecryptConfig::kDecryptionKeySize))
      .Times(Exactly(2));

  CreateDemuxer("bear-320x240-av_enc-av.webm");
  InitializeDemuxer();
}

TEST_F(FFmpegDemuxerTest, Read_Audio) {
  // We test that on a successful audio packet read.
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();

  // Attempt a read from the audio stream and run the message loop until done.
  DemuxerStream* audio = demuxer_->GetStream(DemuxerStream::AUDIO);

  audio->Read(NewReadCB(FROM_HERE, 29, 0));
  message_loop_.Run();

  audio->Read(NewReadCB(FROM_HERE, 27, 3000));
  message_loop_.Run();
}

TEST_F(FFmpegDemuxerTest, Read_Video) {
  // We test that on a successful video packet read.
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();

  // Attempt a read from the video stream and run the message loop until done.
  DemuxerStream* video = demuxer_->GetStream(DemuxerStream::VIDEO);

  video->Read(NewReadCB(FROM_HERE, 22084, 0));
  message_loop_.Run();

  video->Read(NewReadCB(FROM_HERE, 1057, 33000));
  message_loop_.Run();
}

TEST_F(FFmpegDemuxerTest, Read_Text) {
  // We test that on a successful text packet read.
  CreateDemuxer("bear-vp8-webvtt.webm");
  DemuxerStream* text_stream = NULL;
  EXPECT_CALL(host_, AddTextStream(_, _))
      .WillOnce(SaveArg<0>(&text_stream));
  InitializeDemuxerText(true);
  ASSERT_TRUE(text_stream);
  EXPECT_EQ(DemuxerStream::TEXT, text_stream->type());

  text_stream->Read(NewReadCB(FROM_HERE, 31, 0));
  message_loop_.Run();

  text_stream->Read(NewReadCB(FROM_HERE, 19, 500000));
  message_loop_.Run();
}

TEST_F(FFmpegDemuxerTest, SeekInitialized_NoVideoStartTime) {
  CreateDemuxer("audio-start-time-only.webm");
  InitializeDemuxer();
  EXPECT_EQ(0, preferred_seeking_stream_index());
}

TEST_F(FFmpegDemuxerTest, Read_VideoPositiveStartTime) {
  const int64 kTimelineOffsetMs = 1352550896000LL;

  // Test the start time is the first timestamp of the video and audio stream.
  CreateDemuxer("nonzero-start-time.webm");
  InitializeDemuxerWithTimelineOffset(
      false, base::Time::FromJsTime(kTimelineOffsetMs));

  // Attempt a read from the video stream and run the message loop until done.
  DemuxerStream* video = demuxer_->GetStream(DemuxerStream::VIDEO);
  DemuxerStream* audio = demuxer_->GetStream(DemuxerStream::AUDIO);

  const base::TimeDelta video_start_time =
      base::TimeDelta::FromMicroseconds(400000);
  const base::TimeDelta audio_start_time =
      base::TimeDelta::FromMicroseconds(396000);

  // Run the test twice with a seek in between.
  for (int i = 0; i < 2; ++i) {
    // Check first buffer in video stream.  It should have been adjusted such
    // that it starts 400ms after the first audio buffer.
    video->Read(
        NewReadCB(FROM_HERE,
                  5636,
                  (video_start_time - audio_start_time).InMicroseconds()));
    message_loop_.Run();

    // Since the audio buffer has a lower first timestamp, it should become
    // zero.
    audio->Read(NewReadCB(FROM_HERE, 165, 0));
    message_loop_.Run();

    // Verify that the start time is equal to the lowest timestamp (ie the
    // audio).
    EXPECT_EQ(audio_start_time, demuxer_->start_time());

    // Verify that the timeline offset has been adjusted by the start time.
    EXPECT_EQ(kTimelineOffsetMs + audio_start_time.InMilliseconds(),
              demuxer_->GetTimelineOffset().ToJavaTime());

    // Seek back to the beginning and repeat the test.
    WaitableMessageLoopEvent event;
    demuxer_->Seek(base::TimeDelta(), event.GetPipelineStatusCB());
    event.RunAndWaitForStatus(PIPELINE_OK);
  }
}

TEST_F(FFmpegDemuxerTest, Read_AudioNoStartTime) {
  // FFmpeg does not set timestamps when demuxing wave files.  Ensure that the
  // demuxer sets a start time of zero in this case.
  CreateDemuxer("sfx_s24le.wav");
  InitializeDemuxer();

  // Run the test twice with a seek in between.
  for (int i = 0; i < 2; ++i) {
    demuxer_->GetStream(DemuxerStream::AUDIO)
        ->Read(NewReadCB(FROM_HERE, 4095, 0));
    message_loop_.Run();
    EXPECT_EQ(base::TimeDelta(), demuxer_->start_time());

    // Seek back to the beginning and repeat the test.
    WaitableMessageLoopEvent event;
    demuxer_->Seek(base::TimeDelta(), event.GetPipelineStatusCB());
    event.RunAndWaitForStatus(PIPELINE_OK);
  }
}

// TODO(dalecurtis): Test is disabled since FFmpeg does not currently guarantee
// the order of demuxed packets in OGG containers.  Re-enable once we decide to
// either workaround it or attempt a fix upstream.  See http://crbug.com/387996.
TEST_F(FFmpegDemuxerTest,
       DISABLED_Read_AudioNegativeStartTimeAndOggDiscard_Bear) {
  // Many ogg files have negative starting timestamps, so ensure demuxing and
  // seeking work correctly with a negative start time.
  CreateDemuxer("bear.ogv");
  InitializeDemuxer();

  // Attempt a read from the video stream and run the message loop until done.
  DemuxerStream* video = demuxer_->GetStream(DemuxerStream::VIDEO);
  DemuxerStream* audio = demuxer_->GetStream(DemuxerStream::AUDIO);

  // Run the test twice with a seek in between.
  for (int i = 0; i < 2; ++i) {
    audio->Read(
        NewReadCBWithCheckedDiscard(FROM_HERE, 40, 0, kInfiniteDuration()));
    message_loop_.Run();
    audio->Read(
        NewReadCBWithCheckedDiscard(FROM_HERE, 41, 2903, kInfiniteDuration()));
    message_loop_.Run();
    audio->Read(NewReadCBWithCheckedDiscard(
        FROM_HERE, 173, 5805, base::TimeDelta::FromMicroseconds(10159)));
    message_loop_.Run();

    audio->Read(NewReadCB(FROM_HERE, 148, 18866));
    message_loop_.Run();
    EXPECT_EQ(base::TimeDelta::FromMicroseconds(-15964),
              demuxer_->start_time());

    video->Read(NewReadCB(FROM_HERE, 5751, 0));
    message_loop_.Run();

    video->Read(NewReadCB(FROM_HERE, 846, 33367));
    message_loop_.Run();

    video->Read(NewReadCB(FROM_HERE, 1255, 66733));
    message_loop_.Run();

    // Seek back to the beginning and repeat the test.
    WaitableMessageLoopEvent event;
    demuxer_->Seek(base::TimeDelta(), event.GetPipelineStatusCB());
    event.RunAndWaitForStatus(PIPELINE_OK);
  }
}

// Same test above, but using sync2.ogv which has video stream muxed before the
// audio stream, so seeking based only on start time will fail since ffmpeg is
// essentially just seeking based on file position.
TEST_F(FFmpegDemuxerTest, Read_AudioNegativeStartTimeAndOggDiscard_Sync) {
  // Many ogg files have negative starting timestamps, so ensure demuxing and
  // seeking work correctly with a negative start time.
  CreateDemuxer("sync2.ogv");
  InitializeDemuxer();

  // Attempt a read from the video stream and run the message loop until done.
  DemuxerStream* video = demuxer_->GetStream(DemuxerStream::VIDEO);
  DemuxerStream* audio = demuxer_->GetStream(DemuxerStream::AUDIO);

  // Run the test twice with a seek in between.
  for (int i = 0; i < 2; ++i) {
    audio->Read(NewReadCBWithCheckedDiscard(
        FROM_HERE, 1, 0, base::TimeDelta::FromMicroseconds(2902)));
    message_loop_.Run();

    audio->Read(NewReadCB(FROM_HERE, 1, 2902));
    message_loop_.Run();
    EXPECT_EQ(base::TimeDelta::FromMicroseconds(-2902),
              demuxer_->start_time());

    video->Read(NewReadCB(FROM_HERE, 9997, 0));
    message_loop_.Run();

    video->Read(NewReadCB(FROM_HERE, 16, 33241));
    message_loop_.Run();

    video->Read(NewReadCB(FROM_HERE, 631, 66482));
    message_loop_.Run();

    // Seek back to the beginning and repeat the test.
    WaitableMessageLoopEvent event;
    demuxer_->Seek(base::TimeDelta(), event.GetPipelineStatusCB());
    event.RunAndWaitForStatus(PIPELINE_OK);
  }
}

TEST_F(FFmpegDemuxerTest, Read_EndOfStream) {
  // Verify that end of stream buffers are created.
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();
  ReadUntilEndOfStream(demuxer_->GetStream(DemuxerStream::AUDIO));
}

TEST_F(FFmpegDemuxerTest, Read_EndOfStreamText) {
  // Verify that end of stream buffers are created.
  CreateDemuxer("bear-vp8-webvtt.webm");
  DemuxerStream* text_stream = NULL;
  EXPECT_CALL(host_, AddTextStream(_, _))
      .WillOnce(SaveArg<0>(&text_stream));
  InitializeDemuxerText(true);
  ASSERT_TRUE(text_stream);
  EXPECT_EQ(DemuxerStream::TEXT, text_stream->type());

  bool got_eos_buffer = false;
  const int kMaxBuffers = 10;
  for (int i = 0; !got_eos_buffer && i < kMaxBuffers; i++) {
    text_stream->Read(base::Bind(&EosOnReadDone, &got_eos_buffer));
    message_loop_.Run();
  }

  EXPECT_TRUE(got_eos_buffer);
}

TEST_F(FFmpegDemuxerTest, Read_EndOfStream_NoDuration) {
  // Verify that end of stream buffers are created.
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();
  set_duration_known(false);
  EXPECT_CALL(host_, SetDuration(base::TimeDelta::FromMilliseconds(2767)));
  ReadUntilEndOfStream(demuxer_->GetStream(DemuxerStream::AUDIO));
  ReadUntilEndOfStream(demuxer_->GetStream(DemuxerStream::VIDEO));
}

TEST_F(FFmpegDemuxerTest, Read_EndOfStream_NoDuration_VideoOnly) {
  // Verify that end of stream buffers are created.
  CreateDemuxer("bear-320x240-video-only.webm");
  InitializeDemuxer();
  set_duration_known(false);
  EXPECT_CALL(host_, SetDuration(base::TimeDelta::FromMilliseconds(2703)));
  ReadUntilEndOfStream(demuxer_->GetStream(DemuxerStream::VIDEO));
}

TEST_F(FFmpegDemuxerTest, Read_EndOfStream_NoDuration_AudioOnly) {
  // Verify that end of stream buffers are created.
  CreateDemuxer("bear-320x240-audio-only.webm");
  InitializeDemuxer();
  set_duration_known(false);
  EXPECT_CALL(host_, SetDuration(base::TimeDelta::FromMilliseconds(2767)));
  ReadUntilEndOfStream(demuxer_->GetStream(DemuxerStream::AUDIO));
}

TEST_F(FFmpegDemuxerTest, Read_EndOfStream_NoDuration_UnsupportedStream) {
  // Verify that end of stream buffers are created and we don't crash
  // if there are streams in the file that we don't support.
  CreateDemuxer("vorbis_audio_wmv_video.mkv");
  InitializeDemuxer();
  set_duration_known(false);
  EXPECT_CALL(host_, SetDuration(base::TimeDelta::FromMilliseconds(1014)));
  ReadUntilEndOfStream(demuxer_->GetStream(DemuxerStream::AUDIO));
}

TEST_F(FFmpegDemuxerTest, Seek) {
  // We're testing that the demuxer frees all queued packets when it receives
  // a Seek().
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();

  // Get our streams.
  DemuxerStream* video = demuxer_->GetStream(DemuxerStream::VIDEO);
  DemuxerStream* audio = demuxer_->GetStream(DemuxerStream::AUDIO);
  ASSERT_TRUE(video);
  ASSERT_TRUE(audio);

  // Read a video packet and release it.
  video->Read(NewReadCB(FROM_HERE, 22084, 0));
  message_loop_.Run();

  // Issue a simple forward seek, which should discard queued packets.
  WaitableMessageLoopEvent event;
  demuxer_->Seek(base::TimeDelta::FromMicroseconds(1000000),
                 event.GetPipelineStatusCB());
  event.RunAndWaitForStatus(PIPELINE_OK);

  // Audio read #1.
  audio->Read(NewReadCB(FROM_HERE, 145, 803000));
  message_loop_.Run();

  // Audio read #2.
  audio->Read(NewReadCB(FROM_HERE, 148, 826000));
  message_loop_.Run();

  // Video read #1.
  video->Read(NewReadCB(FROM_HERE, 5425, 801000));
  message_loop_.Run();

  // Video read #2.
  video->Read(NewReadCB(FROM_HERE, 1906, 834000));
  message_loop_.Run();
}

TEST_F(FFmpegDemuxerTest, SeekText) {
  // We're testing that the demuxer frees all queued packets when it receives
  // a Seek().
  CreateDemuxer("bear-vp8-webvtt.webm");
  DemuxerStream* text_stream = NULL;
  EXPECT_CALL(host_, AddTextStream(_, _))
      .WillOnce(SaveArg<0>(&text_stream));
  InitializeDemuxerText(true);
  ASSERT_TRUE(text_stream);
  EXPECT_EQ(DemuxerStream::TEXT, text_stream->type());

  // Get our streams.
  DemuxerStream* video = demuxer_->GetStream(DemuxerStream::VIDEO);
  DemuxerStream* audio = demuxer_->GetStream(DemuxerStream::AUDIO);
  ASSERT_TRUE(video);
  ASSERT_TRUE(audio);

  // Read a text packet and release it.
  text_stream->Read(NewReadCB(FROM_HERE, 31, 0));
  message_loop_.Run();

  // Issue a simple forward seek, which should discard queued packets.
  WaitableMessageLoopEvent event;
  demuxer_->Seek(base::TimeDelta::FromMicroseconds(1000000),
                 event.GetPipelineStatusCB());
  event.RunAndWaitForStatus(PIPELINE_OK);

  // Audio read #1.
  audio->Read(NewReadCB(FROM_HERE, 145, 803000));
  message_loop_.Run();

  // Audio read #2.
  audio->Read(NewReadCB(FROM_HERE, 148, 826000));
  message_loop_.Run();

  // Video read #1.
  video->Read(NewReadCB(FROM_HERE, 5425, 801000));
  message_loop_.Run();

  // Video read #2.
  video->Read(NewReadCB(FROM_HERE, 1906, 834000));
  message_loop_.Run();

  // Text read #1.
  text_stream->Read(NewReadCB(FROM_HERE, 19, 500000));
  message_loop_.Run();

  // Text read #2.
  text_stream->Read(NewReadCB(FROM_HERE, 19, 1000000));
  message_loop_.Run();
}

class MockReadCB {
 public:
  MockReadCB() {}
  ~MockReadCB() {}

  MOCK_METHOD2(Run, void(DemuxerStream::Status status,
                         const scoped_refptr<DecoderBuffer>& buffer));
 private:
  DISALLOW_COPY_AND_ASSIGN(MockReadCB);
};

TEST_F(FFmpegDemuxerTest, Stop) {
  // Tests that calling Read() on a stopped demuxer stream immediately deletes
  // the callback.
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();

  // Get our stream.
  DemuxerStream* audio = demuxer_->GetStream(DemuxerStream::AUDIO);
  ASSERT_TRUE(audio);

  WaitableMessageLoopEvent event;
  demuxer_->Stop(event.GetClosure());
  event.RunAndWait();

  // Reads after being stopped are all EOS buffers.
  StrictMock<MockReadCB> callback;
  EXPECT_CALL(callback, Run(DemuxerStream::kOk, IsEndOfStreamBuffer()));

  // Attempt the read...
  audio->Read(base::Bind(&MockReadCB::Run, base::Unretained(&callback)));
  message_loop_.RunUntilIdle();

  // Don't let the test call Stop() again.
  demuxer_.reset();
}

// Verify that seek works properly when the WebM cues data is at the start of
// the file instead of at the end.
TEST_F(FFmpegDemuxerTest, SeekWithCuesBeforeFirstCluster) {
  CreateDemuxer("bear-320x240-cues-in-front.webm");
  InitializeDemuxer();

  // Get our streams.
  DemuxerStream* video = demuxer_->GetStream(DemuxerStream::VIDEO);
  DemuxerStream* audio = demuxer_->GetStream(DemuxerStream::AUDIO);
  ASSERT_TRUE(video);
  ASSERT_TRUE(audio);

  // Read a video packet and release it.
  video->Read(NewReadCB(FROM_HERE, 22084, 0));
  message_loop_.Run();

  // Issue a simple forward seek, which should discard queued packets.
  WaitableMessageLoopEvent event;
  demuxer_->Seek(base::TimeDelta::FromMicroseconds(2500000),
                 event.GetPipelineStatusCB());
  event.RunAndWaitForStatus(PIPELINE_OK);

  // Audio read #1.
  audio->Read(NewReadCB(FROM_HERE, 40, 2403000));
  message_loop_.Run();

  // Audio read #2.
  audio->Read(NewReadCB(FROM_HERE, 42, 2406000));
  message_loop_.Run();

  // Video read #1.
  video->Read(NewReadCB(FROM_HERE, 5276, 2402000));
  message_loop_.Run();

  // Video read #2.
  video->Read(NewReadCB(FROM_HERE, 1740, 2436000));
  message_loop_.Run();
}

#if defined(USE_PROPRIETARY_CODECS)
// Ensure ID3v1 tag reading is disabled.  id3_test.mp3 has an ID3v1 tag with the
// field "title" set to "sample for id3 test".
TEST_F(FFmpegDemuxerTest, NoID3TagData) {
  CreateDemuxer("id3_test.mp3");
  InitializeDemuxer();
  EXPECT_FALSE(av_dict_get(format_context()->metadata, "title", NULL, 0));
}
#endif

#if defined(USE_PROPRIETARY_CODECS)
// Ensure MP3 files with large image/video based ID3 tags demux okay.  FFmpeg
// will hand us a video stream to the data which will likely be in a format we
// don't accept as video; e.g. PNG.
TEST_F(FFmpegDemuxerTest, Mp3WithVideoStreamID3TagData) {
  CreateDemuxer("id3_png_test.mp3");
  InitializeDemuxer();

  // Ensure the expected streams are present.
  EXPECT_FALSE(demuxer_->GetStream(DemuxerStream::VIDEO));
  EXPECT_TRUE(demuxer_->GetStream(DemuxerStream::AUDIO));
}
#endif

// Ensure a video with an unsupported audio track still results in the video
// stream being demuxed.
TEST_F(FFmpegDemuxerTest, UnsupportedAudioSupportedVideoDemux) {
  CreateDemuxer("speex_audio_vorbis_video.ogv");
  InitializeDemuxer();

  // Ensure the expected streams are present.
  EXPECT_TRUE(demuxer_->GetStream(DemuxerStream::VIDEO));
  EXPECT_FALSE(demuxer_->GetStream(DemuxerStream::AUDIO));
}

// Ensure a video with an unsupported video track still results in the audio
// stream being demuxed.
TEST_F(FFmpegDemuxerTest, UnsupportedVideoSupportedAudioDemux) {
  CreateDemuxer("vorbis_audio_wmv_video.mkv");
  InitializeDemuxer();

  // Ensure the expected streams are present.
  EXPECT_FALSE(demuxer_->GetStream(DemuxerStream::VIDEO));
  EXPECT_TRUE(demuxer_->GetStream(DemuxerStream::AUDIO));
}

#if defined(USE_PROPRIETARY_CODECS)
// FFmpeg returns null data pointers when samples have zero size, leading to
// mistakenly creating end of stream buffers http://crbug.com/169133
TEST_F(FFmpegDemuxerTest, MP4_ZeroStszEntry) {
  CreateDemuxer("bear-1280x720-zero-stsz-entry.mp4");
  InitializeDemuxer();
  ReadUntilEndOfStream(demuxer_->GetStream(DemuxerStream::AUDIO));
}


static void ValidateAnnexB(DemuxerStream* stream,
                           DemuxerStream::Status status,
                           const scoped_refptr<DecoderBuffer>& buffer) {
  EXPECT_EQ(status, DemuxerStream::kOk);

  if (buffer->end_of_stream()) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
    return;
  }

  bool is_valid =
      mp4::AVC::IsValidAnnexB(buffer->data(), buffer->data_size());
  EXPECT_TRUE(is_valid);

  if (!is_valid) {
    LOG(ERROR) << "Buffer contains invalid Annex B data.";
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
    return;
  }

  stream->Read(base::Bind(&ValidateAnnexB, stream));
};

TEST_F(FFmpegDemuxerTest, IsValidAnnexB) {
  const char* files[] = {
    "bear-1280x720-av_frag.mp4",
    "bear-1280x720-av_with-aud-nalus_frag.mp4"
  };

  for (size_t i = 0; i < arraysize(files); ++i) {
    DVLOG(1) << "Testing " << files[i];
    CreateDemuxer(files[i]);
    InitializeDemuxer();

    // Ensure the expected streams are present.
    DemuxerStream* stream = demuxer_->GetStream(DemuxerStream::VIDEO);
    ASSERT_TRUE(stream);
    stream->EnableBitstreamConverter();

    stream->Read(base::Bind(&ValidateAnnexB, stream));
    message_loop_.Run();

    WaitableMessageLoopEvent event;
    demuxer_->Stop(event.GetClosure());
    event.RunAndWait();
    demuxer_.reset();
    data_source_.reset();
  }
}

TEST_F(FFmpegDemuxerTest, Rotate_Metadata_0) {
  CreateDemuxer("bear_rotate_0.mp4");
  InitializeDemuxer();

  DemuxerStream* stream = demuxer_->GetStream(DemuxerStream::VIDEO);
  ASSERT_TRUE(stream);
  ASSERT_EQ(VIDEO_ROTATION_0, stream->video_rotation());
}

TEST_F(FFmpegDemuxerTest, Rotate_Metadata_90) {
  CreateDemuxer("bear_rotate_90.mp4");
  InitializeDemuxer();

  DemuxerStream* stream = demuxer_->GetStream(DemuxerStream::VIDEO);
  ASSERT_TRUE(stream);
  ASSERT_EQ(VIDEO_ROTATION_90, stream->video_rotation());
}

TEST_F(FFmpegDemuxerTest, Rotate_Metadata_180) {
  CreateDemuxer("bear_rotate_180.mp4");
  InitializeDemuxer();

  DemuxerStream* stream = demuxer_->GetStream(DemuxerStream::VIDEO);
  ASSERT_TRUE(stream);
  ASSERT_EQ(VIDEO_ROTATION_180, stream->video_rotation());
}

TEST_F(FFmpegDemuxerTest, Rotate_Metadata_270) {
  CreateDemuxer("bear_rotate_270.mp4");
  InitializeDemuxer();

  DemuxerStream* stream = demuxer_->GetStream(DemuxerStream::VIDEO);
  ASSERT_TRUE(stream);
  ASSERT_EQ(VIDEO_ROTATION_270, stream->video_rotation());
}

#endif

}  // namespace media
