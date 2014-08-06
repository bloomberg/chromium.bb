// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/simple_thread.h"
#include "base/time/clock.h"
#include "media/base/fake_text_track_stream.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/media_log.h"
#include "media/base/mock_filters.h"
#include "media/base/pipeline.h"
#include "media/base/test_helpers.h"
#include "media/base/text_renderer.h"
#include "media/base/text_track_config.h"
#include "media/base/time_delta_interpolator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DeleteArg;
using ::testing::DoAll;
// TODO(scherkus): Remove InSequence after refactoring Pipeline.
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace media {

ACTION_P(SetDemuxerProperties, duration) {
  arg0->SetDuration(duration);
}

ACTION_P2(Stop, pipeline, stop_cb) {
  pipeline->Stop(stop_cb);
}

ACTION_P2(SetError, pipeline, status) {
  pipeline->SetErrorForTesting(status);
}

ACTION_P2(SetBufferingState, cb, buffering_state) {
  cb->Run(buffering_state);
}

// Used for setting expectations on pipeline callbacks.  Using a StrictMock
// also lets us test for missing callbacks.
class CallbackHelper {
 public:
  CallbackHelper() {}
  virtual ~CallbackHelper() {}

  MOCK_METHOD1(OnStart, void(PipelineStatus));
  MOCK_METHOD1(OnSeek, void(PipelineStatus));
  MOCK_METHOD0(OnStop, void());
  MOCK_METHOD0(OnEnded, void());
  MOCK_METHOD1(OnError, void(PipelineStatus));
  MOCK_METHOD1(OnMetadata, void(PipelineMetadata));
  MOCK_METHOD1(OnBufferingStateChange, void(BufferingState));
  MOCK_METHOD0(OnDurationChange, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(CallbackHelper);
};

// TODO(scherkus): even though some filters are initialized on separate
// threads these test aren't flaky... why?  It's because filters' Initialize()
// is executed on |message_loop_| and the mock filters instantly call
// InitializationComplete(), which keeps the pipeline humming along.  If
// either filters don't call InitializationComplete() immediately or filter
// initialization is moved to a separate thread this test will become flaky.
class PipelineTest : public ::testing::Test {
 public:
  PipelineTest()
      : pipeline_(new Pipeline(message_loop_.message_loop_proxy(),
                               new MediaLog())),
        filter_collection_(new FilterCollection()),
        demuxer_(new StrictMock<MockDemuxer>()) {
    filter_collection_->SetDemuxer(demuxer_.get());

    video_renderer_ = new StrictMock<MockVideoRenderer>();
    scoped_ptr<VideoRenderer> video_renderer(video_renderer_);
    filter_collection_->SetVideoRenderer(video_renderer.Pass());

    audio_renderer_ = new StrictMock<MockAudioRenderer>();
    scoped_ptr<AudioRenderer> audio_renderer(audio_renderer_);
    filter_collection_->SetAudioRenderer(audio_renderer.Pass());

    text_renderer_ = new TextRenderer(
                         message_loop_.message_loop_proxy(),
                         base::Bind(&PipelineTest::OnAddTextTrack,
                                    base::Unretained(this)));
    scoped_ptr<TextRenderer> text_renderer(text_renderer_);
    filter_collection_->SetTextRenderer(text_renderer.Pass());

    // SetDemuxerExpectations() adds overriding expectations for expected
    // non-NULL streams.
    DemuxerStream* null_pointer = NULL;
    EXPECT_CALL(*demuxer_, GetStream(_))
        .WillRepeatedly(Return(null_pointer));

    EXPECT_CALL(*demuxer_, GetTimelineOffset())
        .WillRepeatedly(Return(base::Time()));

    EXPECT_CALL(*demuxer_, GetLiveness())
        .WillRepeatedly(Return(Demuxer::LIVENESS_UNKNOWN));
  }

  virtual ~PipelineTest() {
    if (!pipeline_ || !pipeline_->IsRunning())
      return;

    ExpectDemuxerStop();

    // The mock demuxer doesn't stop the fake text track stream,
    // so just stop it manually.
    if (text_stream_) {
      text_stream_->Stop();
      message_loop_.RunUntilIdle();
    }

    // Expect a stop callback if we were started.
    ExpectPipelineStopAndDestroyPipeline();
    pipeline_->Stop(base::Bind(&CallbackHelper::OnStop,
                               base::Unretained(&callbacks_)));
    message_loop_.RunUntilIdle();
  }

  void OnDemuxerError() {
    // Cast because OnDemuxerError is private in Pipeline.
    static_cast<DemuxerHost*>(pipeline_.get())
        ->OnDemuxerError(PIPELINE_ERROR_ABORT);
  }

 protected:
  // Sets up expectations to allow the demuxer to initialize.
  typedef std::vector<MockDemuxerStream*> MockDemuxerStreamVector;
  void SetDemuxerExpectations(MockDemuxerStreamVector* streams,
                              const base::TimeDelta& duration) {
    EXPECT_CALL(callbacks_, OnDurationChange());
    EXPECT_CALL(*demuxer_, Initialize(_, _, _))
        .WillOnce(DoAll(SetDemuxerProperties(duration),
                        RunCallback<1>(PIPELINE_OK)));

    // Configure the demuxer to return the streams.
    for (size_t i = 0; i < streams->size(); ++i) {
      DemuxerStream* stream = (*streams)[i];
      EXPECT_CALL(*demuxer_, GetStream(stream->type()))
          .WillRepeatedly(Return(stream));
    }
  }

  void SetDemuxerExpectations(MockDemuxerStreamVector* streams) {
    // Initialize with a default non-zero duration.
    SetDemuxerExpectations(streams, base::TimeDelta::FromSeconds(10));
  }

  scoped_ptr<StrictMock<MockDemuxerStream> > CreateStream(
      DemuxerStream::Type type) {
    scoped_ptr<StrictMock<MockDemuxerStream> > stream(
        new StrictMock<MockDemuxerStream>(type));
    return stream.Pass();
  }

  // Sets up expectations to allow the video renderer to initialize.
  void SetVideoRendererExpectations(DemuxerStream* stream) {
    EXPECT_CALL(*video_renderer_, Initialize(stream, _, _, _, _, _, _, _, _, _))
        .WillOnce(DoAll(SaveArg<5>(&video_buffering_state_cb_),
                        SaveArg<6>(&video_ended_cb_),
                        RunCallback<2>(PIPELINE_OK)));
  }

  // Sets up expectations to allow the audio renderer to initialize.
  void SetAudioRendererExpectations(DemuxerStream* stream) {
    EXPECT_CALL(*audio_renderer_, Initialize(stream, _, _, _, _, _, _))
        .WillOnce(DoAll(SaveArg<3>(&audio_time_cb_),
                        SaveArg<4>(&audio_buffering_state_cb_),
                        SaveArg<5>(&audio_ended_cb_),
                        RunCallback<1>(PIPELINE_OK)));
  }

  void AddTextStream() {
    EXPECT_CALL(*this, OnAddTextTrack(_,_))
        .WillOnce(Invoke(this, &PipelineTest::DoOnAddTextTrack));
    static_cast<DemuxerHost*>(pipeline_.get())->AddTextStream(text_stream(),
                              TextTrackConfig(kTextSubtitles, "", "", ""));
  }

  // Sets up expectations on the callback and initializes the pipeline. Called
  // after tests have set expectations any filters they wish to use.
  void StartPipeline(PipelineStatus start_status) {
    EXPECT_CALL(callbacks_, OnStart(start_status));

    if (start_status == PIPELINE_OK) {
      EXPECT_CALL(callbacks_, OnMetadata(_)).WillOnce(SaveArg<0>(&metadata_));

      if (audio_stream_) {
        EXPECT_CALL(*audio_renderer_, GetTimeSource())
            .WillOnce(Return(&time_source_));
        EXPECT_CALL(time_source_, SetPlaybackRate(0.0f));
        EXPECT_CALL(time_source_, SetMediaTime(base::TimeDelta()));
        EXPECT_CALL(time_source_, StartTicking());
        EXPECT_CALL(*audio_renderer_, SetVolume(1.0f));
        EXPECT_CALL(*audio_renderer_, StartPlaying())
            .WillOnce(SetBufferingState(&audio_buffering_state_cb_,
                                        BUFFERING_HAVE_ENOUGH));
      }

      if (video_stream_) {
        EXPECT_CALL(*video_renderer_, StartPlaying())
            .WillOnce(SetBufferingState(&video_buffering_state_cb_,
                                        BUFFERING_HAVE_ENOUGH));
      }

      EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
    }

    pipeline_->Start(
        filter_collection_.Pass(),
        base::Bind(&CallbackHelper::OnEnded, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnError, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnStart, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnMetadata, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnBufferingStateChange,
                   base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnDurationChange,
                   base::Unretained(&callbacks_)));
    message_loop_.RunUntilIdle();
  }

  void CreateAudioStream() {
    audio_stream_ = CreateStream(DemuxerStream::AUDIO);
  }

  void CreateVideoStream() {
    video_stream_ = CreateStream(DemuxerStream::VIDEO);
    video_stream_->set_video_decoder_config(video_decoder_config_);
  }

  void CreateTextStream() {
    scoped_ptr<FakeTextTrackStream> text_stream(new FakeTextTrackStream());
    EXPECT_CALL(*text_stream, OnRead()).Times(AnyNumber());
    text_stream_ = text_stream.Pass();
  }

  MockDemuxerStream* audio_stream() {
    return audio_stream_.get();
  }

  MockDemuxerStream* video_stream() {
    return video_stream_.get();
  }

  FakeTextTrackStream* text_stream() {
    return text_stream_.get();
  }

  void ExpectSeek(const base::TimeDelta& seek_time, bool underflowed) {
    // Every filter should receive a call to Seek().
    EXPECT_CALL(*demuxer_, Seek(seek_time, _))
        .WillOnce(RunCallback<1>(PIPELINE_OK));

    if (audio_stream_) {
      if (!underflowed)
        EXPECT_CALL(time_source_, StopTicking());
      EXPECT_CALL(*audio_renderer_, Flush(_))
          .WillOnce(DoAll(SetBufferingState(&audio_buffering_state_cb_,
                                            BUFFERING_HAVE_NOTHING),
                          RunClosure<0>()));
      EXPECT_CALL(time_source_, SetMediaTime(seek_time));
      EXPECT_CALL(time_source_, SetPlaybackRate(_));
      EXPECT_CALL(time_source_, StartTicking());
      EXPECT_CALL(*audio_renderer_, StartPlaying())
          .WillOnce(SetBufferingState(&audio_buffering_state_cb_,
                                      BUFFERING_HAVE_ENOUGH));
      EXPECT_CALL(*audio_renderer_, SetVolume(_));
    }

    if (video_stream_) {
      EXPECT_CALL(*video_renderer_, Flush(_))
          .WillOnce(DoAll(SetBufferingState(&video_buffering_state_cb_,
                                            BUFFERING_HAVE_NOTHING),
                          RunClosure<0>()));
      EXPECT_CALL(*video_renderer_, StartPlaying())
          .WillOnce(SetBufferingState(&video_buffering_state_cb_,
                                      BUFFERING_HAVE_ENOUGH));
    }

    // We expect a successful seek callback followed by a buffering update.
    EXPECT_CALL(callbacks_, OnSeek(PIPELINE_OK));
    EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
  }

  void DoSeek(const base::TimeDelta& seek_time) {
    pipeline_->Seek(seek_time,
                    base::Bind(&CallbackHelper::OnSeek,
                               base::Unretained(&callbacks_)));

    // We expect the time to be updated only after the seek has completed.
    EXPECT_NE(seek_time, pipeline_->GetMediaTime());
    message_loop_.RunUntilIdle();
    EXPECT_EQ(seek_time, pipeline_->GetMediaTime());
  }

  void DestroyPipeline() {
    pipeline_.reset();
  }

  void ExpectDemuxerStop() {
    if (demuxer_)
      EXPECT_CALL(*demuxer_, Stop(_)).WillOnce(RunClosure<0>());
  }

  void ExpectPipelineStopAndDestroyPipeline() {
    // After the Pipeline is stopped, it could be destroyed any time. Always
    // destroy the pipeline immediately after OnStop() to test this.
    EXPECT_CALL(callbacks_, OnStop())
        .WillOnce(Invoke(this, &PipelineTest::DestroyPipeline));
  }

  MOCK_METHOD2(OnAddTextTrack, void(const TextTrackConfig&,
                                    const AddTextTrackDoneCB&));

  void DoOnAddTextTrack(const TextTrackConfig& config,
                        const AddTextTrackDoneCB& done_cb) {
    scoped_ptr<TextTrack> text_track(new MockTextTrack);
    done_cb.Run(text_track.Pass());
  }

  // Fixture members.
  StrictMock<CallbackHelper> callbacks_;
  base::SimpleTestTickClock test_tick_clock_;
  base::MessageLoop message_loop_;
  scoped_ptr<Pipeline> pipeline_;

  scoped_ptr<FilterCollection> filter_collection_;
  scoped_ptr<StrictMock<MockDemuxer> > demuxer_;
  StrictMock<MockVideoRenderer>* video_renderer_;
  StrictMock<MockAudioRenderer>* audio_renderer_;
  StrictMock<MockTimeSource> time_source_;
  StrictMock<CallbackHelper> text_renderer_callbacks_;
  TextRenderer* text_renderer_;
  scoped_ptr<StrictMock<MockDemuxerStream> > audio_stream_;
  scoped_ptr<StrictMock<MockDemuxerStream> > video_stream_;
  scoped_ptr<FakeTextTrackStream> text_stream_;
  AudioRenderer::TimeCB audio_time_cb_;
  BufferingStateCB audio_buffering_state_cb_;
  BufferingStateCB video_buffering_state_cb_;
  base::Closure audio_ended_cb_;
  base::Closure video_ended_cb_;
  VideoDecoderConfig video_decoder_config_;
  PipelineMetadata metadata_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PipelineTest);
};

// Test that playback controls methods no-op when the pipeline hasn't been
// started.
TEST_F(PipelineTest, NotStarted) {
  const base::TimeDelta kZero;

  EXPECT_FALSE(pipeline_->IsRunning());

  // Setting should still work.
  EXPECT_EQ(0.0f, pipeline_->GetPlaybackRate());
  pipeline_->SetPlaybackRate(-1.0f);
  EXPECT_EQ(0.0f, pipeline_->GetPlaybackRate());
  pipeline_->SetPlaybackRate(1.0f);
  EXPECT_EQ(1.0f, pipeline_->GetPlaybackRate());

  // Setting should still work.
  EXPECT_EQ(1.0f, pipeline_->GetVolume());
  pipeline_->SetVolume(-1.0f);
  EXPECT_EQ(1.0f, pipeline_->GetVolume());
  pipeline_->SetVolume(0.0f);
  EXPECT_EQ(0.0f, pipeline_->GetVolume());

  EXPECT_TRUE(kZero == pipeline_->GetMediaTime());
  EXPECT_EQ(0u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_TRUE(kZero == pipeline_->GetMediaDuration());
}

TEST_F(PipelineTest, NeverInitializes) {
  // Don't execute the callback passed into Initialize().
  EXPECT_CALL(*demuxer_, Initialize(_, _, _));

  // This test hangs during initialization by never calling
  // InitializationComplete().  StrictMock<> will ensure that the callback is
  // never executed.
  pipeline_->Start(
        filter_collection_.Pass(),
        base::Bind(&CallbackHelper::OnEnded, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnError, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnStart, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnMetadata, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnBufferingStateChange,
                   base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnDurationChange,
                   base::Unretained(&callbacks_)));
  message_loop_.RunUntilIdle();

  // Because our callback will get executed when the test tears down, we'll
  // verify that nothing has been called, then set our expectation for the call
  // made during tear down.
  Mock::VerifyAndClear(&callbacks_);
  EXPECT_CALL(callbacks_, OnStart(PIPELINE_OK));
}

TEST_F(PipelineTest, StopWithoutStart) {
  ExpectPipelineStopAndDestroyPipeline();
  pipeline_->Stop(
      base::Bind(&CallbackHelper::OnStop, base::Unretained(&callbacks_)));
  message_loop_.RunUntilIdle();
}

TEST_F(PipelineTest, StartThenStopImmediately) {
  EXPECT_CALL(*demuxer_, Initialize(_, _, _))
      .WillOnce(RunCallback<1>(PIPELINE_OK));
  EXPECT_CALL(*demuxer_, Stop(_))
      .WillOnce(RunClosure<0>());

  EXPECT_CALL(callbacks_, OnStart(_));

  pipeline_->Start(
      filter_collection_.Pass(),
      base::Bind(&CallbackHelper::OnEnded, base::Unretained(&callbacks_)),
      base::Bind(&CallbackHelper::OnError, base::Unretained(&callbacks_)),
      base::Bind(&CallbackHelper::OnStart, base::Unretained(&callbacks_)),
      base::Bind(&CallbackHelper::OnMetadata, base::Unretained(&callbacks_)),
      base::Bind(&CallbackHelper::OnBufferingStateChange,
                 base::Unretained(&callbacks_)),
      base::Bind(&CallbackHelper::OnDurationChange,
                 base::Unretained(&callbacks_)));

  // Expect a stop callback if we were started.
  ExpectPipelineStopAndDestroyPipeline();
  pipeline_->Stop(
      base::Bind(&CallbackHelper::OnStop, base::Unretained(&callbacks_)));
  message_loop_.RunUntilIdle();
}

TEST_F(PipelineTest, DemuxerErrorDuringStop) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  SetDemuxerExpectations(&streams);
  SetAudioRendererExpectations(audio_stream());

  StartPipeline(PIPELINE_OK);

  EXPECT_CALL(*demuxer_, Stop(_))
      .WillOnce(DoAll(InvokeWithoutArgs(this, &PipelineTest::OnDemuxerError),
                      RunClosure<0>()));
  ExpectPipelineStopAndDestroyPipeline();

  pipeline_->Stop(
      base::Bind(&CallbackHelper::OnStop, base::Unretained(&callbacks_)));
  message_loop_.RunUntilIdle();
}

TEST_F(PipelineTest, URLNotFound) {
  EXPECT_CALL(*demuxer_, Initialize(_, _, _))
      .WillOnce(RunCallback<1>(PIPELINE_ERROR_URL_NOT_FOUND));
  EXPECT_CALL(*demuxer_, Stop(_))
      .WillOnce(RunClosure<0>());

  StartPipeline(PIPELINE_ERROR_URL_NOT_FOUND);
}

TEST_F(PipelineTest, NoStreams) {
  EXPECT_CALL(*demuxer_, Initialize(_, _, _))
      .WillOnce(RunCallback<1>(PIPELINE_OK));
  EXPECT_CALL(*demuxer_, Stop(_))
      .WillOnce(RunClosure<0>());

  StartPipeline(PIPELINE_ERROR_COULD_NOT_RENDER);
}

TEST_F(PipelineTest, AudioStream) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  SetDemuxerExpectations(&streams);
  SetAudioRendererExpectations(audio_stream());

  StartPipeline(PIPELINE_OK);
  EXPECT_TRUE(metadata_.has_audio);
  EXPECT_FALSE(metadata_.has_video);
}

