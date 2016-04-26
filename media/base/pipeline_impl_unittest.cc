// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/pipeline_impl.h"

#include <stddef.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/simple_thread.h"
#include "base/time/clock.h"
#include "media/base/fake_text_track_stream.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/media_log.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/base/text_renderer.h"
#include "media/base/text_track_config.h"
#include "media/base/time_delta_interpolator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

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

ACTION_TEMPLATE(PostCallback,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p0)) {
  return base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(::std::tr1::get<k>(args), p0));
}

// TODO(scherkus): even though some filters are initialized on separate
// threads these test aren't flaky... why?  It's because filters' Initialize()
// is executed on |message_loop_| and the mock filters instantly call
// InitializationComplete(), which keeps the pipeline humming along.  If
// either filters don't call InitializationComplete() immediately or filter
// initialization is moved to a separate thread this test will become flaky.
class PipelineImplTest : public ::testing::Test {
 public:
  // Used for setting expectations on pipeline callbacks.  Using a StrictMock
  // also lets us test for missing callbacks.
  class CallbackHelper {
   public:
    CallbackHelper() {}
    virtual ~CallbackHelper() {}

    MOCK_METHOD1(OnStart, void(PipelineStatus));
    MOCK_METHOD1(OnSeek, void(PipelineStatus));
    MOCK_METHOD1(OnSuspend, void(PipelineStatus));
    MOCK_METHOD1(OnResume, void(PipelineStatus));
    MOCK_METHOD0(OnStop, void());
    MOCK_METHOD0(OnEnded, void());
    MOCK_METHOD1(OnError, void(PipelineStatus));
    MOCK_METHOD1(OnMetadata, void(PipelineMetadata));
    MOCK_METHOD1(OnBufferingStateChange, void(BufferingState));
    MOCK_METHOD0(OnDurationChange, void());

   private:
    DISALLOW_COPY_AND_ASSIGN(CallbackHelper);
  };

  PipelineImplTest()
      : pipeline_(
            new PipelineImpl(message_loop_.task_runner(), new MediaLog())),
        demuxer_(new StrictMock<MockDemuxer>()),
        scoped_renderer_(new StrictMock<MockRenderer>()),
        renderer_(scoped_renderer_.get()) {
    // SetDemuxerExpectations() adds overriding expectations for expected
    // non-NULL streams.
    DemuxerStream* null_pointer = NULL;
    EXPECT_CALL(*demuxer_, GetStream(_)).WillRepeatedly(Return(null_pointer));

    EXPECT_CALL(*demuxer_, GetTimelineOffset())
        .WillRepeatedly(Return(base::Time()));

    EXPECT_CALL(*renderer_, GetMediaTime())
        .WillRepeatedly(Return(base::TimeDelta()));

    EXPECT_CALL(*demuxer_, GetStartTime()).WillRepeatedly(Return(start_time_));
  }

