// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/threading/simple_thread.h"
#include "media/base/clock.h"
#include "media/base/filter_host.h"
#include "media/base/filters.h"
#include "media/base/media_log.h"
#include "media/base/pipeline.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filters.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size.h"

using ::testing::_;
using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace media {

// Demuxer properties.
static const int kTotalBytes = 1024;
static const int kBufferedBytes = 1024;
static const int kBitrate = 1234;

ACTION_P(InitializeDemuxerWithError, error) {
  arg1.Run(error);
}

ACTION_P(SetDemuxerProperties, duration) {
  arg0->SetTotalBytes(kTotalBytes);
  arg0->SetDuration(duration);
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
  MOCK_METHOD1(OnEnded, void(PipelineStatus));
  MOCK_METHOD1(OnError, void(PipelineStatus));

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
      : pipeline_(new Pipeline(&message_loop_, new MediaLog())) {
    mocks_.reset(new MockFilterCollection());

    // InitializeDemuxer() adds overriding expectations for expected non-NULL
    // streams.
    DemuxerStream* null_pointer = NULL;
    EXPECT_CALL(*mocks_->demuxer(), GetStream(_))
        .WillRepeatedly(Return(null_pointer));

    EXPECT_CALL(*mocks_->demuxer(), GetStartTime())
        .WillRepeatedly(Return(base::TimeDelta()));
  }

  virtual ~PipelineTest() {
    if (!pipeline_->IsRunning()) {
      return;
    }

    // Expect a stop callback if we were started.
    EXPECT_CALL(callbacks_, OnStop());
    pipeline_->Stop(base::Bind(&CallbackHelper::OnStop,
                               base::Unretained(&callbacks_)));
    message_loop_.RunAllPending();

    pipeline_ = NULL;
    mocks_.reset();
  }

 protected:
  // Sets up expectations to allow the demuxer to initialize.
  typedef std::vector<MockDemuxerStream*> MockDemuxerStreamVector;
  void InitializeDemuxer(MockDemuxerStreamVector* streams,
                         const base::TimeDelta& duration) {
    EXPECT_CALL(*mocks_->demuxer(), Initialize(_, _))
        .WillOnce(DoAll(SetDemuxerProperties(duration),
                        Invoke(&RunPipelineStatusCB2)));
    EXPECT_CALL(*mocks_->demuxer(), SetPlaybackRate(0.0f));
    EXPECT_CALL(*mocks_->demuxer(), Stop(_))
        .WillOnce(Invoke(&RunClosure));

    // Demuxer properties.
    EXPECT_CALL(*mocks_->demuxer(), GetBitrate())
        .WillRepeatedly(Return(kBitrate));

    // Configure the demuxer to return the streams.
    for (size_t i = 0; i < streams->size(); ++i) {
      scoped_refptr<DemuxerStream> stream((*streams)[i]);
      EXPECT_CALL(*mocks_->demuxer(), GetStream(stream->type()))
          .WillRepeatedly(Return(stream));
    }
  }

  void InitializeDemuxer(MockDemuxerStreamVector* streams) {
    // Initialize with a default non-zero duration.
    InitializeDemuxer(streams, base::TimeDelta::FromSeconds(10));
  }

  StrictMock<MockDemuxerStream>* CreateStream(DemuxerStream::Type type) {
    StrictMock<MockDemuxerStream>* stream =
        new StrictMock<MockDemuxerStream>();
    EXPECT_CALL(*stream, type())
        .WillRepeatedly(Return(type));
    return stream;
  }

  // Sets up expectations to allow the video decoder to initialize.
  void InitializeVideoDecoder(const scoped_refptr<DemuxerStream>& stream) {
    EXPECT_CALL(*mocks_->video_decoder(),
                Initialize(stream, _, _))
        .WillOnce(Invoke(&RunPipelineStatusCB3));
  }

  // Sets up expectations to allow the audio decoder to initialize.
  void InitializeAudioDecoder(const scoped_refptr<DemuxerStream>& stream) {
    EXPECT_CALL(*mocks_->audio_decoder(), Initialize(stream, _, _))
        .WillOnce(Invoke(&RunPipelineStatusCB3));
  }

  // Sets up expectations to allow the video renderer to initialize.
  void InitializeVideoRenderer() {
    EXPECT_CALL(*mocks_->video_renderer(), Initialize(
        scoped_refptr<VideoDecoder>(mocks_->video_decoder()), _, _, _))
        .WillOnce(Invoke(&RunPipelineStatusCB4));
    EXPECT_CALL(*mocks_->video_renderer(), SetPlaybackRate(0.0f));
    EXPECT_CALL(*mocks_->video_renderer(),
                Seek(mocks_->demuxer()->GetStartTime(), _))
        .WillOnce(Invoke(&RunPipelineStatusCB2));
    EXPECT_CALL(*mocks_->video_renderer(), Stop(_))
        .WillOnce(Invoke(&RunClosure));
  }

  // Sets up expectations to allow the audio renderer to initialize.
  void InitializeAudioRenderer(bool disable_after_init_cb = false) {
    if (disable_after_init_cb) {
      EXPECT_CALL(*mocks_->audio_renderer(), Initialize(
          scoped_refptr<AudioDecoder>(mocks_->audio_decoder()), _, _, _))
          .WillOnce(DoAll(Invoke(&RunPipelineStatusCB4),
                          DisableAudioRenderer(mocks_->audio_renderer())));
    } else {
      EXPECT_CALL(*mocks_->audio_renderer(), Initialize(
          scoped_refptr<AudioDecoder>(mocks_->audio_decoder()), _, _, _))
          .WillOnce(Invoke(&RunPipelineStatusCB4));
    }
    EXPECT_CALL(*mocks_->audio_renderer(), SetPlaybackRate(0.0f));
    EXPECT_CALL(*mocks_->audio_renderer(), SetVolume(1.0f));
    EXPECT_CALL(*mocks_->audio_renderer(), Seek(base::TimeDelta(), _))
        .WillOnce(Invoke(&RunPipelineStatusCB2));
    EXPECT_CALL(*mocks_->audio_renderer(), Stop(_))
        .WillOnce(Invoke(&RunClosure));
  }

  // Sets up expectations on the callback and initializes the pipeline.  Called
  // after tests have set expectations any filters they wish to use.
  void InitializePipeline(PipelineStatus start_status) {
    EXPECT_CALL(callbacks_, OnStart(start_status));

    pipeline_->Start(
        mocks_->Create().Pass(),
        base::Bind(&CallbackHelper::OnEnded, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnError, base::Unretained(&callbacks_)),
        NetworkEventCB(),
        base::Bind(&CallbackHelper::OnStart, base::Unretained(&callbacks_)));
    message_loop_.RunAllPending();
  }

  void CreateAudioStream() {
    audio_stream_ = CreateStream(DemuxerStream::AUDIO);
  }

  void CreateVideoStream() {
    video_stream_ = CreateStream(DemuxerStream::VIDEO);
  }

  MockDemuxerStream* audio_stream() {
    return audio_stream_;
  }

  MockDemuxerStream* video_stream() {
    return video_stream_;
  }

  void ExpectSeek(const base::TimeDelta& seek_time) {
    // Every filter should receive a call to Seek().
    EXPECT_CALL(*mocks_->demuxer(), Seek(seek_time, _))
        .WillOnce(Invoke(&RunPipelineStatusCB2));

    if (audio_stream_) {
      EXPECT_CALL(*mocks_->audio_renderer(), Seek(seek_time, _))
          .WillOnce(Invoke(&RunPipelineStatusCB2));
    }

    if (video_stream_) {
      EXPECT_CALL(*mocks_->video_renderer(), Seek(seek_time, _))
          .WillOnce(Invoke(&RunPipelineStatusCB2));
    }

    // We expect a successful seek callback.
    EXPECT_CALL(callbacks_, OnSeek(PIPELINE_OK));
  }

  void DoSeek(const base::TimeDelta& seek_time) {
    pipeline_->Seek(seek_time,
                    base::Bind(&CallbackHelper::OnSeek,
                               base::Unretained(&callbacks_)));

    // We expect the time to be updated only after the seek has completed.
    EXPECT_NE(seek_time, pipeline_->GetCurrentTime());
    message_loop_.RunAllPending();
    EXPECT_EQ(seek_time, pipeline_->GetCurrentTime());
  }

  // Fixture members.
  StrictMock<CallbackHelper> callbacks_;
  MessageLoop message_loop_;
  scoped_refptr<Pipeline> pipeline_;
  scoped_ptr<media::MockFilterCollection> mocks_;
  scoped_refptr<StrictMock<MockDemuxerStream> > audio_stream_;
  scoped_refptr<StrictMock<MockDemuxerStream> > video_stream_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PipelineTest);
};