TEST_F(PipelineTest, VideoStream) {
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  SetDemuxerExpectations(&streams);
  SetVideoRendererExpectations(video_stream());

  StartPipeline(PIPELINE_OK);
  EXPECT_FALSE(metadata_.has_audio);
  EXPECT_TRUE(metadata_.has_video);
}

TEST_F(PipelineTest, AudioVideoStream) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  SetDemuxerExpectations(&streams);
  SetAudioRendererExpectations(audio_stream());
  SetVideoRendererExpectations(video_stream());

  StartPipeline(PIPELINE_OK);
  EXPECT_TRUE(metadata_.has_audio);
  EXPECT_TRUE(metadata_.has_video);
}

TEST_F(PipelineTest, VideoTextStream) {
  CreateVideoStream();
  CreateTextStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  SetDemuxerExpectations(&streams);
  SetVideoRendererExpectations(video_stream());

  StartPipeline(PIPELINE_OK);
  EXPECT_FALSE(metadata_.has_audio);
  EXPECT_TRUE(metadata_.has_video);

  AddTextStream();
  message_loop_.RunUntilIdle();
}

TEST_F(PipelineTest, VideoAudioTextStream) {
  CreateVideoStream();
  CreateAudioStream();
  CreateTextStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());
  streams.push_back(audio_stream());

  SetDemuxerExpectations(&streams);
  SetVideoRendererExpectations(video_stream());
  SetAudioRendererExpectations(audio_stream());

  StartPipeline(PIPELINE_OK);
  EXPECT_TRUE(metadata_.has_audio);
  EXPECT_TRUE(metadata_.has_video);

  AddTextStream();
  message_loop_.RunUntilIdle();
}