  virtual ~PipelineImplTest() {
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
    pipeline_->Stop(
        base::Bind(&CallbackHelper::OnStop, base::Unretained(&callbacks_)));
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
                        PostCallback<1>(PIPELINE_OK)));

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

  std::unique_ptr<StrictMock<MockDemuxerStream>> CreateStream(
      DemuxerStream::Type type) {
    std::unique_ptr<StrictMock<MockDemuxerStream>> stream(
        new StrictMock<MockDemuxerStream>(type));
    return stream;
  }

  // Sets up expectations to allow the video renderer to initialize.
  void SetRendererExpectations() {
    EXPECT_CALL(*renderer_, Initialize(_, _, _, _, _, _, _))
        .WillOnce(DoAll(SaveArg<2>(&statistics_cb_),
                        SaveArg<3>(&buffering_state_cb_),
                        SaveArg<4>(&ended_cb_), PostCallback<1>(PIPELINE_OK)));
    EXPECT_CALL(*renderer_, HasAudio()).WillRepeatedly(Return(audio_stream()));
    EXPECT_CALL(*renderer_, HasVideo()).WillRepeatedly(Return(video_stream()));
  }

  void AddTextStream() {
    EXPECT_CALL(*this, OnAddTextTrack(_, _))
        .WillOnce(Invoke(this, &PipelineImplTest::DoOnAddTextTrack));
    static_cast<DemuxerHost*>(pipeline_.get())
        ->AddTextStream(text_stream(),
                        TextTrackConfig(kTextSubtitles, "", "", ""));
    message_loop_.RunUntilIdle();
  }

  void StartPipeline() {
    EXPECT_CALL(*this, OnWaitingForDecryptionKey()).Times(0);
    pipeline_->Start(
        demuxer_.get(), std::move(scoped_renderer_),
        base::Bind(&CallbackHelper::OnEnded, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnError, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnStart, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnMetadata, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnBufferingStateChange,
                   base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnDurationChange,
                   base::Unretained(&callbacks_)),
        base::Bind(&PipelineImplTest::OnAddTextTrack, base::Unretained(this)),
        base::Bind(&PipelineImplTest::OnWaitingForDecryptionKey,
                   base::Unretained(this)));
  }

  // Sets up expectations on the callback and initializes the pipeline. Called
  // after tests have set expectations any filters they wish to use.
  void StartPipelineAndExpect(PipelineStatus start_status) {
    EXPECT_CALL(callbacks_, OnStart(start_status));

    if (start_status == PIPELINE_OK) {
      EXPECT_CALL(callbacks_, OnMetadata(_)).WillOnce(SaveArg<0>(&metadata_));
      EXPECT_CALL(*renderer_, SetPlaybackRate(0.0));
      EXPECT_CALL(*renderer_, SetVolume(1.0f));
      EXPECT_CALL(*renderer_, StartPlayingFrom(start_time_))
          .WillOnce(
              SetBufferingState(&buffering_state_cb_, BUFFERING_HAVE_ENOUGH));
      EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
    }

    StartPipeline();
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
    std::unique_ptr<FakeTextTrackStream> text_stream(new FakeTextTrackStream());
    EXPECT_CALL(*text_stream, OnRead()).Times(AnyNumber());
    text_stream_ = std::move(text_stream);
  }

  MockDemuxerStream* audio_stream() { return audio_stream_.get(); }

  MockDemuxerStream* video_stream() { return video_stream_.get(); }

  FakeTextTrackStream* text_stream() { return text_stream_.get(); }

  void ExpectSeek(const base::TimeDelta& seek_time, bool underflowed) {
    EXPECT_CALL(*demuxer_, Seek(seek_time, _))
        .WillOnce(RunCallback<1>(PIPELINE_OK));

    EXPECT_CALL(*renderer_, Flush(_))
        .WillOnce(DoAll(
            SetBufferingState(&buffering_state_cb_, BUFFERING_HAVE_NOTHING),
            RunClosure<0>()));
    EXPECT_CALL(*renderer_, SetPlaybackRate(_));
    EXPECT_CALL(*renderer_, SetVolume(_));
    EXPECT_CALL(*renderer_, StartPlayingFrom(seek_time))
        .WillOnce(
            SetBufferingState(&buffering_state_cb_, BUFFERING_HAVE_ENOUGH));
    EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));

    // We expect a successful seek callback followed by a buffering update.
    EXPECT_CALL(callbacks_, OnSeek(PIPELINE_OK));
    EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
  }

  void DoSeek(const base::TimeDelta& seek_time) {
    pipeline_->Seek(seek_time, base::Bind(&CallbackHelper::OnSeek,
                                          base::Unretained(&callbacks_)));
    message_loop_.RunUntilIdle();
  }

  void ExpectSuspend() {
    EXPECT_CALL(*renderer_, SetPlaybackRate(0));
    EXPECT_CALL(*renderer_, Flush(_))
        .WillOnce(DoAll(
            SetBufferingState(&buffering_state_cb_, BUFFERING_HAVE_NOTHING),
            RunClosure<0>()));
    EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));
    EXPECT_CALL(callbacks_, OnSuspend(PIPELINE_OK));
  }

  void DoSuspend() {
    pipeline_->Suspend(
        base::Bind(&CallbackHelper::OnSuspend, base::Unretained(&callbacks_)));
    message_loop_.RunUntilIdle();

    // |renderer_| has been deleted, replace it.
    scoped_renderer_.reset(new StrictMock<MockRenderer>()),
        renderer_ = scoped_renderer_.get();
  }

  void ExpectResume(const base::TimeDelta& seek_time) {
    SetRendererExpectations();
    EXPECT_CALL(*demuxer_, Seek(seek_time, _))
        .WillOnce(RunCallback<1>(PIPELINE_OK));
    EXPECT_CALL(*renderer_, SetPlaybackRate(_));
    EXPECT_CALL(*renderer_, SetVolume(_));
    EXPECT_CALL(*renderer_, StartPlayingFrom(seek_time))
        .WillOnce(
            SetBufferingState(&buffering_state_cb_, BUFFERING_HAVE_ENOUGH));
    EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
    EXPECT_CALL(callbacks_, OnResume(PIPELINE_OK));
  }

  void DoResume(const base::TimeDelta& seek_time) {
    pipeline_->Resume(
        std::move(scoped_renderer_), seek_time,
        base::Bind(&CallbackHelper::OnResume, base::Unretained(&callbacks_)));
    message_loop_.RunUntilIdle();
  }

  void DestroyPipeline() {
    // In real code Pipeline could be destroyed on a different thread. All weak
    // pointers must have been invalidated before the stop callback returns.
    DCHECK(!pipeline_->HasWeakPtrsForTesting());
    pipeline_.reset();
  }

  void ExpectDemuxerStop() {
    if (demuxer_)
      EXPECT_CALL(*demuxer_, Stop());
  }

  void ExpectPipelineStopAndDestroyPipeline() {
    // After the Pipeline is stopped, it could be destroyed any time. Always
    // destroy the pipeline immediately after OnStop() to test this.
    EXPECT_CALL(callbacks_, OnStop())
        .WillOnce(Invoke(this, &PipelineImplTest::DestroyPipeline));
  }

  MOCK_METHOD2(OnAddTextTrack,
               void(const TextTrackConfig&, const AddTextTrackDoneCB&));
  MOCK_METHOD0(OnWaitingForDecryptionKey, void(void));

  void DoOnAddTextTrack(const TextTrackConfig& config,
                        const AddTextTrackDoneCB& done_cb) {
    std::unique_ptr<TextTrack> text_track(new MockTextTrack);
    done_cb.Run(std::move(text_track));
  }

  void RunBufferedTimeRangesTest(const base::TimeDelta duration) {
    EXPECT_EQ(0u, pipeline_->GetBufferedTimeRanges().size());
    EXPECT_FALSE(pipeline_->DidLoadingProgress());
    Ranges<base::TimeDelta> ranges;
    ranges.Add(base::TimeDelta(), duration);
    pipeline_->OnBufferedTimeRangesChanged(ranges);
    EXPECT_TRUE(pipeline_->DidLoadingProgress());
    EXPECT_FALSE(pipeline_->DidLoadingProgress());
    EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
    EXPECT_EQ(base::TimeDelta(), pipeline_->GetBufferedTimeRanges().start(0));
    EXPECT_EQ(duration, pipeline_->GetBufferedTimeRanges().end(0));
  }

  // Fixture members.
  StrictMock<CallbackHelper> callbacks_;
  base::SimpleTestTickClock test_tick_clock_;
  base::MessageLoop message_loop_;
  std::unique_ptr<PipelineImpl> pipeline_;

  std::unique_ptr<StrictMock<MockDemuxer>> demuxer_;
  std::unique_ptr<StrictMock<MockRenderer>> scoped_renderer_;
  StrictMock<MockRenderer>* renderer_;
  StrictMock<CallbackHelper> text_renderer_callbacks_;
  TextRenderer* text_renderer_;
  std::unique_ptr<StrictMock<MockDemuxerStream>> audio_stream_;
  std::unique_ptr<StrictMock<MockDemuxerStream>> video_stream_;
  std::unique_ptr<FakeTextTrackStream> text_stream_;
  BufferingStateCB buffering_state_cb_;
  base::Closure ended_cb_;
  StatisticsCB statistics_cb_;
  VideoDecoderConfig video_decoder_config_;
  PipelineMetadata metadata_;
  base::TimeDelta start_time_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PipelineImplTest);
};

