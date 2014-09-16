// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/filters/renderer_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

const int64 kStartPlayingTimeInMs = 100;

ACTION_P2(SetBufferingState, cb, buffering_state) {
  cb->Run(buffering_state);
}

ACTION_P2(AudioError, cb, error) {
  cb->Run(error);
}

class RendererImplTest : public ::testing::Test {
 public:
  // Used for setting expectations on pipeline callbacks. Using a StrictMock
  // also lets us test for missing callbacks.
  class CallbackHelper {
   public:
    CallbackHelper() {}
    virtual ~CallbackHelper() {}

    MOCK_METHOD0(OnInitialize, void());
    MOCK_METHOD0(OnFlushed, void());
    MOCK_METHOD0(OnEnded, void());
    MOCK_METHOD1(OnError, void(PipelineStatus));
    MOCK_METHOD1(OnUpdateStatistics, void(const PipelineStatistics&));
    MOCK_METHOD1(OnBufferingStateChange, void(BufferingState));

   private:
    DISALLOW_COPY_AND_ASSIGN(CallbackHelper);
  };

  RendererImplTest()
      : demuxer_(new StrictMock<MockDemuxer>()),
        video_renderer_(new StrictMock<MockVideoRenderer>()),
        audio_renderer_(new StrictMock<MockAudioRenderer>()),
        renderer_impl_(
            new RendererImpl(message_loop_.message_loop_proxy(),
                             demuxer_.get(),
                             scoped_ptr<AudioRenderer>(audio_renderer_),
                             scoped_ptr<VideoRenderer>(video_renderer_))) {
    // SetDemuxerExpectations() adds overriding expectations for expected
    // non-NULL streams.
    DemuxerStream* null_pointer = NULL;
    EXPECT_CALL(*demuxer_, GetStream(_))
        .WillRepeatedly(Return(null_pointer));
    EXPECT_CALL(*demuxer_, GetLiveness())
        .WillRepeatedly(Return(Demuxer::LIVENESS_UNKNOWN));
  }

  virtual ~RendererImplTest() {
    renderer_impl_.reset();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  typedef std::vector<MockDemuxerStream*> MockDemuxerStreamVector;

  scoped_ptr<StrictMock<MockDemuxerStream> > CreateStream(
      DemuxerStream::Type type) {
    scoped_ptr<StrictMock<MockDemuxerStream> > stream(
        new StrictMock<MockDemuxerStream>(type));
    return stream.Pass();
  }

  // Sets up expectations to allow the audio renderer to initialize.
  void SetAudioRendererInitializeExpectations(PipelineStatus status) {
    EXPECT_CALL(*audio_renderer_,
                Initialize(audio_stream_.get(), _,  _, _, _, _))
        .WillOnce(DoAll(SaveArg<3>(&audio_buffering_state_cb_),
                        SaveArg<4>(&audio_ended_cb_),
                        SaveArg<5>(&audio_error_cb_),
                        RunCallback<1>(status)));
  }

  // Sets up expectations to allow the video renderer to initialize.
  void SetVideoRendererInitializeExpectations(PipelineStatus status) {
    EXPECT_CALL(*video_renderer_,
                Initialize(video_stream_.get(), _, _, _, _, _, _, _))
        .WillOnce(DoAll(SaveArg<4>(&video_buffering_state_cb_),
                        SaveArg<5>(&video_ended_cb_),
                        RunCallback<2>(status)));
  }

  void InitializeAndExpect(PipelineStatus start_status) {
    if (start_status != PIPELINE_OK)
      EXPECT_CALL(callbacks_, OnError(start_status));

    EXPECT_CALL(callbacks_, OnInitialize());

    if (start_status == PIPELINE_OK && audio_stream_) {
      EXPECT_CALL(*audio_renderer_, GetTimeSource())
          .WillOnce(Return(&time_source_));
    }

    renderer_impl_->Initialize(
        base::Bind(&CallbackHelper::OnInitialize,
                   base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnUpdateStatistics,
                   base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnEnded, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnError, base::Unretained(&callbacks_)),
        base::Bind(&CallbackHelper::OnBufferingStateChange,
                   base::Unretained(&callbacks_)));
    base::RunLoop().RunUntilIdle();
  }

  void CreateAudioStream() {
    audio_stream_ = CreateStream(DemuxerStream::AUDIO);
    streams_.push_back(audio_stream_.get());
    EXPECT_CALL(*demuxer_, GetStream(DemuxerStream::AUDIO))
        .WillRepeatedly(Return(audio_stream_.get()));
  }

  void CreateVideoStream() {
    video_stream_ = CreateStream(DemuxerStream::VIDEO);
    video_stream_->set_video_decoder_config(video_decoder_config_);
    streams_.push_back(video_stream_.get());
    EXPECT_CALL(*demuxer_, GetStream(DemuxerStream::VIDEO))
        .WillRepeatedly(Return(video_stream_.get()));
  }

  void CreateAudioAndVideoStream() {
    CreateAudioStream();
    CreateVideoStream();
  }

  void InitializeWithAudio() {
    CreateAudioStream();
    SetAudioRendererInitializeExpectations(PIPELINE_OK);
    InitializeAndExpect(PIPELINE_OK);
  }

  void InitializeWithVideo() {
    CreateVideoStream();
    SetVideoRendererInitializeExpectations(PIPELINE_OK);
    InitializeAndExpect(PIPELINE_OK);
  }

  void InitializeWithAudioAndVideo() {
    CreateAudioAndVideoStream();
    SetAudioRendererInitializeExpectations(PIPELINE_OK);
    SetVideoRendererInitializeExpectations(PIPELINE_OK);
    InitializeAndExpect(PIPELINE_OK);
  }

  void Play() {
    DCHECK(audio_stream_ || video_stream_);
    EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));