TEST_F(PipelineTest, Seek) {
  CreateAudioStream();
  CreateVideoStream();
  CreateTextStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  SetDemuxerExpectations(&streams, base::TimeDelta::FromSeconds(3000));
  SetAudioRendererExpectations(audio_stream());
  SetVideoRendererExpectations(video_stream());

  // Initialize then seek!
  StartPipeline(PIPELINE_OK);

  // Every filter should receive a call to Seek().
  base::TimeDelta expected = base::TimeDelta::FromSeconds(2000);
  ExpectSeek(expected, false);
  DoSeek(expected);
}

TEST_F(PipelineTest, SeekAfterError) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  SetDemuxerExpectations(&streams, base::TimeDelta::FromSeconds(3000));
  SetAudioRendererExpectations(audio_stream());

  // Initialize then seek!
  StartPipeline(PIPELINE_OK);

  EXPECT_CALL(*demuxer_, Stop(_))
      .WillOnce(RunClosure<0>());
  EXPECT_CALL(callbacks_, OnError(_));

  static_cast<DemuxerHost*>(pipeline_.get())
      ->OnDemuxerError(PIPELINE_ERROR_ABORT);
  message_loop_.RunUntilIdle();

  pipeline_->Seek(
      base::TimeDelta::FromMilliseconds(100),
      base::Bind(&CallbackHelper::OnSeek, base::Unretained(&callbacks_)));
  message_loop_.RunUntilIdle();
}