// Test that playback controls methods no-op when the pipeline hasn't been
// started.
TEST_F(PipelineImplTest, NotStarted) {
  const base::TimeDelta kZero;

  EXPECT_FALSE(pipeline_->IsRunning());

  // Setting should still work.
  EXPECT_EQ(0.0f, pipeline_->GetPlaybackRate());
  pipeline_->SetPlaybackRate(-1.0);
  EXPECT_EQ(0.0f, pipeline_->GetPlaybackRate());
  pipeline_->SetPlaybackRate(1.0);
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

TEST_F(PipelineImplTest, NeverInitializes) {
  // Don't execute the callback passed into Initialize().
  EXPECT_CALL(*demuxer_, Initialize(_, _, _));

  // This test hangs during initialization by never calling
  // InitializationComplete().  StrictMock<> will ensure that the callback is
  // never executed.
  StartPipeline();
  message_loop_.RunUntilIdle();

  // Because our callback will get executed when the test tears down, we'll
  // verify that nothing has been called, then set our expectation for the call
  // made during tear down.
  Mock::VerifyAndClear(&callbacks_);
  EXPECT_CALL(callbacks_, OnStart(PIPELINE_OK));
}

TEST_F(PipelineImplTest, StopWithoutStart) {
  ExpectPipelineStopAndDestroyPipeline();
  pipeline_->Stop(
      base::Bind(&CallbackHelper::OnStop, base::Unretained(&callbacks_)));
  message_loop_.RunUntilIdle();
}

TEST_F(PipelineImplTest, StartThenStopImmediately) {
  EXPECT_CALL(*demuxer_, Initialize(_, _, _))
      .WillOnce(PostCallback<1>(PIPELINE_OK));
  EXPECT_CALL(*demuxer_, Stop());

  EXPECT_CALL(callbacks_, OnStart(_));
  StartPipeline();

  // Expect a stop callback if we were started.
  ExpectPipelineStopAndDestroyPipeline();
  pipeline_->Stop(
      base::Bind(&CallbackHelper::OnStop, base::Unretained(&callbacks_)));
  message_loop_.RunUntilIdle();
}

TEST_F(PipelineImplTest, DemuxerErrorDuringStop) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  SetDemuxerExpectations(&streams);
  SetRendererExpectations();

  StartPipelineAndExpect(PIPELINE_OK);

  EXPECT_CALL(*demuxer_, Stop())
      .WillOnce(InvokeWithoutArgs(this, &PipelineImplTest::OnDemuxerError));
  ExpectPipelineStopAndDestroyPipeline();

  pipeline_->Stop(
      base::Bind(&CallbackHelper::OnStop, base::Unretained(&callbacks_)));
  message_loop_.RunUntilIdle();
}