// Test that playback controls methods no-op when the pipeline hasn't been
// started.
TEST_F(PipelineTest, NotStarted) {
  const base::TimeDelta kZero;

  EXPECT_FALSE(pipeline_->IsRunning());
  EXPECT_FALSE(pipeline_->IsInitialized());
  EXPECT_FALSE(pipeline_->HasAudio());
  EXPECT_FALSE(pipeline_->HasVideo());

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

  EXPECT_TRUE(kZero == pipeline_->GetCurrentTime());
  EXPECT_EQ(0u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_TRUE(kZero == pipeline_->GetMediaDuration());

  EXPECT_EQ(0, pipeline_->GetBufferedBytes());
  EXPECT_EQ(0, pipeline_->GetTotalBytes());

  // Should always get set to zero.
  gfx::Size size(1, 1);
  pipeline_->GetNaturalVideoSize(&size);
  EXPECT_EQ(0, size.width());
  EXPECT_EQ(0, size.height());
}

TEST_F(PipelineTest, NeverInitializes) {
  // Don't execute the callback passed into Initialize().
  EXPECT_CALL(*mocks_->demuxer(), Initialize(_, _));
  EXPECT_CALL(*mocks_->demuxer(), Stop(_))
      .WillOnce(Invoke(&RunClosure));

  // This test hangs during initialization by never calling
  // InitializationComplete().  StrictMock<> will ensure that the callback is
  // never executed.
  pipeline_->Start(
        mocks_->Create().Pass(),
        base::Bind(&CallbackHelper::OnEnded, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnError, base::Unretained(&callbacks_)),
        NetworkEventCB(),
        base::Bind(&CallbackHelper::OnStart, base::Unretained(&callbacks_)));
  message_loop_.RunAllPending();

  EXPECT_FALSE(pipeline_->IsInitialized());

  // Because our callback will get executed when the test tears down, we'll
  // verify that nothing has been called, then set our expectation for the call
  // made during tear down.
  Mock::VerifyAndClear(&callbacks_);
  EXPECT_CALL(callbacks_, OnStart(PIPELINE_OK));
}

TEST_F(PipelineTest, RequiredFilterMissing) {
  // Create a filter collection with missing filter.
  scoped_ptr<FilterCollection> collection(mocks_->Create());
  collection->SetDemuxer(NULL);

  EXPECT_CALL(callbacks_, OnStart(PIPELINE_ERROR_REQUIRED_FILTER_MISSING));
  pipeline_->Start(
      collection.Pass(),
      base::Bind(&CallbackHelper::OnEnded, base::Unretained(&callbacks_)),
      base::Bind(&CallbackHelper::OnError, base::Unretained(&callbacks_)),
      NetworkEventCB(),
      base::Bind(&CallbackHelper::OnStart, base::Unretained(&callbacks_)));
  message_loop_.RunAllPending();
  EXPECT_FALSE(pipeline_->IsInitialized());
}

TEST_F(PipelineTest, URLNotFound) {
  EXPECT_CALL(*mocks_->demuxer(), Initialize(_, _))
      .WillOnce(InitializeDemuxerWithError(PIPELINE_ERROR_URL_NOT_FOUND));
  EXPECT_CALL(*mocks_->demuxer(), Stop(_))
      .WillOnce(Invoke(&RunClosure));

  InitializePipeline(PIPELINE_ERROR_URL_NOT_FOUND);
  EXPECT_FALSE(pipeline_->IsInitialized());
}

TEST_F(PipelineTest, NoStreams) {
  EXPECT_CALL(*mocks_->demuxer(), Initialize(_, _))
      .WillOnce(Invoke(&RunPipelineStatusCB2));
  EXPECT_CALL(*mocks_->demuxer(), Stop(_))
      .WillOnce(Invoke(&RunClosure));

  InitializePipeline(PIPELINE_ERROR_COULD_NOT_RENDER);
  EXPECT_FALSE(pipeline_->IsInitialized());
}

TEST_F(PipelineTest, AudioStream) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  InitializeDemuxer(&streams);
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer();

  InitializePipeline(PIPELINE_OK);
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_TRUE(pipeline_->HasAudio());
  EXPECT_FALSE(pipeline_->HasVideo());
}