TEST_F(PipelineTest, SetVolume) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  SetDemuxerExpectations(&streams);
  SetAudioRendererExpectations(audio_stream());

  // The audio renderer should receive a call to SetVolume().
  float expected = 0.5f;
  EXPECT_CALL(*audio_renderer_, SetVolume(expected));

  // Initialize then set volume!
  StartPipeline(PIPELINE_OK);
  pipeline_->SetVolume(expected);
}

TEST_F(PipelineTest, Properties) {
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  const base::TimeDelta kDuration = base::TimeDelta::FromSeconds(100);
  SetDemuxerExpectations(&streams, kDuration);
  SetVideoRendererExpectations(video_stream());

  StartPipeline(PIPELINE_OK);
  EXPECT_EQ(kDuration.ToInternalValue(),
            pipeline_->GetMediaDuration().ToInternalValue());
  EXPECT_FALSE(pipeline_->DidLoadingProgress());
}

TEST_F(PipelineTest, GetBufferedTimeRanges) {
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  const base::TimeDelta kDuration = base::TimeDelta::FromSeconds(100);
  SetDemuxerExpectations(&streams, kDuration);
  SetVideoRendererExpectations(video_stream());

  StartPipeline(PIPELINE_OK);

  EXPECT_EQ(0u, pipeline_->GetBufferedTimeRanges().size());

  EXPECT_FALSE(pipeline_->DidLoadingProgress());
  pipeline_->AddBufferedTimeRange(base::TimeDelta(), kDuration / 8);
  EXPECT_TRUE(pipeline_->DidLoadingProgress());
  EXPECT_FALSE(pipeline_->DidLoadingProgress());
  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(base::TimeDelta(), pipeline_->GetBufferedTimeRanges().start(0));
  EXPECT_EQ(kDuration / 8, pipeline_->GetBufferedTimeRanges().end(0));

  base::TimeDelta kSeekTime = kDuration / 2;
  ExpectSeek(kSeekTime, false);
  DoSeek(kSeekTime);

  EXPECT_FALSE(pipeline_->DidLoadingProgress());
}