TEST_F(PipelineImplTest, NoStreams) {
  EXPECT_CALL(*demuxer_, Initialize(_, _, _))
      .WillOnce(PostCallback<1>(PIPELINE_OK));
  EXPECT_CALL(*demuxer_, Stop());
  EXPECT_CALL(callbacks_, OnMetadata(_));

  StartPipelineAndExpect(PIPELINE_ERROR_COULD_NOT_RENDER);
}

TEST_F(PipelineImplTest, AudioStream) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  SetDemuxerExpectations(&streams);
  SetRendererExpectations();

  StartPipelineAndExpect(PIPELINE_OK);
  EXPECT_TRUE(metadata_.has_audio);
  EXPECT_FALSE(metadata_.has_video);
}

TEST_F(PipelineImplTest, VideoStream) {
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  SetDemuxerExpectations(&streams);
  SetRendererExpectations();

  StartPipelineAndExpect(PIPELINE_OK);
  EXPECT_FALSE(metadata_.has_audio);
  EXPECT_TRUE(metadata_.has_video);
}

TEST_F(PipelineImplTest, AudioVideoStream) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  SetDemuxerExpectations(&streams);
  SetRendererExpectations();

  StartPipelineAndExpect(PIPELINE_OK);
  EXPECT_TRUE(metadata_.has_audio);
  EXPECT_TRUE(metadata_.has_video);
}

TEST_F(PipelineImplTest, VideoTextStream) {
  CreateVideoStream();
  CreateTextStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  SetDemuxerExpectations(&streams);
  SetRendererExpectations();

  StartPipelineAndExpect(PIPELINE_OK);
  EXPECT_FALSE(metadata_.has_audio);
  EXPECT_TRUE(metadata_.has_video);

  AddTextStream();
}

TEST_F(PipelineImplTest, VideoAudioTextStream) {
  CreateVideoStream();
  CreateAudioStream();
  CreateTextStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());
  streams.push_back(audio_stream());

  SetDemuxerExpectations(&streams);
  SetRendererExpectations();

  StartPipelineAndExpect(PIPELINE_OK);
  EXPECT_TRUE(metadata_.has_audio);
  EXPECT_TRUE(metadata_.has_video);

  AddTextStream();
}

TEST_F(PipelineImplTest, Seek) {
  CreateAudioStream();
  CreateVideoStream();
  CreateTextStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  SetDemuxerExpectations(&streams, base::TimeDelta::FromSeconds(3000));
  SetRendererExpectations();

  // Initialize then seek!
  StartPipelineAndExpect(PIPELINE_OK);

  // Every filter should receive a call to Seek().
  base::TimeDelta expected = base::TimeDelta::FromSeconds(2000);
  ExpectSeek(expected, false);
  DoSeek(expected);
}

TEST_F(PipelineImplTest, SeekAfterError) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  SetDemuxerExpectations(&streams, base::TimeDelta::FromSeconds(3000));
  SetRendererExpectations();

  // Initialize then seek!
  StartPipelineAndExpect(PIPELINE_OK);

  EXPECT_CALL(*demuxer_, Stop());
  EXPECT_CALL(callbacks_, OnError(_));

  static_cast<DemuxerHost*>(pipeline_.get())
      ->OnDemuxerError(PIPELINE_ERROR_ABORT);
  message_loop_.RunUntilIdle();

  pipeline_->Seek(
      base::TimeDelta::FromMilliseconds(100),
      base::Bind(&CallbackHelper::OnSeek, base::Unretained(&callbacks_)));
  message_loop_.RunUntilIdle();
}

TEST_F(PipelineImplTest, SuspendResume) {
  CreateAudioStream();
  CreateVideoStream();
  CreateTextStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  SetDemuxerExpectations(&streams, base::TimeDelta::FromSeconds(3000));
  SetRendererExpectations();

  StartPipelineAndExpect(PIPELINE_OK);

  // Inject some fake memory usage to verify its cleared after suspend.
  PipelineStatistics stats;
  stats.audio_memory_usage = 12345;
  stats.video_memory_usage = 67890;
  statistics_cb_.Run(stats);
  EXPECT_EQ(stats.audio_memory_usage,
            pipeline_->GetStatistics().audio_memory_usage);
  EXPECT_EQ(stats.video_memory_usage,
            pipeline_->GetStatistics().video_memory_usage);

  ExpectSuspend();
  DoSuspend();

  EXPECT_EQ(pipeline_->GetStatistics().audio_memory_usage, 0);
  EXPECT_EQ(pipeline_->GetStatistics().video_memory_usage, 0);

  base::TimeDelta expected = base::TimeDelta::FromSeconds(2000);
  ExpectResume(expected);
  DoResume(expected);
}