TEST_F(PipelineTest, VideoStream) {
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  InitializeDemuxer(&streams);
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();

  InitializePipeline(PIPELINE_OK);
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_FALSE(pipeline_->HasAudio());
  EXPECT_TRUE(pipeline_->HasVideo());
}

TEST_F(PipelineTest, AudioVideoStream) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  InitializeDemuxer(&streams);
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer();
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();

  InitializePipeline(PIPELINE_OK);
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_TRUE(pipeline_->HasAudio());
  EXPECT_TRUE(pipeline_->HasVideo());
}

TEST_F(PipelineTest, Seek) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  InitializeDemuxer(&streams, base::TimeDelta::FromSeconds(3000));
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer();
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();

  // Every filter should receive a call to Seek().
  base::TimeDelta expected = base::TimeDelta::FromSeconds(2000);
  ExpectSeek(expected);

  // Initialize then seek!
  InitializePipeline(PIPELINE_OK);
  DoSeek(expected);
}

TEST_F(PipelineTest, SetVolume) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  InitializeDemuxer(&streams);
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer();

  // The audio renderer should receive a call to SetVolume().
  float expected = 0.5f;
  EXPECT_CALL(*mocks_->audio_renderer(), SetVolume(expected));

  // Initialize then set volume!
  InitializePipeline(PIPELINE_OK);
  pipeline_->SetVolume(expected);
}