    base::TimeDelta start_time(
        base::TimeDelta::FromMilliseconds(kStartPlayingTimeInMs));

    if (audio_stream_) {
      EXPECT_CALL(time_source_, SetMediaTime(start_time));
      EXPECT_CALL(time_source_, StartTicking());
      EXPECT_CALL(*audio_renderer_, StartPlaying())
          .WillOnce(SetBufferingState(&audio_buffering_state_cb_,
                                      BUFFERING_HAVE_ENOUGH));
    }

    if (video_stream_) {
      EXPECT_CALL(*video_renderer_, StartPlayingFrom(start_time))
          .WillOnce(SetBufferingState(&video_buffering_state_cb_,
                                      BUFFERING_HAVE_ENOUGH));
    }

    renderer_impl_->StartPlayingFrom(start_time);
    base::RunLoop().RunUntilIdle();
  }

  void Flush(bool underflowed) {
    if (audio_stream_) {
      if (!underflowed)
        EXPECT_CALL(time_source_, StopTicking());
      EXPECT_CALL(*audio_renderer_, Flush(_))
          .WillOnce(DoAll(SetBufferingState(&audio_buffering_state_cb_,
                                            BUFFERING_HAVE_NOTHING),
                          RunClosure<0>()));
    }

    if (video_stream_) {
      EXPECT_CALL(*video_renderer_, Flush(_))
          .WillOnce(DoAll(SetBufferingState(&video_buffering_state_cb_,
                                            BUFFERING_HAVE_NOTHING),
                          RunClosure<0>()));
    }

    EXPECT_CALL(callbacks_, OnFlushed());

    renderer_impl_->Flush(
        base::Bind(&CallbackHelper::OnFlushed, base::Unretained(&callbacks_)));
    base::RunLoop().RunUntilIdle();
  }

  void SetPlaybackRate(float playback_rate) {
    EXPECT_CALL(time_source_, SetPlaybackRate(playback_rate));
    renderer_impl_->SetPlaybackRate(playback_rate);
    base::RunLoop().RunUntilIdle();
  }

  int64 GetMediaTimeMs() {
    return renderer_impl_->GetMediaTime().InMilliseconds();
  }

  bool IsMediaTimeAdvancing(float playback_rate) {
    int64 start_time_ms = GetMediaTimeMs();
    const int64 time_to_advance_ms = 100;

    test_tick_clock_.Advance(
        base::TimeDelta::FromMilliseconds(time_to_advance_ms));

    if (GetMediaTimeMs() == start_time_ms + time_to_advance_ms * playback_rate)
      return true;

    DCHECK_EQ(start_time_ms, GetMediaTimeMs());
    return false;
  }

  bool IsMediaTimeAdvancing() {
    return IsMediaTimeAdvancing(1.0f);
  }

  // Fixture members.
  base::MessageLoop message_loop_;
  StrictMock<CallbackHelper> callbacks_;
  base::SimpleTestTickClock test_tick_clock_;

  scoped_ptr<StrictMock<MockDemuxer> > demuxer_;
  StrictMock<MockVideoRenderer>* video_renderer_;
  StrictMock<MockAudioRenderer>* audio_renderer_;
  scoped_ptr<RendererImpl> renderer_impl_;

  StrictMock<MockTimeSource> time_source_;
  scoped_ptr<StrictMock<MockDemuxerStream> > audio_stream_;
  scoped_ptr<StrictMock<MockDemuxerStream> > video_stream_;
  MockDemuxerStreamVector streams_;
  BufferingStateCB audio_buffering_state_cb_;
  BufferingStateCB video_buffering_state_cb_;
  base::Closure audio_ended_cb_;
  base::Closure video_ended_cb_;
  PipelineStatusCB audio_error_cb_;
  VideoDecoderConfig video_decoder_config_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RendererImplTest);
};

TEST_F(RendererImplTest, DestroyBeforeInitialize) {
  // |renderer_impl_| will be destroyed in the dtor.
}

TEST_F(RendererImplTest, InitializeWithAudio) {
  InitializeWithAudio();
}

TEST_F(RendererImplTest, InitializeWithVideo) {
  InitializeWithVideo();
}

TEST_F(RendererImplTest, InitializeWithAudioVideo) {
  InitializeWithAudioAndVideo();
}