TEST_F(PipelineImplTest, SetVolume) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  SetDemuxerExpectations(&streams);
  SetRendererExpectations();

  // The audio renderer should receive a call to SetVolume().
  float expected = 0.5f;
  EXPECT_CALL(*renderer_, SetVolume(expected));

  // Initialize then set volume!
  StartPipelineAndExpect(PIPELINE_OK);
  pipeline_->SetVolume(expected);
}

TEST_F(PipelineImplTest, Properties) {
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  const base::TimeDelta kDuration = base::TimeDelta::FromSeconds(100);
  SetDemuxerExpectations(&streams, kDuration);
  SetRendererExpectations();

  StartPipelineAndExpect(PIPELINE_OK);
  EXPECT_EQ(kDuration.ToInternalValue(),
            pipeline_->GetMediaDuration().ToInternalValue());
  EXPECT_FALSE(pipeline_->DidLoadingProgress());
}

TEST_F(PipelineImplTest, GetBufferedTimeRanges) {
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  const base::TimeDelta kDuration = base::TimeDelta::FromSeconds(100);
  SetDemuxerExpectations(&streams, kDuration);
  SetRendererExpectations();

  StartPipelineAndExpect(PIPELINE_OK);
  RunBufferedTimeRangesTest(kDuration / 8);

  base::TimeDelta kSeekTime = kDuration / 2;
  ExpectSeek(kSeekTime, false);
  DoSeek(kSeekTime);

  EXPECT_FALSE(pipeline_->DidLoadingProgress());
}

TEST_F(PipelineImplTest, BufferedTimeRangesCanChangeAfterStop) {
  EXPECT_CALL(*demuxer_, Initialize(_, _, _))
      .WillOnce(PostCallback<1>(PIPELINE_OK));
  EXPECT_CALL(*demuxer_, Stop());

  EXPECT_CALL(callbacks_, OnStart(_));
  StartPipeline();

  EXPECT_CALL(callbacks_, OnStop());
  pipeline_->Stop(
      base::Bind(&CallbackHelper::OnStop, base::Unretained(&callbacks_)));
  message_loop_.RunUntilIdle();

  RunBufferedTimeRangesTest(base::TimeDelta::FromSeconds(5));
  DestroyPipeline();
}

TEST_F(PipelineImplTest, EndedCallback) {
  CreateAudioStream();
  CreateVideoStream();
  CreateTextStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  SetDemuxerExpectations(&streams);
  SetRendererExpectations();
  StartPipelineAndExpect(PIPELINE_OK);

  AddTextStream();

  // The ended callback shouldn't run until all renderers have ended.
  ended_cb_.Run();
  message_loop_.RunUntilIdle();

  EXPECT_CALL(callbacks_, OnEnded());
  text_stream()->SendEosNotification();
  message_loop_.RunUntilIdle();
}

TEST_F(PipelineImplTest, ErrorDuringSeek) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  SetDemuxerExpectations(&streams);
  SetRendererExpectations();
  StartPipelineAndExpect(PIPELINE_OK);

  double playback_rate = 1.0;
  EXPECT_CALL(*renderer_, SetPlaybackRate(playback_rate));
  pipeline_->SetPlaybackRate(playback_rate);
  message_loop_.RunUntilIdle();

  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(5);

  EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));
  EXPECT_CALL(*renderer_, Flush(_))
      .WillOnce(
          DoAll(SetBufferingState(&buffering_state_cb_, BUFFERING_HAVE_NOTHING),
                RunClosure<0>()));

  EXPECT_CALL(*demuxer_, Seek(seek_time, _))
      .WillOnce(RunCallback<1>(PIPELINE_ERROR_READ));
  EXPECT_CALL(*demuxer_, Stop());

  pipeline_->Seek(seek_time, base::Bind(&CallbackHelper::OnSeek,
                                        base::Unretained(&callbacks_)));
  EXPECT_CALL(callbacks_, OnSeek(PIPELINE_ERROR_READ));
  message_loop_.RunUntilIdle();
}

// Invoked function OnError. This asserts that the pipeline does not enqueue
// non-teardown related tasks while tearing down.
static void TestNoCallsAfterError(PipelineImpl* pipeline,
                                  base::MessageLoop* message_loop,
                                  PipelineStatus /* status */) {
  CHECK(pipeline);
  CHECK(message_loop);

  // When we get to this stage, the message loop should be empty.
  EXPECT_TRUE(message_loop->IsIdleForTesting());

  // Make calls on pipeline after error has occurred.
  pipeline->SetPlaybackRate(0.5);
  pipeline->SetVolume(0.5f);

  // No additional tasks should be queued as a result of these calls.
  EXPECT_TRUE(message_loop->IsIdleForTesting());
}