TEST_F(PipelineTest, Properties) {
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  const base::TimeDelta kDuration = base::TimeDelta::FromSeconds(100);
  InitializeDemuxer(&streams, kDuration);
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();

  InitializePipeline(PIPELINE_OK);
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_EQ(kDuration.ToInternalValue(),
            pipeline_->GetMediaDuration().ToInternalValue());
  EXPECT_EQ(kTotalBytes, pipeline_->GetTotalBytes());
  EXPECT_EQ(0, pipeline_->GetBufferedBytes());
}

TEST_F(PipelineTest, GetBufferedTimeRanges) {
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  const base::TimeDelta kDuration = base::TimeDelta::FromSeconds(100);
  InitializeDemuxer(&streams, kDuration);
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();

  InitializePipeline(PIPELINE_OK);
  EXPECT_TRUE(pipeline_->IsInitialized());

  EXPECT_EQ(0u, pipeline_->GetBufferedTimeRanges().size());

  pipeline_->SetBufferedBytes(kTotalBytes / 8);
  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(base::TimeDelta(), pipeline_->GetBufferedTimeRanges().start(0));
  EXPECT_EQ(kDuration / 8, pipeline_->GetBufferedTimeRanges().end(0));

  base::TimeDelta kSeekTime = kDuration / 2;
  ExpectSeek(kSeekTime);
  DoSeek(kSeekTime);

  pipeline_->SetBufferedBytes(kTotalBytes / 2 + kTotalBytes / 8);
  EXPECT_EQ(2u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(base::TimeDelta(), pipeline_->GetBufferedTimeRanges().start(0));
  EXPECT_EQ(kDuration / 8, pipeline_->GetBufferedTimeRanges().end(0));
  EXPECT_EQ(kDuration / 2, pipeline_->GetBufferedTimeRanges().start(1));
  EXPECT_EQ(kDuration / 2 + kDuration / 8,
            pipeline_->GetBufferedTimeRanges().end(1));
}

TEST_F(PipelineTest, DisableAudioRenderer) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  InitializeDemuxer(&streams);
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer();
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();

  InitializePipeline(PIPELINE_OK);
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_TRUE(pipeline_->HasAudio());
  EXPECT_TRUE(pipeline_->HasVideo());

  EXPECT_CALL(*mocks_->audio_renderer(), SetPlaybackRate(1.0f))
      .WillOnce(DisableAudioRenderer(mocks_->audio_renderer()));
  EXPECT_CALL(*mocks_->demuxer(),
              OnAudioRendererDisabled());
  EXPECT_CALL(*mocks_->audio_renderer(),
              OnAudioRendererDisabled());
  EXPECT_CALL(*mocks_->video_renderer(),
              OnAudioRendererDisabled());

  mocks_->audio_renderer()->SetPlaybackRate(1.0f);

  // Verify that ended event is fired when video ends.
  EXPECT_CALL(*mocks_->video_renderer(), HasEnded())
      .WillOnce(Return(true));
  EXPECT_CALL(callbacks_, OnEnded(PIPELINE_OK));
  FilterHost* host = pipeline_;
  host->NotifyEnded();
}