TEST_F(PipelineTest, EndedCallback) {
  CreateAudioStream();
  CreateVideoStream();
  CreateTextStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  SetDemuxerExpectations(&streams);
  SetAudioRendererExpectations(audio_stream());
  SetVideoRendererExpectations(video_stream());
  StartPipeline(PIPELINE_OK);

  AddTextStream();

  // The ended callback shouldn't run until all renderers have ended.
  audio_ended_cb_.Run();
  message_loop_.RunUntilIdle();

  video_ended_cb_.Run();
  message_loop_.RunUntilIdle();

  EXPECT_CALL(time_source_, StopTicking());
  EXPECT_CALL(callbacks_, OnEnded());
  text_stream()->SendEosNotification();
  message_loop_.RunUntilIdle();
}

TEST_F(PipelineTest, AudioStreamShorterThanVideo) {
  base::TimeDelta duration = base::TimeDelta::FromSeconds(10);

  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  // Replace what's used for interpolating to simulate wall clock time.
  pipeline_->SetTimeDeltaInterpolatorForTesting(
      new TimeDeltaInterpolator(&test_tick_clock_));

  SetDemuxerExpectations(&streams, duration);
  SetAudioRendererExpectations(audio_stream());
  SetVideoRendererExpectations(video_stream());
  StartPipeline(PIPELINE_OK);

  EXPECT_EQ(0, pipeline_->GetMediaTime().ToInternalValue());

  float playback_rate = 1.0f;
  EXPECT_CALL(time_source_, SetPlaybackRate(playback_rate));
  pipeline_->SetPlaybackRate(playback_rate);
  message_loop_.RunUntilIdle();

  InSequence s;

  // Verify that the clock doesn't advance since it hasn't been started by
  // a time update from the audio stream.
  int64 start_time = pipeline_->GetMediaTime().ToInternalValue();
  test_tick_clock_.Advance(base::TimeDelta::FromMilliseconds(100));
  EXPECT_EQ(pipeline_->GetMediaTime().ToInternalValue(), start_time);

  // Signal end of audio stream.
  audio_ended_cb_.Run();
  message_loop_.RunUntilIdle();

  // Verify that the clock advances.
  start_time = pipeline_->GetMediaTime().ToInternalValue();
  test_tick_clock_.Advance(base::TimeDelta::FromMilliseconds(100));
  EXPECT_GT(pipeline_->GetMediaTime().ToInternalValue(), start_time);

  // Signal end of video stream and make sure OnEnded() callback occurs.
  EXPECT_CALL(time_source_, StopTicking());
  EXPECT_CALL(callbacks_, OnEnded());
  video_ended_cb_.Run();
}

TEST_F(PipelineTest, ErrorDuringSeek) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  SetDemuxerExpectations(&streams);
  SetAudioRendererExpectations(audio_stream());
  StartPipeline(PIPELINE_OK);

  float playback_rate = 1.0f;
  EXPECT_CALL(time_source_, SetPlaybackRate(playback_rate));
  pipeline_->SetPlaybackRate(playback_rate);
  message_loop_.RunUntilIdle();

  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(5);

  // Preroll() isn't called as the demuxer errors out first.
  EXPECT_CALL(time_source_, StopTicking());
  EXPECT_CALL(*audio_renderer_, Flush(_))
      .WillOnce(DoAll(SetBufferingState(&audio_buffering_state_cb_,
                                        BUFFERING_HAVE_NOTHING),
                      RunClosure<0>()));

  EXPECT_CALL(*demuxer_, Seek(seek_time, _))
      .WillOnce(RunCallback<1>(PIPELINE_ERROR_READ));
  EXPECT_CALL(*demuxer_, Stop(_))
      .WillOnce(RunClosure<0>());

  pipeline_->Seek(seek_time, base::Bind(&CallbackHelper::OnSeek,
                                        base::Unretained(&callbacks_)));
  EXPECT_CALL(callbacks_, OnSeek(PIPELINE_ERROR_READ));
  message_loop_.RunUntilIdle();
}

// Invoked function OnError. This asserts that the pipeline does not enqueue
// non-teardown related tasks while tearing down.
static void TestNoCallsAfterError(
    Pipeline* pipeline, base::MessageLoop* message_loop,
    PipelineStatus /* status */) {
  CHECK(pipeline);
  CHECK(message_loop);

  // When we get to this stage, the message loop should be empty.
  EXPECT_TRUE(message_loop->IsIdleForTesting());

  // Make calls on pipeline after error has occurred.
  pipeline->SetPlaybackRate(0.5f);
  pipeline->SetVolume(0.5f);

  // No additional tasks should be queued as a result of these calls.
  EXPECT_TRUE(message_loop->IsIdleForTesting());
}