TEST_F(PipelineImplTest, NoMessageDuringTearDownFromError) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  SetDemuxerExpectations(&streams);
  SetRendererExpectations();
  StartPipelineAndExpect(PIPELINE_OK);

  // Trigger additional requests on the pipeline during tear down from error.
  base::Callback<void(PipelineStatus)> cb =
      base::Bind(&TestNoCallsAfterError, pipeline_.get(), &message_loop_);
  ON_CALL(callbacks_, OnError(_))
      .WillByDefault(Invoke(&cb, &base::Callback<void(PipelineStatus)>::Run));

  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(5);

  // Seek() isn't called as the demuxer errors out first.
  EXPECT_CALL(*renderer_, Flush(_))
      .WillOnce(
          DoAll(SetBufferingState(&buffering_state_cb_, BUFFERING_HAVE_NOTHING),
                RunClosure<0>()));
  EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));

  EXPECT_CALL(*demuxer_, Seek(seek_time, _))
      .WillOnce(RunCallback<1>(PIPELINE_ERROR_READ));
  EXPECT_CALL(*demuxer_, Stop());

  pipeline_->Seek(seek_time, base::Bind(&CallbackHelper::OnSeek,
                                        base::Unretained(&callbacks_)));
  EXPECT_CALL(callbacks_, OnSeek(PIPELINE_ERROR_READ));
  message_loop_.RunUntilIdle();
}

TEST_F(PipelineImplTest, DestroyAfterStop) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  SetDemuxerExpectations(&streams);
  SetRendererExpectations();
  StartPipelineAndExpect(PIPELINE_OK);

  ExpectDemuxerStop();

  ExpectPipelineStopAndDestroyPipeline();
  pipeline_->Stop(
      base::Bind(&CallbackHelper::OnStop, base::Unretained(&callbacks_)));
  message_loop_.RunUntilIdle();
}

TEST_F(PipelineImplTest, Underflow) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  SetDemuxerExpectations(&streams);
  SetRendererExpectations();
  StartPipelineAndExpect(PIPELINE_OK);

  // Simulate underflow.
  EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));
  buffering_state_cb_.Run(BUFFERING_HAVE_NOTHING);

  // Seek while underflowed.
  base::TimeDelta expected = base::TimeDelta::FromSeconds(5);
  ExpectSeek(expected, true);
  DoSeek(expected);
}

TEST_F(PipelineImplTest, PositiveStartTime) {
  start_time_ = base::TimeDelta::FromSeconds(1);
  EXPECT_CALL(*demuxer_, GetStartTime()).WillRepeatedly(Return(start_time_));
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  SetDemuxerExpectations(&streams);
  SetRendererExpectations();
  StartPipelineAndExpect(PIPELINE_OK);
  ExpectDemuxerStop();
  ExpectPipelineStopAndDestroyPipeline();
  pipeline_->Stop(
      base::Bind(&CallbackHelper::OnStop, base::Unretained(&callbacks_)));
  message_loop_.RunUntilIdle();
}

class PipelineTeardownTest : public PipelineImplTest {
 public:
  enum TeardownState {
    kInitDemuxer,
    kInitRenderer,
    kFlushing,
    kSeeking,
    kPlaying,
    kSuspending,
    kSuspended,
    kResuming,
  };

  enum StopOrError {
    kStop,
    kError,
    kErrorAndStop,
  };

  PipelineTeardownTest() {}
  ~PipelineTeardownTest() override {}