TEST_F(PipelineTest, DisableAudioRendererDuringInit) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  InitializeDemuxer(&streams);
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer(true);
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();

  EXPECT_CALL(*mocks_->demuxer(),
              OnAudioRendererDisabled());
  EXPECT_CALL(*mocks_->audio_renderer(),
              OnAudioRendererDisabled());
  EXPECT_CALL(*mocks_->video_renderer(),
              OnAudioRendererDisabled());

  InitializePipeline(PIPELINE_OK);
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_FALSE(pipeline_->HasAudio());
  EXPECT_TRUE(pipeline_->HasVideo());

  // Verify that ended event is fired when video ends.
  EXPECT_CALL(*mocks_->video_renderer(), HasEnded())
      .WillOnce(Return(true));
  EXPECT_CALL(callbacks_, OnEnded(PIPELINE_OK));
  FilterHost* host = pipeline_;
  host->NotifyEnded();
}

TEST_F(PipelineTest, EndedCallback) {
  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  InitializeDemuxer(&streams);
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer();
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();
  InitializePipeline(PIPELINE_OK);

  // For convenience to simulate filters calling the methods.
  FilterHost* host = pipeline_;

  // Due to short circuit evaluation we only need to test a subset of cases.
  InSequence s;
  EXPECT_CALL(*mocks_->audio_renderer(), HasEnded())
      .WillOnce(Return(false));
  host->NotifyEnded();

  EXPECT_CALL(*mocks_->audio_renderer(), HasEnded())
      .WillOnce(Return(true));
  EXPECT_CALL(*mocks_->video_renderer(), HasEnded())
      .WillOnce(Return(false));
  host->NotifyEnded();

  EXPECT_CALL(*mocks_->audio_renderer(), HasEnded())
      .WillOnce(Return(true));
  EXPECT_CALL(*mocks_->video_renderer(), HasEnded())
      .WillOnce(Return(true));
  EXPECT_CALL(callbacks_, OnEnded(PIPELINE_OK));
  host->NotifyEnded();
}

// Static function & time variable used to simulate changes in wallclock time.
static int64 g_static_clock_time;
static base::Time StaticClockFunction() {
  return base::Time::FromInternalValue(g_static_clock_time);
}