TEST_F(RendererImplTest, InitializeWithAudio_Failed) {
  CreateAudioStream();
  SetAudioRendererInitializeExpectations(PIPELINE_ERROR_INITIALIZATION_FAILED);
  InitializeAndExpect(PIPELINE_ERROR_INITIALIZATION_FAILED);
}

TEST_F(RendererImplTest, InitializeWithVideo_Failed) {
  CreateVideoStream();
  SetVideoRendererInitializeExpectations(PIPELINE_ERROR_INITIALIZATION_FAILED);
  InitializeAndExpect(PIPELINE_ERROR_INITIALIZATION_FAILED);
}

TEST_F(RendererImplTest, InitializeWithAudioVideo_AudioRendererFailed) {
  CreateAudioAndVideoStream();
  SetAudioRendererInitializeExpectations(PIPELINE_ERROR_INITIALIZATION_FAILED);
  // VideoRenderer::Initialize() should not be called.
  InitializeAndExpect(PIPELINE_ERROR_INITIALIZATION_FAILED);
}

TEST_F(RendererImplTest, InitializeWithAudioVideo_VideoRendererFailed) {
  CreateAudioAndVideoStream();
  SetAudioRendererInitializeExpectations(PIPELINE_OK);
  SetVideoRendererInitializeExpectations(PIPELINE_ERROR_INITIALIZATION_FAILED);
  InitializeAndExpect(PIPELINE_ERROR_INITIALIZATION_FAILED);
}

TEST_F(RendererImplTest, StartPlayingFrom) {
  InitializeWithAudioAndVideo();
  Play();
}

TEST_F(RendererImplTest, FlushAfterInitialization) {
  InitializeWithAudioAndVideo();
  Flush(true);
}

TEST_F(RendererImplTest, FlushAfterPlay) {
  InitializeWithAudioAndVideo();
  Play();
  Flush(false);
}

TEST_F(RendererImplTest, FlushAfterUnderflow) {
  InitializeWithAudioAndVideo();
  Play();

  // Simulate underflow.
  EXPECT_CALL(time_source_, StopTicking());
  audio_buffering_state_cb_.Run(BUFFERING_HAVE_NOTHING);

  // Flush while underflowed. We shouldn't call StopTicking() again.
  Flush(true);
}

TEST_F(RendererImplTest, SetPlaybackRate) {
  InitializeWithAudioAndVideo();
  SetPlaybackRate(1.0f);
  SetPlaybackRate(2.0f);
}

TEST_F(RendererImplTest, SetVolume) {
  InitializeWithAudioAndVideo();
  EXPECT_CALL(*audio_renderer_, SetVolume(2.0f));
  renderer_impl_->SetVolume(2.0f);
}

TEST_F(RendererImplTest, AudioStreamEnded) {
  InitializeWithAudio();
  Play();

  EXPECT_CALL(time_source_, StopTicking());
  EXPECT_CALL(callbacks_, OnEnded());

  audio_ended_cb_.Run();
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, VideoStreamEnded) {
  InitializeWithVideo();
  Play();

  // Video ended won't affect |time_source_|.
  EXPECT_CALL(callbacks_, OnEnded());

  video_ended_cb_.Run();
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, AudioVideoStreamsEnded) {
  InitializeWithAudioAndVideo();
  Play();

  // OnEnded() is called only when all streams have finished.
  audio_ended_cb_.Run();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(time_source_, StopTicking());
  EXPECT_CALL(callbacks_, OnEnded());

  video_ended_cb_.Run();
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, ErrorAfterInitialize) {
  InitializeWithAudio();
  EXPECT_CALL(callbacks_, OnError(PIPELINE_ERROR_DECODE));
  audio_error_cb_.Run(PIPELINE_ERROR_DECODE);
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, ErrorDuringPlaying) {
  InitializeWithAudio();
  Play();

  EXPECT_CALL(callbacks_, OnError(PIPELINE_ERROR_DECODE));
  audio_error_cb_.Run(PIPELINE_ERROR_DECODE);
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, ErrorDuringFlush) {
  InitializeWithAudio();
  Play();

  InSequence s;
  EXPECT_CALL(time_source_, StopTicking());
  EXPECT_CALL(*audio_renderer_, Flush(_)).WillOnce(DoAll(
      AudioError(&audio_error_cb_, PIPELINE_ERROR_DECODE),
      RunClosure<0>()));
  EXPECT_CALL(callbacks_, OnError(PIPELINE_ERROR_DECODE));
  EXPECT_CALL(callbacks_, OnFlushed());
  renderer_impl_->Flush(
      base::Bind(&CallbackHelper::OnFlushed, base::Unretained(&callbacks_)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, ErrorAfterFlush) {
  InitializeWithAudio();
  Play();
  Flush(false);

  EXPECT_CALL(callbacks_, OnError(PIPELINE_ERROR_DECODE));
  audio_error_cb_.Run(PIPELINE_ERROR_DECODE);
  base::RunLoop().RunUntilIdle();
}

}  // namespace media