  void RunTest(TeardownState state, StopOrError stop_or_error) {
    switch (state) {
      case kInitDemuxer:
      case kInitRenderer:
        DoInitialize(state, stop_or_error);
        break;

      case kFlushing:
      case kSeeking:
        DoInitialize(state, stop_or_error);
        DoSeek(state, stop_or_error);
        break;

      case kPlaying:
        DoInitialize(state, stop_or_error);
        DoStopOrError(stop_or_error, true);
        break;

      case kSuspending:
      case kSuspended:
      case kResuming:
        DoInitialize(state, stop_or_error);
        DoSuspend(state, stop_or_error);
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
    StartPipeline();
    message_loop_.RunUntilIdle();
  }

  PipelineStatus SetInitializeExpectations(TeardownState state,
                                           StopOrError stop_or_error) {
    PipelineStatus status = PIPELINE_OK;
    base::Closure stop_cb =
        base::Bind(&CallbackHelper::OnStop, base::Unretained(&callbacks_));

    if (state == kInitDemuxer) {
      if (stop_or_error == kStop) {
        EXPECT_CALL(*demuxer_, Initialize(_, _, _))
            .WillOnce(DoAll(Stop(pipeline_.get(), stop_cb),
                            PostCallback<1>(PIPELINE_OK)));
        ExpectPipelineStopAndDestroyPipeline();
      } else {
        status = DEMUXER_ERROR_COULD_NOT_OPEN;
        EXPECT_CALL(*demuxer_, Initialize(_, _, _))
            .WillOnce(PostCallback<1>(status));
      }

      EXPECT_CALL(*demuxer_, Stop());
      return status;
    }

    CreateAudioStream();
    CreateVideoStream();
    MockDemuxerStreamVector streams;
    streams.push_back(audio_stream());
    streams.push_back(video_stream());
    SetDemuxerExpectations(&streams, base::TimeDelta::FromSeconds(3000));

    EXPECT_CALL(*renderer_, HasAudio()).WillRepeatedly(Return(true));
    EXPECT_CALL(*renderer_, HasVideo()).WillRepeatedly(Return(true));

    if (state == kInitRenderer) {
      if (stop_or_error == kStop) {
        EXPECT_CALL(*renderer_, Initialize(_, _, _, _, _, _, _))
            .WillOnce(DoAll(Stop(pipeline_.get(), stop_cb),
                            PostCallback<1>(PIPELINE_OK)));
        ExpectPipelineStopAndDestroyPipeline();
      } else {
        status = PIPELINE_ERROR_INITIALIZATION_FAILED;
        EXPECT_CALL(*renderer_, Initialize(_, _, _, _, _, _, _))
            .WillOnce(PostCallback<1>(status));
      }

      EXPECT_CALL(*demuxer_, Stop());
      EXPECT_CALL(callbacks_, OnMetadata(_));
      return status;
    }

    EXPECT_CALL(*renderer_, Initialize(_, _, _, _, _, _, _))
        .WillOnce(DoAll(SaveArg<3>(&buffering_state_cb_),
                        PostCallback<1>(PIPELINE_OK)));

    EXPECT_CALL(callbacks_, OnMetadata(_));

    // If we get here it's a successful initialization.
    EXPECT_CALL(*renderer_, SetPlaybackRate(0.0));
    EXPECT_CALL(*renderer_, SetVolume(1.0f));
    EXPECT_CALL(*renderer_, StartPlayingFrom(base::TimeDelta()))
        .WillOnce(
            SetBufferingState(&buffering_state_cb_, BUFFERING_HAVE_ENOUGH));

    if (status == PIPELINE_OK)
      EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));