TEST_F(PipelineTest, AudioStreamShorterThanVideo) {
  base::TimeDelta duration = base::TimeDelta::FromSeconds(10);

  CreateAudioStream();
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());
  streams.push_back(video_stream());

  // Replace the clock so we can simulate wallclock time advancing w/o using
  // Sleep().
  pipeline_->SetClockForTesting(new Clock(&StaticClockFunction));

  InitializeDemuxer(&streams, duration);
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer();
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();
  InitializePipeline(PIPELINE_OK);

  // For convenience to simulate filters calling the methods.
  FilterHost* host = pipeline_;

  EXPECT_EQ(0, host->GetTime().ToInternalValue());

  float playback_rate = 1.0f;
  EXPECT_CALL(*mocks_->demuxer(), SetPlaybackRate(playback_rate));
  EXPECT_CALL(*mocks_->video_renderer(), SetPlaybackRate(playback_rate));
  EXPECT_CALL(*mocks_->audio_renderer(), SetPlaybackRate(playback_rate));
  pipeline_->SetPlaybackRate(playback_rate);
  message_loop_.RunAllPending();

  InSequence s;

  // Verify that the clock doesn't advance since it hasn't been started by
  // a time update from the audio stream.
  int64 start_time = host->GetTime().ToInternalValue();
  g_static_clock_time +=
      base::TimeDelta::FromMilliseconds(100).ToInternalValue();
  EXPECT_EQ(host->GetTime().ToInternalValue(), start_time);

  // Signal end of audio stream.
  EXPECT_CALL(*mocks_->audio_renderer(), HasEnded())
      .WillOnce(Return(true));
  EXPECT_CALL(*mocks_->video_renderer(), HasEnded())
      .WillOnce(Return(false));
  host->NotifyEnded();
  message_loop_.RunAllPending();

  // Verify that the clock advances.
  start_time = host->GetTime().ToInternalValue();
  g_static_clock_time +=
      base::TimeDelta::FromMilliseconds(100).ToInternalValue();
  EXPECT_GT(host->GetTime().ToInternalValue(), start_time);

  // Signal end of video stream and make sure OnEnded() callback occurs.
  EXPECT_CALL(*mocks_->audio_renderer(), HasEnded())
      .WillOnce(Return(true));
  EXPECT_CALL(*mocks_->video_renderer(), HasEnded())
      .WillOnce(Return(true));
  EXPECT_CALL(callbacks_, OnEnded(PIPELINE_OK));
  host->NotifyEnded();
}

void SendReadErrorToCB(::testing::Unused, const PipelineStatusCB& cb) {
  cb.Run(PIPELINE_ERROR_READ);
}

TEST_F(PipelineTest, ErrorDuringSeek) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  InitializeDemuxer(&streams, base::TimeDelta::FromSeconds(10));
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer();
  InitializePipeline(PIPELINE_OK);

  float playback_rate = 1.0f;
  EXPECT_CALL(*mocks_->demuxer(), SetPlaybackRate(playback_rate));
  EXPECT_CALL(*mocks_->audio_renderer(), SetPlaybackRate(playback_rate));
  pipeline_->SetPlaybackRate(playback_rate);
  message_loop_.RunAllPending();

  InSequence s;

  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(5);

  EXPECT_CALL(*mocks_->demuxer(), Seek(seek_time, _))
      .WillOnce(Invoke(&SendReadErrorToCB));

  pipeline_->Seek(seek_time, base::Bind(&CallbackHelper::OnSeek,
                                        base::Unretained(&callbacks_)));
  EXPECT_CALL(callbacks_, OnSeek(PIPELINE_ERROR_READ));
  message_loop_.RunAllPending();
}

// Invoked function OnError. This asserts that the pipeline does not enqueue
// non-teardown related tasks while tearing down.
static void TestNoCallsAfterError(
    Pipeline* pipeline, MessageLoop* message_loop,
    PipelineStatus /* status */) {
  CHECK(pipeline);
  CHECK(message_loop);

  // When we get to this stage, the message loop should be empty.
  message_loop->AssertIdle();

  // Make calls on pipeline after error has occurred.
  pipeline->SetPlaybackRate(0.5f);
  pipeline->SetVolume(0.5f);

  // No additional tasks should be queued as a result of these calls.
  message_loop->AssertIdle();
}