TEST_F(PipelineTest, NoMessageDuringTearDownFromError) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  SetDemuxerExpectations(&streams);
  SetAudioRendererExpectations(audio_stream());
  StartPipeline(PIPELINE_OK);

  // Trigger additional requests on the pipeline during tear down from error.
  base::Callback<void(PipelineStatus)> cb = base::Bind(
      &TestNoCallsAfterError, pipeline_.get(), &message_loop_);
  ON_CALL(callbacks_, OnError(_))
      .WillByDefault(Invoke(&cb, &base::Callback<void(PipelineStatus)>::Run));

  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(5);

  // Seek() isn't called as the demuxer errors out first.
  EXPECT_CALL(time_source_, StopTicking());
  EXPECT_CALL(*audio_renderer_, Flush(_))
      .WillOnce(DoAll(SetBufferingState(&audio_buffering_state_cb_,
                                        BUFFERING_HAVE_NOTHING),
                      RunClosure<0>()));

  EXPECT_CALL(*demuxer_, Seek(seek_time, _))
      .WillOnce(RunCallback<1>(PIPELINE_ERROR_READ));
  EXPECT_CALL(*demuxer_, Stop(_))
      .WillOnce(RunClosure<0>());

  pipeline_->Seek(seek_time, base::Bind(&CallbackHelper::OnSeek,
                                        base::Unretained(&callbacks_)));
  EXPECT_CALL(callbacks_, OnSeek(PIPELINE_ERROR_READ));
  message_loop_.RunUntilIdle();
}

static void RunTimeCB(const AudioRenderer::TimeCB& time_cb,
                       int time_in_ms,
                       int max_time_in_ms) {
  time_cb.Run(base::TimeDelta::FromMilliseconds(time_in_ms),
              base::TimeDelta::FromMilliseconds(max_time_in_ms));
}

TEST_F(PipelineTest, AudioTimeUpdateDuringSeek) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  SetDemuxerExpectations(&streams);
  SetAudioRendererExpectations(audio_stream());
  StartPipeline(PIPELINE_OK);

  float playback_rate = 1.0f;
  EXPECT_CALL(time_source_, SetPlaybackRate(playback_rate));
  pipeline_->SetPlaybackRate(playback_rate);
  message_loop_.RunUntilIdle();

  // Provide an initial time update so that the pipeline transitions out of the
  // "waiting for time update" state.
  audio_time_cb_.Run(base::TimeDelta::FromMilliseconds(100),
                     base::TimeDelta::FromMilliseconds(500));

  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(5);

  // Arrange to trigger a time update while the demuxer is in the middle of
  // seeking. This update should be ignored by the pipeline and the clock should
  // not get updated.
  base::Closure closure = base::Bind(&RunTimeCB, audio_time_cb_, 300, 700);
  EXPECT_CALL(*demuxer_, Seek(seek_time, _))
      .WillOnce(DoAll(InvokeWithoutArgs(&closure, &base::Closure::Run),
                      RunCallback<1>(PIPELINE_OK)));

  EXPECT_CALL(time_source_, StopTicking());
  EXPECT_CALL(*audio_renderer_, Flush(_))
      .WillOnce(DoAll(SetBufferingState(&audio_buffering_state_cb_,
                                        BUFFERING_HAVE_NOTHING),
                      RunClosure<0>()));
  EXPECT_CALL(time_source_, SetMediaTime(seek_time));
  EXPECT_CALL(time_source_, SetPlaybackRate(_));
  EXPECT_CALL(time_source_, StartTicking());
  EXPECT_CALL(*audio_renderer_, StartPlaying())
      .WillOnce(SetBufferingState(&audio_buffering_state_cb_,
                                  BUFFERING_HAVE_ENOUGH));
  EXPECT_CALL(*audio_renderer_, SetVolume(_));

  EXPECT_CALL(callbacks_, OnSeek(PIPELINE_OK));
  EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
  DoSeek(seek_time);

  EXPECT_EQ(pipeline_->GetMediaTime(), seek_time);

  // Now that the seek is complete, verify that time updates advance the current
  // time.
  base::TimeDelta new_time = seek_time + base::TimeDelta::FromMilliseconds(100);
  audio_time_cb_.Run(new_time, new_time);

  EXPECT_EQ(pipeline_->GetMediaTime(), new_time);
}

TEST_F(PipelineTest, DestroyAfterStop) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  SetDemuxerExpectations(&streams);
  SetAudioRendererExpectations(audio_stream());
  StartPipeline(PIPELINE_OK);

  ExpectDemuxerStop();

  ExpectPipelineStopAndDestroyPipeline();
  pipeline_->Stop(
      base::Bind(&CallbackHelper::OnStop, base::Unretained(&callbacks_)));
  message_loop_.RunUntilIdle();
}

TEST_F(PipelineTest, Underflow) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  SetDemuxerExpectations(&streams);
  SetAudioRendererExpectations(audio_stream());
  SetVideoRendererExpectations(video_stream());
  StartPipeline(PIPELINE_OK);

  // Simulate underflow.
  EXPECT_CALL(time_source_, StopTicking());
  audio_buffering_state_cb_.Run(BUFFERING_HAVE_NOTHING);

  // Seek while underflowed. We shouldn't call StopTicking() again.
  base::TimeDelta expected = base::TimeDelta::FromSeconds(5);
  ExpectSeek(expected, true);
  DoSeek(expected);
}

static void PostTimeCB(base::MessageLoop* message_loop,
                       const AudioRenderer::TimeCB& time_cb) {
  base::TimeDelta new_time = base::TimeDelta::FromMilliseconds(100);
  message_loop->PostTask(FROM_HERE, base::Bind(time_cb, new_time, new_time));
}