    return status;
  }

  void DoSeek(TeardownState state, StopOrError stop_or_error) {
    InSequence s;
    PipelineStatus status = SetSeekExpectations(state, stop_or_error);

    EXPECT_CALL(*demuxer_, Stop());
    EXPECT_CALL(callbacks_, OnSeek(status));

    if (status == PIPELINE_OK) {
      ExpectPipelineStopAndDestroyPipeline();
    }

    pipeline_->Seek(
        base::TimeDelta::FromSeconds(10),
        base::Bind(&CallbackHelper::OnSeek, base::Unretained(&callbacks_)));
    message_loop_.RunUntilIdle();
  }

  PipelineStatus SetSeekExpectations(TeardownState state,
                                     StopOrError stop_or_error) {
    PipelineStatus status = PIPELINE_OK;
    base::Closure stop_cb =
        base::Bind(&CallbackHelper::OnStop, base::Unretained(&callbacks_));

    if (state == kFlushing) {
      if (stop_or_error == kStop) {
        EXPECT_CALL(*renderer_, Flush(_))
            .WillOnce(DoAll(
                Stop(pipeline_.get(), stop_cb),
                SetBufferingState(&buffering_state_cb_, BUFFERING_HAVE_NOTHING),
                RunClosure<0>()));
        EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));
      } else {
        status = PIPELINE_ERROR_READ;
        EXPECT_CALL(*renderer_, Flush(_))
            .WillOnce(DoAll(
                SetError(pipeline_.get(), status),
                SetBufferingState(&buffering_state_cb_, BUFFERING_HAVE_NOTHING),
                RunClosure<0>()));
        EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));
      }

      return status;
    }

    EXPECT_CALL(*renderer_, Flush(_))
        .WillOnce(DoAll(
            SetBufferingState(&buffering_state_cb_, BUFFERING_HAVE_NOTHING),
            RunClosure<0>()));
    EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));

    if (state == kSeeking) {
      if (stop_or_error == kStop) {
        EXPECT_CALL(*demuxer_, Seek(_, _))
            .WillOnce(DoAll(Stop(pipeline_.get(), stop_cb),
                            RunCallback<1>(PIPELINE_OK)));
      } else {
        status = PIPELINE_ERROR_READ;
        EXPECT_CALL(*demuxer_, Seek(_, _)).WillOnce(RunCallback<1>(status));
      }

      return status;
    }

    NOTREACHED() << "State not supported: " << state;
    return status;
  }

  void DoSuspend(TeardownState state, StopOrError stop_or_error) {
    PipelineStatus status = SetSuspendExpectations(state, stop_or_error);

    if (state == kResuming) {
      EXPECT_CALL(*demuxer_, Stop());
      if (status == PIPELINE_OK)
        ExpectPipelineStopAndDestroyPipeline();
    }

    PipelineImplTest::DoSuspend();

    if (state == kResuming) {
      PipelineImplTest::DoResume(base::TimeDelta());
      return;
    }

    // kSuspended, kSuspending never throw errors, since Resume() is always able
    // to restore the pipeline to a pristine state.
    DoStopOrError(stop_or_error, false);
  }

  PipelineStatus SetSuspendExpectations(TeardownState state,
                                        StopOrError stop_or_error) {
    PipelineStatus status = PIPELINE_OK;
    base::Closure stop_cb =
        base::Bind(&CallbackHelper::OnStop, base::Unretained(&callbacks_));

    EXPECT_CALL(*renderer_, SetPlaybackRate(0));
    EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));
    EXPECT_CALL(callbacks_, OnSuspend(PIPELINE_OK));
    EXPECT_CALL(*renderer_, Flush(_))
        .WillOnce(DoAll(
            SetBufferingState(&buffering_state_cb_, BUFFERING_HAVE_NOTHING),
            RunClosure<0>()));
    if (state == kResuming) {
      if (stop_or_error == kStop) {
        EXPECT_CALL(*demuxer_, Seek(_, _))
            .WillOnce(DoAll(Stop(pipeline_.get(), stop_cb),
                            RunCallback<1>(PIPELINE_OK)));
        EXPECT_CALL(callbacks_, OnResume(PIPELINE_OK));
      } else {
        status = PIPELINE_ERROR_READ;
        EXPECT_CALL(*demuxer_, Seek(_, _)).WillOnce(RunCallback<1>(status));
        EXPECT_CALL(callbacks_, OnResume(status));
      }
    } else if (state != kSuspended && state != kSuspending) {
      NOTREACHED() << "State not supported: " << state;
    }

    return status;
  }

  void DoStopOrError(StopOrError stop_or_error, bool expect_errors) {
    InSequence s;

    switch (stop_or_error) {
      case kStop:
        EXPECT_CALL(*demuxer_, Stop());
        ExpectPipelineStopAndDestroyPipeline();
        pipeline_->Stop(
            base::Bind(&CallbackHelper::OnStop, base::Unretained(&callbacks_)));
        break;

      case kError:
        if (expect_errors) {
          EXPECT_CALL(*demuxer_, Stop());
          EXPECT_CALL(callbacks_, OnError(PIPELINE_ERROR_READ));
        }
        pipeline_->SetErrorForTesting(PIPELINE_ERROR_READ);
        break;

      case kErrorAndStop:
        EXPECT_CALL(*demuxer_, Stop());
        if (expect_errors)
          EXPECT_CALL(callbacks_, OnError(PIPELINE_ERROR_READ));
        ExpectPipelineStopAndDestroyPipeline();
        pipeline_->SetErrorForTesting(PIPELINE_ERROR_READ);
        message_loop_.RunUntilIdle();
        pipeline_->Stop(
            base::Bind(&CallbackHelper::OnStop, base::Unretained(&callbacks_)));
        break;
    }

    message_loop_.RunUntilIdle();
  }

  DISALLOW_COPY_AND_ASSIGN(PipelineTeardownTest);
};

#define INSTANTIATE_TEARDOWN_TEST(stop_or_error, state)   \
  TEST_F(PipelineTeardownTest, stop_or_error##_##state) { \
    RunTest(k##state, k##stop_or_error);                  \
  }

INSTANTIATE_TEARDOWN_TEST(Stop, InitDemuxer);
INSTANTIATE_TEARDOWN_TEST(Stop, InitRenderer);
INSTANTIATE_TEARDOWN_TEST(Stop, Flushing);
INSTANTIATE_TEARDOWN_TEST(Stop, Seeking);
INSTANTIATE_TEARDOWN_TEST(Stop, Playing);
INSTANTIATE_TEARDOWN_TEST(Stop, Suspending);
INSTANTIATE_TEARDOWN_TEST(Stop, Suspended);
INSTANTIATE_TEARDOWN_TEST(Stop, Resuming);

INSTANTIATE_TEARDOWN_TEST(Error, InitDemuxer);
INSTANTIATE_TEARDOWN_TEST(Error, InitRenderer);
INSTANTIATE_TEARDOWN_TEST(Error, Flushing);
INSTANTIATE_TEARDOWN_TEST(Error, Seeking);
INSTANTIATE_TEARDOWN_TEST(Error, Playing);
INSTANTIATE_TEARDOWN_TEST(Error, Suspending);
INSTANTIATE_TEARDOWN_TEST(Error, Suspended);
INSTANTIATE_TEARDOWN_TEST(Error, Resuming);

INSTANTIATE_TEARDOWN_TEST(ErrorAndStop, Playing);
INSTANTIATE_TEARDOWN_TEST(ErrorAndStop, Suspended);

}  // namespace media