TEST_F(PipelineTest, NoMessageDuringTearDownFromError) {
  CreateAudioStream();
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream());

  InitializeDemuxer(&streams, base::TimeDelta::FromSeconds(10));
  InitializeAudioDecoder(audio_stream());
  InitializeAudioRenderer();
  InitializePipeline(PIPELINE_OK);

  // Trigger additional requests on the pipeline during tear down from error.
  base::Callback<void(PipelineStatus)> cb = base::Bind(
      &TestNoCallsAfterError, pipeline_, &message_loop_);
  ON_CALL(callbacks_, OnError(_))
      .WillByDefault(Invoke(&cb, &base::Callback<void(PipelineStatus)>::Run));

  InSequence s;

  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(5);

  EXPECT_CALL(*mocks_->demuxer(), Seek(seek_time, _))
      .WillOnce(Invoke(&SendReadErrorToCB));

  pipeline_->Seek(seek_time, base::Bind(&CallbackHelper::OnSeek,
                                        base::Unretained(&callbacks_)));
  EXPECT_CALL(callbacks_, OnSeek(PIPELINE_ERROR_READ));
  message_loop_.RunAllPending();
}

TEST_F(PipelineTest, StartTimeIsZero) {
  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  const base::TimeDelta kDuration = base::TimeDelta::FromSeconds(100);
  InitializeDemuxer(&streams, kDuration);
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();

  InitializePipeline(PIPELINE_OK);
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_FALSE(pipeline_->HasAudio());
  EXPECT_TRUE(pipeline_->HasVideo());

  EXPECT_EQ(base::TimeDelta(), pipeline_->GetCurrentTime());
}

TEST_F(PipelineTest, StartTimeIsNonZero) {
  const base::TimeDelta kStartTime = base::TimeDelta::FromSeconds(4);
  const base::TimeDelta kDuration = base::TimeDelta::FromSeconds(100);

  EXPECT_CALL(*mocks_->demuxer(), GetStartTime())
      .WillRepeatedly(Return(kStartTime));

  CreateVideoStream();
  MockDemuxerStreamVector streams;
  streams.push_back(video_stream());

  InitializeDemuxer(&streams, kDuration);
  InitializeVideoDecoder(video_stream());
  InitializeVideoRenderer();

  InitializePipeline(PIPELINE_OK);
  EXPECT_TRUE(pipeline_->IsInitialized());
  EXPECT_FALSE(pipeline_->HasAudio());
  EXPECT_TRUE(pipeline_->HasVideo());

  EXPECT_EQ(kStartTime, pipeline_->GetCurrentTime());
}

class FlexibleCallbackRunner : public base::DelegateSimpleThread::Delegate {
 public:
  FlexibleCallbackRunner(base::TimeDelta delay, PipelineStatus status,
                         const PipelineStatusCB& status_cb)
      : delay_(delay),
        status_(status),
        status_cb_(status_cb) {
    if (delay_ < base::TimeDelta()) {
      status_cb_.Run(status_);
      return;
    }
  }
  virtual void Run() {
    if (delay_ < base::TimeDelta()) return;
    base::PlatformThread::Sleep(delay_);
    status_cb_.Run(status_);
  }

 private:
  base::TimeDelta delay_;
  PipelineStatus status_;
  PipelineStatusCB status_cb_;
};

void TestPipelineStatusNotification(base::TimeDelta delay) {
  PipelineStatusNotification note;
  // Arbitrary error value we expect to fish out of the notification after the
  // callback is fired.
  const PipelineStatus expected_error = PIPELINE_ERROR_URL_NOT_FOUND;
  FlexibleCallbackRunner runner(delay, expected_error, note.Callback());
  base::DelegateSimpleThread thread(&runner, "FlexibleCallbackRunner");
  thread.Start();
  note.Wait();
  EXPECT_EQ(note.status(), expected_error);
  thread.Join();
}

// Test that in-line callback (same thread, no yield) works correctly.
TEST(PipelineStatusNotificationTest, InlineCallback) {
  TestPipelineStatusNotification(base::TimeDelta::FromMilliseconds(-1));
}

// Test that different-thread, no-delay callback works correctly.
TEST(PipelineStatusNotificationTest, ImmediateCallback) {
  TestPipelineStatusNotification(base::TimeDelta::FromMilliseconds(0));
}

// Test that different-thread, some-delay callback (the expected common case)
// works correctly.
TEST(PipelineStatusNotificationTest, DelayedCallback) {
  TestPipelineStatusNotification(base::TimeDelta::FromMilliseconds(20));
}

}  // namespace media