TEST_F(PipelineTest, TimeUpdateAfterStop) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  SetDemuxerExpectations(&streams);
  SetAudioRendererExpectations(audio_stream());
  SetVideoRendererExpectations(video_stream());
  StartPipeline(PIPELINE_OK);

  // Double post here! This is a hack to simulate the case where TimeCB is
  // posted during ~AudioRenderer(), which is triggered in Pipeline::DoStop.
  // Since we can't EXPECT_CALL the dtor and Pipeline::DoStop() is posted
  // as well, we need to post twice here.
  message_loop_.PostTask(
      FROM_HERE, base::Bind(&PostTimeCB, &message_loop_, audio_time_cb_));

  EXPECT_CALL(*demuxer_, Stop(_)).WillOnce(RunClosure<0>());

  ExpectPipelineStopAndDestroyPipeline();
  pipeline_->Stop(
      base::Bind(&CallbackHelper::OnStop, base::Unretained(&callbacks_)));
  message_loop_.RunUntilIdle();
}

class PipelineTeardownTest : public PipelineTest {
 public:
  enum TeardownState {
    kInitDemuxer,
    kInitAudioRenderer,
    kInitVideoRenderer,
    kFlushing,
    kSeeking,
    kPlaying,
  };

  enum StopOrError {
    kStop,
    kError,
    kErrorAndStop,
  };

  PipelineTeardownTest() {}
  virtual ~PipelineTeardownTest() {}

  void RunTest(TeardownState state, StopOrError stop_or_error) {
    switch (state) {
      case kInitDemuxer:
      case kInitAudioRenderer:
      case kInitVideoRenderer:
        DoInitialize(state, stop_or_error);
        break;

      case kFlushing:
      case kSeeking:
        DoInitialize(state, stop_or_error);
        DoSeek(state, stop_or_error);
        break;

      case kPlaying:
        DoInitialize(state, stop_or_error);
        DoStopOrError(stop_or_error);
        break;
    }
  }

 private:
  // TODO(scherkus): We do radically different things whether teardown is
  // invoked via stop vs error. The teardown path should be the same,
  // see http://crbug.com/110228
  void DoInitialize(TeardownState state, StopOrError stop_or_error) {
    PipelineStatus expected_status =
        SetInitializeExpectations(state, stop_or_error);

    EXPECT_CALL(callbacks_, OnStart(expected_status));
    pipeline_->Start(
        filter_collection_.Pass(),
        base::Bind(&CallbackHelper::OnEnded, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnError, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnStart, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnMetadata, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnBufferingStateChange,
                   base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnDurationChange,
                   base::Unretained(&callbacks_)));
    message_loop_.RunUntilIdle();
  }

  PipelineStatus SetInitializeExpectations(TeardownState state,
                                           StopOrError stop_or_error) {
    PipelineStatus status = PIPELINE_OK;
    base::Closure stop_cb = base::Bind(
        &CallbackHelper::OnStop, base::Unretained(&callbacks_));

    if (state == kInitDemuxer) {
      if (stop_or_error == kStop) {
        EXPECT_CALL(*demuxer_, Initialize(_, _, _))
            .WillOnce(DoAll(Stop(pipeline_.get(), stop_cb),
                            RunCallback<1>(PIPELINE_OK)));
        ExpectPipelineStopAndDestroyPipeline();
      } else {
        status = DEMUXER_ERROR_COULD_NOT_OPEN;
        EXPECT_CALL(*demuxer_, Initialize(_, _, _))
            .WillOnce(RunCallback<1>(status));
      }

      EXPECT_CALL(*demuxer_, Stop(_)).WillOnce(RunClosure<0>());
      return status;
    }

    CreateAudioStream();
    CreateVideoStream();
    MockDemuxerStreamVector streams;
    streams.push_back(audio_stream());
    streams.push_back(video_stream());
    SetDemuxerExpectations(&streams, base::TimeDelta::FromSeconds(3000));

    if (state == kInitAudioRenderer) {
      if (stop_or_error == kStop) {
        EXPECT_CALL(*audio_renderer_, Initialize(_, _, _, _, _, _, _))
            .WillOnce(DoAll(Stop(pipeline_.get(), stop_cb),
                            RunCallback<1>(PIPELINE_OK)));
        ExpectPipelineStopAndDestroyPipeline();
      } else {
        status = PIPELINE_ERROR_INITIALIZATION_FAILED;
        EXPECT_CALL(*audio_renderer_, Initialize(_, _, _, _, _, _, _))
            .WillOnce(RunCallback<1>(status));
      }

      EXPECT_CALL(*demuxer_, Stop(_)).WillOnce(RunClosure<0>());
      return status;
    }

    EXPECT_CALL(*audio_renderer_, Initialize(_, _, _, _, _, _, _))
        .WillOnce(DoAll(SaveArg<4>(&audio_buffering_state_cb_),
                        RunCallback<1>(PIPELINE_OK)));

    if (state == kInitVideoRenderer) {
      if (stop_or_error == kStop) {
        EXPECT_CALL(*video_renderer_, Initialize(_, _, _, _, _, _, _, _, _, _))
            .WillOnce(DoAll(Stop(pipeline_.get(), stop_cb),
                            RunCallback<2>(PIPELINE_OK)));
        ExpectPipelineStopAndDestroyPipeline();
      } else {
        status = PIPELINE_ERROR_INITIALIZATION_FAILED;
        EXPECT_CALL(*video_renderer_, Initialize(_, _, _, _, _, _, _, _, _, _))
            .WillOnce(RunCallback<2>(status));
      }

      EXPECT_CALL(*demuxer_, Stop(_)).WillOnce(RunClosure<0>());
      return status;
    }

    EXPECT_CALL(*video_renderer_, Initialize(_, _, _, _, _, _, _, _, _, _))
        .WillOnce(DoAll(SaveArg<5>(&video_buffering_state_cb_),
                        RunCallback<2>(PIPELINE_OK)));

    EXPECT_CALL(callbacks_, OnMetadata(_));

    // If we get here it's a successful initialization.
    EXPECT_CALL(*audio_renderer_, GetTimeSource())
        .WillOnce(Return(&time_source_));
    EXPECT_CALL(time_source_, SetMediaTime(base::TimeDelta()));
    EXPECT_CALL(time_source_, SetPlaybackRate(0.0f));
    EXPECT_CALL(time_source_, StartTicking());
    EXPECT_CALL(*audio_renderer_, SetVolume(1.0f));
    EXPECT_CALL(*audio_renderer_, StartPlaying())
        .WillOnce(SetBufferingState(&audio_buffering_state_cb_,
                                    BUFFERING_HAVE_ENOUGH));
    EXPECT_CALL(*video_renderer_, StartPlaying())
        .WillOnce(SetBufferingState(&video_buffering_state_cb_,
                                    BUFFERING_HAVE_ENOUGH));

    if (status == PIPELINE_OK)
      EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));

    return status;
  }

  void DoSeek(TeardownState state, StopOrError stop_or_error) {
    InSequence s;
    PipelineStatus status = SetSeekExpectations(state, stop_or_error);

    EXPECT_CALL(*demuxer_, Stop(_)).WillOnce(RunClosure<0>());
    EXPECT_CALL(callbacks_, OnSeek(status));

    if (status == PIPELINE_OK) {
      ExpectPipelineStopAndDestroyPipeline();
    }

    pipeline_->Seek(base::TimeDelta::FromSeconds(10), base::Bind(
        &CallbackHelper::OnSeek, base::Unretained(&callbacks_)));
    message_loop_.RunUntilIdle();
  }

  PipelineStatus SetSeekExpectations(TeardownState state,
                                     StopOrError stop_or_error) {
    PipelineStatus status = PIPELINE_OK;
    base::Closure stop_cb = base::Bind(
        &CallbackHelper::OnStop, base::Unretained(&callbacks_));

    EXPECT_CALL(time_source_, StopTicking());

    if (state == kFlushing) {
      if (stop_or_error == kStop) {
        EXPECT_CALL(*audio_renderer_, Flush(_))
            .WillOnce(DoAll(Stop(pipeline_.get(), stop_cb),
                            SetBufferingState(&audio_buffering_state_cb_,
                                              BUFFERING_HAVE_NOTHING),
                            RunClosure<0>()));
      } else {
        status = PIPELINE_ERROR_READ;
        EXPECT_CALL(*audio_renderer_, Flush(_)).WillOnce(
            DoAll(SetError(pipeline_.get(), status),
                  SetBufferingState(&audio_buffering_state_cb_,
                                    BUFFERING_HAVE_NOTHING),
                  RunClosure<0>()));
      }

      return status;
    }

    EXPECT_CALL(*audio_renderer_, Flush(_))
        .WillOnce(DoAll(SetBufferingState(&audio_buffering_state_cb_,
                                          BUFFERING_HAVE_NOTHING),
                        RunClosure<0>()));
    EXPECT_CALL(*video_renderer_, Flush(_))
        .WillOnce(DoAll(SetBufferingState(&video_buffering_state_cb_,
                                          BUFFERING_HAVE_NOTHING),
                        RunClosure<0>()));

    if (state == kSeeking) {
      if (stop_or_error == kStop) {
        EXPECT_CALL(*demuxer_, Seek(_, _))
            .WillOnce(DoAll(Stop(pipeline_.get(), stop_cb),
                            RunCallback<1>(PIPELINE_OK)));
      } else {
        status = PIPELINE_ERROR_READ;
        EXPECT_CALL(*demuxer_, Seek(_, _))
            .WillOnce(RunCallback<1>(status));
      }

      return status;
    }

    NOTREACHED() << "State not supported: " << state;
    return status;
  }

  void DoStopOrError(StopOrError stop_or_error) {
    InSequence s;

    EXPECT_CALL(*demuxer_, Stop(_)).WillOnce(RunClosure<0>());

    switch (stop_or_error) {
      case kStop:
        ExpectPipelineStopAndDestroyPipeline();
        pipeline_->Stop(base::Bind(
            &CallbackHelper::OnStop, base::Unretained(&callbacks_)));
        break;

      case kError:
        EXPECT_CALL(callbacks_, OnError(PIPELINE_ERROR_READ));
        pipeline_->SetErrorForTesting(PIPELINE_ERROR_READ);
        break;

      case kErrorAndStop:
        EXPECT_CALL(callbacks_, OnError(PIPELINE_ERROR_READ));
        ExpectPipelineStopAndDestroyPipeline();
        pipeline_->SetErrorForTesting(PIPELINE_ERROR_READ);
        pipeline_->Stop(base::Bind(
            &CallbackHelper::OnStop, base::Unretained(&callbacks_)));
        break;
    }

    message_loop_.RunUntilIdle();
  }

  DISALLOW_COPY_AND_ASSIGN(PipelineTeardownTest);
};

#define INSTANTIATE_TEARDOWN_TEST(stop_or_error, state) \
    TEST_F(PipelineTeardownTest, stop_or_error##_##state) { \
      RunTest(k##state, k##stop_or_error); \
    }

INSTANTIATE_TEARDOWN_TEST(Stop, InitDemuxer);
INSTANTIATE_TEARDOWN_TEST(Stop, InitAudioRenderer);
INSTANTIATE_TEARDOWN_TEST(Stop, InitVideoRenderer);
INSTANTIATE_TEARDOWN_TEST(Stop, Flushing);
INSTANTIATE_TEARDOWN_TEST(Stop, Seeking);
INSTANTIATE_TEARDOWN_TEST(Stop, Playing);

INSTANTIATE_TEARDOWN_TEST(Error, InitDemuxer);
INSTANTIATE_TEARDOWN_TEST(Error, InitAudioRenderer);
INSTANTIATE_TEARDOWN_TEST(Error, InitVideoRenderer);
INSTANTIATE_TEARDOWN_TEST(Error, Flushing);
INSTANTIATE_TEARDOWN_TEST(Error, Seeking);
INSTANTIATE_TEARDOWN_TEST(Error, Playing);

INSTANTIATE_TEARDOWN_TEST(ErrorAndStop, Playing);

}  // namespace media
