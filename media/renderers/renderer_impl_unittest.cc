// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/renderers/renderer_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace media {

const int64_t kStartPlayingTimeInMs = 100;

ACTION_P2(SetBool, var, value) {
  *var = value;
}

ACTION_P2(SetBufferingState, renderer_client, buffering_state) {
  (*renderer_client)->OnBufferingStateChange(buffering_state);
}

ACTION_P2(SetError, renderer_client, error) {
  (*renderer_client)->OnError(error);
}

ACTION(PostCallback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, arg0);
}

ACTION(PostQuitWhenIdle) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

class RendererImplTest : public ::testing::Test {
 public:
  // Used for setting expectations on pipeline callbacks. Using a StrictMock
  // also lets us test for missing callbacks.
  class CallbackHelper : public MockRendererClient {
   public:
    CallbackHelper() {}
    virtual ~CallbackHelper() {}

    // Completion callbacks.
    MOCK_METHOD1(OnInitialize, void(PipelineStatus));
    MOCK_METHOD0(OnFlushed, void());
    MOCK_METHOD1(OnCdmAttached, void(bool));
    MOCK_METHOD1(OnDurationChange, void(base::TimeDelta duration));

   private:
    DISALLOW_COPY_AND_ASSIGN(CallbackHelper);
  };

  RendererImplTest()
      : demuxer_(new StrictMock<MockDemuxer>()),
        video_renderer_(new StrictMock<MockVideoRenderer>()),
        audio_renderer_(new StrictMock<MockAudioRenderer>()),
        renderer_impl_(
            new RendererImpl(message_loop_.task_runner(),
                             std::unique_ptr<AudioRenderer>(audio_renderer_),
                             std::unique_ptr<VideoRenderer>(video_renderer_))),
        cdm_context_(new StrictMock<MockCdmContext>()),
        video_renderer_client_(nullptr),
        audio_renderer_client_(nullptr),
        initialization_status_(PIPELINE_OK) {
    // CreateAudioStream() and CreateVideoStream() overrides expectations for
    // expected non-NULL streams.
    EXPECT_CALL(*demuxer_, GetAllStreams()).WillRepeatedly(Return(streams_));
  }

  virtual ~RendererImplTest() { Destroy(); }

 protected:
  void Destroy() {
    renderer_impl_.reset();
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<StrictMock<MockDemuxerStream>> CreateStream(
      DemuxerStream::Type type) {
    std::unique_ptr<StrictMock<MockDemuxerStream>> stream(
        new StrictMock<MockDemuxerStream>(type));
    EXPECT_CALL(*demuxer_, SetStreamStatusChangeCB(_))
        .Times(testing::AnyNumber());
    return stream;
  }

  // Sets up expectations to allow the audio renderer to initialize.
  void SetAudioRendererInitializeExpectations(PipelineStatus status) {
    EXPECT_CALL(*audio_renderer_, Initialize(audio_stream_.get(), _, _, _))
        .WillOnce(
            DoAll(SaveArg<2>(&audio_renderer_client_), RunCallback<3>(status)));
  }

  // Sets up expectations to allow the video renderer to initialize.
  void SetVideoRendererInitializeExpectations(PipelineStatus status) {
    EXPECT_CALL(*video_renderer_, Initialize(video_stream_.get(), _, _, _, _))
        .WillOnce(
            DoAll(SaveArg<2>(&video_renderer_client_), RunCallback<4>(status)));
  }

  void InitializeAndExpect(PipelineStatus start_status) {
    EXPECT_CALL(callbacks_, OnInitialize(start_status))
        .WillOnce(SaveArg<0>(&initialization_status_));
    EXPECT_CALL(callbacks_, OnWaitingForDecryptionKey()).Times(0);

    if (start_status == PIPELINE_OK && audio_stream_) {
      EXPECT_CALL(*audio_renderer_, GetTimeSource())
          .WillOnce(Return(&time_source_));
    } else {
      renderer_impl_->set_time_source_for_testing(&time_source_);
    }

    renderer_impl_->Initialize(demuxer_.get(), &callbacks_,
                               base::Bind(&CallbackHelper::OnInitialize,
                                          base::Unretained(&callbacks_)));
    base::RunLoop().RunUntilIdle();

    if (start_status == PIPELINE_OK && audio_stream_) {
      ON_CALL(*audio_renderer_, Flush(_))
          .WillByDefault(DoAll(SetBufferingState(&audio_renderer_client_,
                                                 BUFFERING_HAVE_NOTHING),
                               RunClosure<0>()));
      ON_CALL(*audio_renderer_, StartPlaying())
          .WillByDefault(SetBufferingState(&audio_renderer_client_,
                                           BUFFERING_HAVE_ENOUGH));
    }
    if (start_status == PIPELINE_OK && video_stream_) {
      ON_CALL(*video_renderer_, Flush(_))
          .WillByDefault(DoAll(SetBufferingState(&video_renderer_client_,
                                                 BUFFERING_HAVE_NOTHING),
                               RunClosure<0>()));
      ON_CALL(*video_renderer_, StartPlayingFrom(_))
          .WillByDefault(SetBufferingState(&video_renderer_client_,
                                           BUFFERING_HAVE_ENOUGH));
    }
  }

  void CreateAudioStream() {
    audio_stream_ = CreateStream(DemuxerStream::AUDIO);
    streams_.push_back(audio_stream_.get());
    EXPECT_CALL(*demuxer_, GetAllStreams()).WillRepeatedly(Return(streams_));
  }

  void CreateVideoStream(bool is_encrypted = false) {
    video_stream_ = CreateStream(DemuxerStream::VIDEO);
    video_stream_->set_video_decoder_config(
        is_encrypted ? TestVideoConfig::NormalEncrypted()
                     : TestVideoConfig::Normal());
    streams_.push_back(video_stream_.get());
    EXPECT_CALL(*demuxer_, GetAllStreams()).WillRepeatedly(Return(streams_));
  }

  void CreateEncryptedVideoStream() { CreateVideoStream(true); }

  void CreateAudioAndVideoStream() {
    CreateAudioStream();
    CreateVideoStream();
  }

  void InitializeWithAudio() {
    CreateAudioStream();
    SetAudioRendererInitializeExpectations(PIPELINE_OK);
    // There is a potential race between HTMLMediaElement/WMPI shutdown and
    // renderers being initialized which might result in MediaResource
    // GetAllStreams suddenly returning fewer streams than before or even
    // returning
    // and empty stream collection (see crbug.com/668604). So we are going to
    // check here that GetAllStreams will be invoked exactly 3 times during
    // RendererImpl initialization to help catch potential issues. Currently the
    // GetAllStreams is invoked once from the RendererImpl::Initialize via
    // HasEncryptedStream, once from the RendererImpl::InitializeAudioRenderer
    // and once from the RendererImpl::InitializeVideoRenderer.
    EXPECT_CALL(*demuxer_, GetAllStreams())
        .Times(3)
        .WillRepeatedly(Return(streams_));
    InitializeAndExpect(PIPELINE_OK);
  }

  void InitializeWithVideo() {
    CreateVideoStream();
    SetVideoRendererInitializeExpectations(PIPELINE_OK);
    // There is a potential race between HTMLMediaElement/WMPI shutdown and
    // renderers being initialized which might result in MediaResource
    // GetAllStreams suddenly returning fewer streams than before or even
    // returning
    // and empty stream collection (see crbug.com/668604). So we are going to
    // check here that GetAllStreams will be invoked exactly 3 times during
    // RendererImpl initialization to help catch potential issues. Currently the
    // GetAllStreams is invoked once from the RendererImpl::Initialize via
    // HasEncryptedStream, once from the RendererImpl::InitializeAudioRenderer
    // and once from the RendererImpl::InitializeVideoRenderer.
    EXPECT_CALL(*demuxer_, GetAllStreams())
        .Times(3)
        .WillRepeatedly(Return(streams_));
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
    EXPECT_CALL(time_source_, SetMediaTime(start_time));
    EXPECT_CALL(time_source_, StartTicking());

    if (audio_stream_) {
      EXPECT_CALL(*audio_renderer_, StartPlaying());
    }

    if (video_stream_) {
      EXPECT_CALL(*video_renderer_, StartPlayingFrom(start_time));
    }

    renderer_impl_->StartPlayingFrom(start_time);
    base::RunLoop().RunUntilIdle();
  }

  void SetFlushExpectationsForAVRenderers() {
    if (audio_stream_)
      EXPECT_CALL(*audio_renderer_, Flush(_));

    if (video_stream_)
      EXPECT_CALL(*video_renderer_, Flush(_));
  }

  void Flush(bool underflowed) {
    if (!underflowed)
      EXPECT_CALL(time_source_, StopTicking());

    SetFlushExpectationsForAVRenderers();
    EXPECT_CALL(callbacks_, OnFlushed());

    renderer_impl_->Flush(
        base::Bind(&CallbackHelper::OnFlushed, base::Unretained(&callbacks_)));
    base::RunLoop().RunUntilIdle();
  }

  void SetPlaybackRate(double playback_rate) {
    EXPECT_CALL(time_source_, SetPlaybackRate(playback_rate));
    renderer_impl_->SetPlaybackRate(playback_rate);
    base::RunLoop().RunUntilIdle();
  }

  int64_t GetMediaTimeMs() {
    return renderer_impl_->GetMediaTime().InMilliseconds();
  }

  bool IsMediaTimeAdvancing(double playback_rate) {
    int64_t start_time_ms = GetMediaTimeMs();
    const int64_t time_to_advance_ms = 100;

    test_tick_clock_.Advance(
        base::TimeDelta::FromMilliseconds(time_to_advance_ms));

    if (GetMediaTimeMs() == start_time_ms + time_to_advance_ms * playback_rate)
      return true;

    DCHECK_EQ(start_time_ms, GetMediaTimeMs());
    return false;
  }

  bool IsMediaTimeAdvancing() {
    return IsMediaTimeAdvancing(1.0);
  }

  void SetCdmAndExpect(bool expected_result) {
    EXPECT_CALL(callbacks_, OnCdmAttached(expected_result));
    renderer_impl_->SetCdm(cdm_context_.get(),
                           base::Bind(&CallbackHelper::OnCdmAttached,
                                      base::Unretained(&callbacks_)));
    base::RunLoop().RunUntilIdle();
  }

  // Fixture members.
  base::MessageLoop message_loop_;
  StrictMock<CallbackHelper> callbacks_;
  base::SimpleTestTickClock test_tick_clock_;

  std::unique_ptr<StrictMock<MockDemuxer>> demuxer_;
  StrictMock<MockVideoRenderer>* video_renderer_;
  StrictMock<MockAudioRenderer>* audio_renderer_;
  std::unique_ptr<RendererImpl> renderer_impl_;
  std::unique_ptr<StrictMock<MockCdmContext>> cdm_context_;

  StrictMock<MockTimeSource> time_source_;
  std::unique_ptr<StrictMock<MockDemuxerStream>> audio_stream_;
  std::unique_ptr<StrictMock<MockDemuxerStream>> video_stream_;
  std::vector<DemuxerStream*> streams_;
  RendererClient* video_renderer_client_;
  RendererClient* audio_renderer_client_;
  VideoDecoderConfig video_decoder_config_;
  PipelineStatus initialization_status_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RendererImplTest);
};

TEST_F(RendererImplTest, Destroy_BeforeInitialize) {
  Destroy();
}

TEST_F(RendererImplTest, Destroy_PendingInitialize) {
  CreateAudioAndVideoStream();

  SetAudioRendererInitializeExpectations(PIPELINE_OK);
  // Not returning the video initialization callback.
  EXPECT_CALL(*video_renderer_, Initialize(video_stream_.get(), _, _, _, _));

  InitializeAndExpect(PIPELINE_ERROR_ABORT);
  EXPECT_EQ(PIPELINE_OK, initialization_status_);

  Destroy();
}

TEST_F(RendererImplTest, Destroy_PendingInitializeWithoutCdm) {
  CreateAudioStream();
  CreateEncryptedVideoStream();

  // Audio is clear and video is encrypted. Initialization will not start
  // because no CDM is set. So neither AudioRenderer::Initialize() nor
  // VideoRenderer::Initialize() should not be called. The InitCB will be
  // aborted when |renderer_impl_| is destructed.
  InitializeAndExpect(PIPELINE_ERROR_ABORT);
  EXPECT_EQ(PIPELINE_OK, initialization_status_);

  Destroy();
}

TEST_F(RendererImplTest, Destroy_PendingInitializeAfterSetCdm) {
  CreateAudioStream();
  CreateEncryptedVideoStream();

  // Audio is clear and video is encrypted. Initialization will not start
  // because no CDM is set.
  InitializeAndExpect(PIPELINE_ERROR_ABORT);
  EXPECT_EQ(PIPELINE_OK, initialization_status_);

  SetAudioRendererInitializeExpectations(PIPELINE_OK);
  // Not returning the video initialization callback. So initialization will
  // be pending.
  EXPECT_CALL(*video_renderer_, Initialize(video_stream_.get(), _, _, _, _));

  // SetCdm() will trigger the initialization to start. But it will not complete
  // because the |video_renderer_| is not returning the initialization callback.
  SetCdmAndExpect(true);
  EXPECT_EQ(PIPELINE_OK, initialization_status_);

  Destroy();
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

TEST_F(RendererImplTest, SetCdmBeforeInitialize) {
  // CDM will be successfully attached immediately if set before RendererImpl
  // initialization, regardless of the later initialization result.
  SetCdmAndExpect(true);
}

TEST_F(RendererImplTest, SetCdmAfterInitialize_ClearStream) {
  InitializeWithAudioAndVideo();
  EXPECT_EQ(PIPELINE_OK, initialization_status_);

  // CDM will be successfully attached immediately since initialization is
  // completed.
  SetCdmAndExpect(true);
}

TEST_F(RendererImplTest, SetCdmAfterInitialize_EncryptedStream_Success) {
  CreateAudioStream();
  CreateEncryptedVideoStream();

  SetAudioRendererInitializeExpectations(PIPELINE_OK);
  SetVideoRendererInitializeExpectations(PIPELINE_OK);
  InitializeAndExpect(PIPELINE_OK);
  // Initialization is pending until CDM is set.
  EXPECT_EQ(PIPELINE_OK, initialization_status_);

  SetCdmAndExpect(true);
  EXPECT_EQ(PIPELINE_OK, initialization_status_);
}

TEST_F(RendererImplTest, SetCdmAfterInitialize_EncryptedStream_Failure) {
  CreateAudioStream();
  CreateEncryptedVideoStream();

  SetAudioRendererInitializeExpectations(PIPELINE_OK);
  SetVideoRendererInitializeExpectations(PIPELINE_ERROR_INITIALIZATION_FAILED);
  InitializeAndExpect(PIPELINE_ERROR_INITIALIZATION_FAILED);
  // Initialization is pending until CDM is set.
  EXPECT_EQ(PIPELINE_OK, initialization_status_);

  SetCdmAndExpect(true);
  EXPECT_EQ(PIPELINE_ERROR_INITIALIZATION_FAILED, initialization_status_);
}

TEST_F(RendererImplTest, SetCdmMultipleTimes) {
  SetCdmAndExpect(true);
  SetCdmAndExpect(false);  // Do not support switching CDM.
}

TEST_F(RendererImplTest, StartPlayingFrom) {
  InitializeWithAudioAndVideo();
  Play();
}

TEST_F(RendererImplTest, StartPlayingFromWithPlaybackRate) {
  InitializeWithAudioAndVideo();

  // Play with a zero playback rate shouldn't start time.
  Play();
  Mock::VerifyAndClearExpectations(video_renderer_);

  // Positive playback rate when ticking should start time.
  EXPECT_CALL(*video_renderer_, OnTimeProgressing());
  SetPlaybackRate(1.0);
  Mock::VerifyAndClearExpectations(video_renderer_);

  // Double notifications shouldn't be sent.
  SetPlaybackRate(1.0);
  Mock::VerifyAndClearExpectations(video_renderer_);

  // Zero playback rate should stop time.
  EXPECT_CALL(*video_renderer_, OnTimeStopped());
  SetPlaybackRate(0.0);
  Mock::VerifyAndClearExpectations(video_renderer_);

  // Double notifications shouldn't be sent.
  SetPlaybackRate(0.0);
  Mock::VerifyAndClearExpectations(video_renderer_);

  // Starting playback and flushing should cause time to stop.
  EXPECT_CALL(*video_renderer_, OnTimeProgressing());
  EXPECT_CALL(*video_renderer_, OnTimeStopped());
  SetPlaybackRate(1.0);
  Flush(false);

  // A positive playback rate when playback isn't started should do nothing.
  SetPlaybackRate(1.0);
}

TEST_F(RendererImplTest, FlushAfterInitialization) {
  InitializeWithAudioAndVideo();
  EXPECT_CALL(callbacks_, OnFlushed());
  renderer_impl_->Flush(
      base::Bind(&CallbackHelper::OnFlushed, base::Unretained(&callbacks_)));
  base::RunLoop().RunUntilIdle();
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
  EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));
  audio_renderer_client_->OnBufferingStateChange(BUFFERING_HAVE_NOTHING);

  // Flush while underflowed. We shouldn't call StopTicking() again.
  Flush(true);
}

TEST_F(RendererImplTest, SetPlaybackRate) {
  InitializeWithAudioAndVideo();
  SetPlaybackRate(1.0);
  SetPlaybackRate(2.0);
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

  audio_renderer_client_->OnEnded();
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, VideoStreamEnded) {
  InitializeWithVideo();
  Play();

  EXPECT_CALL(time_source_, StopTicking());
  EXPECT_CALL(callbacks_, OnEnded());
  EXPECT_CALL(*video_renderer_, OnTimeStopped());

  video_renderer_client_->OnEnded();
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, AudioVideoStreamsEnded) {
  InitializeWithAudioAndVideo();
  Play();

  // OnEnded() is called only when all streams have finished.
  audio_renderer_client_->OnEnded();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(time_source_, StopTicking());
  EXPECT_CALL(callbacks_, OnEnded());
  EXPECT_CALL(*video_renderer_, OnTimeStopped());

  video_renderer_client_->OnEnded();
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, ErrorAfterInitialize) {
  InitializeWithAudio();
  EXPECT_CALL(callbacks_, OnError(PIPELINE_ERROR_DECODE));
  audio_renderer_client_->OnError(PIPELINE_ERROR_DECODE);
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, ErrorDuringPlaying) {
  InitializeWithAudio();
  Play();

  EXPECT_CALL(callbacks_, OnError(PIPELINE_ERROR_DECODE));
  audio_renderer_client_->OnError(PIPELINE_ERROR_DECODE);
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, ErrorDuringFlush) {
  InitializeWithAudio();
  Play();

  InSequence s;
  EXPECT_CALL(time_source_, StopTicking());
  EXPECT_CALL(*audio_renderer_, Flush(_))
      .WillOnce(DoAll(SetError(&audio_renderer_client_, PIPELINE_ERROR_DECODE),
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
  audio_renderer_client_->OnError(PIPELINE_ERROR_DECODE);
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, ErrorDuringInitialize) {
  CreateAudioAndVideoStream();
  SetAudioRendererInitializeExpectations(PIPELINE_OK);

  // Force an audio error to occur during video renderer initialization.
  EXPECT_CALL(*video_renderer_, Initialize(video_stream_.get(), _, _, _, _))
      .WillOnce(DoAll(SetError(&audio_renderer_client_, PIPELINE_ERROR_DECODE),
                      SaveArg<2>(&video_renderer_client_),
                      RunCallback<4>(PIPELINE_OK)));

  InitializeAndExpect(PIPELINE_ERROR_DECODE);
}

TEST_F(RendererImplTest, AudioUnderflow) {
  InitializeWithAudio();
  Play();

  // Underflow should occur immediately with a single audio track.
  EXPECT_CALL(time_source_, StopTicking());
  EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));
  audio_renderer_client_->OnBufferingStateChange(BUFFERING_HAVE_NOTHING);
}

TEST_F(RendererImplTest, AudioUnderflowWithVideo) {
  InitializeWithAudioAndVideo();
  Play();

  // Underflow should be immediate when both audio and video are present and
  // audio underflows.
  EXPECT_CALL(time_source_, StopTicking());
  EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));
  audio_renderer_client_->OnBufferingStateChange(BUFFERING_HAVE_NOTHING);
}

TEST_F(RendererImplTest, VideoUnderflow) {
  InitializeWithVideo();
  Play();

  // Underflow should occur immediately with a single video track.
  EXPECT_CALL(time_source_, StopTicking());
  EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));
  video_renderer_client_->OnBufferingStateChange(BUFFERING_HAVE_NOTHING);
}

TEST_F(RendererImplTest, VideoUnderflowWithAudio) {
  InitializeWithAudioAndVideo();
  Play();

  // Set a zero threshold such that the underflow will be executed on the next
  // run of the message loop.
  renderer_impl_->set_video_underflow_threshold_for_testing(base::TimeDelta());

  // Underflow should be delayed when both audio and video are present and video
  // underflows.
  EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));
  video_renderer_client_->OnBufferingStateChange(BUFFERING_HAVE_NOTHING);
  Mock::VerifyAndClearExpectations(&time_source_);

  EXPECT_CALL(time_source_, StopTicking());
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, VideoUnderflowWithAudioVideoRecovers) {
  InitializeWithAudioAndVideo();
  Play();

  // Set a zero threshold such that the underflow will be executed on the next
  // run of the message loop.
  renderer_impl_->set_video_underflow_threshold_for_testing(base::TimeDelta());

  // Underflow should be delayed when both audio and video are present and video
  // underflows.
  EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING))
      .Times(0);
  video_renderer_client_->OnBufferingStateChange(BUFFERING_HAVE_NOTHING);
  Mock::VerifyAndClearExpectations(&time_source_);

  // If video recovers, the underflow should never occur.
  video_renderer_client_->OnBufferingStateChange(BUFFERING_HAVE_ENOUGH);
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, VideoAndAudioUnderflow) {
  InitializeWithAudioAndVideo();
  Play();

  // Set a zero threshold such that the underflow will be executed on the next
  // run of the message loop.
  renderer_impl_->set_video_underflow_threshold_for_testing(base::TimeDelta());

  // Underflow should be delayed when both audio and video are present and video
  // underflows.
  EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING))
      .Times(0);
  video_renderer_client_->OnBufferingStateChange(BUFFERING_HAVE_NOTHING);
  Mock::VerifyAndClearExpectations(&time_source_);

  EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));
  EXPECT_CALL(time_source_, StopTicking());
  audio_renderer_client_->OnBufferingStateChange(BUFFERING_HAVE_NOTHING);

  // Nothing else should primed on the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, VideoUnderflowWithAudioFlush) {
  InitializeWithAudioAndVideo();
  Play();

  // Set a massive threshold such that it shouldn't fire within this test.
  renderer_impl_->set_video_underflow_threshold_for_testing(
      base::TimeDelta::FromSeconds(100));

  // Simulate the cases where audio underflows and then video underflows.
  EXPECT_CALL(time_source_, StopTicking());
  EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));
  audio_renderer_client_->OnBufferingStateChange(BUFFERING_HAVE_NOTHING);
  video_renderer_client_->OnBufferingStateChange(BUFFERING_HAVE_NOTHING);
  Mock::VerifyAndClearExpectations(&time_source_);

  // Flush the audio and video renderers, both think they're in an underflow
  // state, but if the video renderer underflow was deferred, RendererImpl would
  // think it still has enough data.
  EXPECT_CALL(*audio_renderer_, Flush(_)).WillOnce(RunClosure<0>());
  EXPECT_CALL(*video_renderer_, Flush(_)).WillOnce(RunClosure<0>());
  EXPECT_CALL(callbacks_, OnFlushed());
  renderer_impl_->Flush(
      base::Bind(&CallbackHelper::OnFlushed, base::Unretained(&callbacks_)));
  base::RunLoop().RunUntilIdle();

  // Start playback after the flush, but never return BUFFERING_HAVE_ENOUGH from
  // the video renderer (which simulates spool up time for the video renderer).
  const base::TimeDelta kStartTime;
  EXPECT_CALL(time_source_, SetMediaTime(kStartTime));
  EXPECT_CALL(time_source_, StartTicking());
  EXPECT_CALL(*audio_renderer_, StartPlaying());
  EXPECT_CALL(*video_renderer_, StartPlayingFrom(kStartTime));
  EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
  renderer_impl_->StartPlayingFrom(kStartTime);

  // Nothing else should primed on the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererImplTest, StreamStatusNotificationHandling) {
  CreateAudioAndVideoStream();

  StreamStatusChangeCB stream_status_change_cb;
  EXPECT_CALL(*demuxer_, SetStreamStatusChangeCB(_))
      .WillOnce(SaveArg<0>(&stream_status_change_cb));
  SetAudioRendererInitializeExpectations(PIPELINE_OK);
  SetVideoRendererInitializeExpectations(PIPELINE_OK);
  InitializeAndExpect(PIPELINE_OK);
  Play();

  EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));

  // Verify that DemuxerStream status changes cause the corresponding
  // audio/video renderer to be flushed and restarted.
  EXPECT_CALL(time_source_, StopTicking());
  EXPECT_CALL(*audio_renderer_, Flush(_));
  EXPECT_CALL(*audio_renderer_, StartPlaying());
  EXPECT_CALL(time_source_, StartTicking());
  stream_status_change_cb.Run(audio_stream_.get(), false, base::TimeDelta());

  EXPECT_CALL(*video_renderer_, Flush(_));
  EXPECT_CALL(*video_renderer_, StartPlayingFrom(_));
  stream_status_change_cb.Run(video_stream_.get(), false, base::TimeDelta());
  base::RunLoop().RunUntilIdle();
}

// Stream status changes are handled asynchronously by the renderer and may take
// some time to process. This test verifies that all status changes are
// processed correctly by the renderer even if status changes of the stream
// happen much faster than the renderer can process them. In that case the
// renderer may postpone processing status changes, but still must process all
// of them eventually.
TEST_F(RendererImplTest, PostponedStreamStatusNotificationHandling) {
  CreateAudioAndVideoStream();

  StreamStatusChangeCB stream_status_change_cb;
  EXPECT_CALL(*demuxer_, SetStreamStatusChangeCB(_))
      .WillOnce(SaveArg<0>(&stream_status_change_cb));
  SetAudioRendererInitializeExpectations(PIPELINE_OK);
  SetVideoRendererInitializeExpectations(PIPELINE_OK);
  InitializeAndExpect(PIPELINE_OK);
  Play();

  EXPECT_CALL(callbacks_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH))
      .Times(2);

  EXPECT_CALL(time_source_, StopTicking()).Times(2);
  EXPECT_CALL(time_source_, StartTicking()).Times(2);
  EXPECT_CALL(*audio_renderer_, Flush(_)).Times(2);
  EXPECT_CALL(*audio_renderer_, StartPlaying()).Times(2);
  // The first stream status change will be processed immediately. Each status
  // change processing involves Flush + StartPlaying when the Flush is done. The
  // Flush operation is async in this case, so the second status change will be
  // postponed by renderer until after processing the first one is finished. But
  // we must still get two pairs of Flush/StartPlaying calls eventually.
  stream_status_change_cb.Run(audio_stream_.get(), false, base::TimeDelta());
  stream_status_change_cb.Run(audio_stream_.get(), true, base::TimeDelta());
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*video_renderer_, Flush(_)).Times(2);
  EXPECT_CALL(*video_renderer_, StartPlayingFrom(base::TimeDelta())).Times(2);
  // The first stream status change will be processed immediately. Each status
  // change processing involves Flush + StartPlaying when the Flush is done. The
  // Flush operation is async in this case, so the second status change will be
  // postponed by renderer until after processing the first one is finished. But
  // we must still get two pairs of Flush/StartPlaying calls eventually.
  stream_status_change_cb.Run(video_stream_.get(), false, base::TimeDelta());
  stream_status_change_cb.Run(video_stream_.get(), true, base::TimeDelta());
  base::RunLoop().RunUntilIdle();
}

// Verify that a RendererImpl::Flush gets postponed until after stream status
// change handling is completed.
TEST_F(RendererImplTest, FlushDuringAudioReinit) {
  CreateAudioAndVideoStream();

  StreamStatusChangeCB stream_status_change_cb;
  EXPECT_CALL(*demuxer_, SetStreamStatusChangeCB(_))
      .WillOnce(SaveArg<0>(&stream_status_change_cb));
  SetAudioRendererInitializeExpectations(PIPELINE_OK);
  SetVideoRendererInitializeExpectations(PIPELINE_OK);
  InitializeAndExpect(PIPELINE_OK);
  Play();

  EXPECT_CALL(time_source_, StopTicking()).Times(testing::AnyNumber());
  base::Closure audio_renderer_flush_cb;
  EXPECT_CALL(*audio_renderer_, Flush(_))
      .WillOnce(SaveArg<0>(&audio_renderer_flush_cb));
  EXPECT_CALL(*audio_renderer_, StartPlaying());

  // This should start flushing the audio renderer (due to audio stream status
  // change) and should populate the |audio_renderer_flush_cb|.
  stream_status_change_cb.Run(audio_stream_.get(), false, base::TimeDelta());
  EXPECT_TRUE(audio_renderer_flush_cb);
  base::RunLoop().RunUntilIdle();

  bool flush_done = false;

  // Now that audio stream change is being handled the RendererImpl::Flush
  // should be postponed, instead of being executed immediately.
  EXPECT_CALL(callbacks_, OnFlushed()).WillOnce(SetBool(&flush_done, true));
  renderer_impl_->Flush(
      base::Bind(&CallbackHelper::OnFlushed, base::Unretained(&callbacks_)));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(flush_done);

  // The renderer_impl_->Flush invoked above should proceed after the first
  // audio renderer flush (initiated by the stream status change) completes.
  SetFlushExpectationsForAVRenderers();
  audio_renderer_flush_cb.Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(flush_done);
}

// Verify that RendererImpl::Flush is not postponed by waiting for data.
TEST_F(RendererImplTest, FlushDuringAudioReinitWhileWaiting) {
  CreateAudioAndVideoStream();

  StreamStatusChangeCB stream_status_change_cb;
  EXPECT_CALL(*demuxer_, SetStreamStatusChangeCB(_))
      .WillOnce(SaveArg<0>(&stream_status_change_cb));
  SetAudioRendererInitializeExpectations(PIPELINE_OK);
  SetVideoRendererInitializeExpectations(PIPELINE_OK);
  InitializeAndExpect(PIPELINE_OK);
  Play();

  EXPECT_CALL(time_source_, StopTicking()).Times(testing::AnyNumber());
  base::Closure audio_renderer_flush_cb;

  // Override the standard buffering state transitions for Flush() and
  // StartPlaying() setup above.
  EXPECT_CALL(*audio_renderer_, Flush(_))
      .WillOnce(DoAll(
          SetBufferingState(&audio_renderer_client_, BUFFERING_HAVE_NOTHING),
          SaveArg<0>(&audio_renderer_flush_cb)));
  EXPECT_CALL(*audio_renderer_, StartPlaying())
      .WillOnce(RunClosure(base::Bind(&base::DoNothing)));

  // This should start flushing the audio renderer (due to audio stream status
  // change) and should populate the |audio_renderer_flush_cb|.
  stream_status_change_cb.Run(audio_stream_.get(), false, base::TimeDelta());
  EXPECT_TRUE(audio_renderer_flush_cb);
  base::RunLoop().RunUntilIdle();

  bool flush_done = false;

  // Complete the first half of the track change, it should be stuck in the
  // StartPlaying() state after this.
  EXPECT_CALL(callbacks_, OnFlushed()).WillOnce(SetBool(&flush_done, true));
  audio_renderer_flush_cb.Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(flush_done);

  // Ensure that even though we're stuck waiting for have_enough from the
  // audio renderer, that our flush still executes immediately.
  SetFlushExpectationsForAVRenderers();
  renderer_impl_->Flush(
      base::Bind(&CallbackHelper::OnFlushed, base::Unretained(&callbacks_)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(flush_done);
}

TEST_F(RendererImplTest, FlushDuringVideoReinit) {
  CreateAudioAndVideoStream();

  StreamStatusChangeCB stream_status_change_cb;
  EXPECT_CALL(*demuxer_, SetStreamStatusChangeCB(_))
      .WillOnce(SaveArg<0>(&stream_status_change_cb));
  SetAudioRendererInitializeExpectations(PIPELINE_OK);
  SetVideoRendererInitializeExpectations(PIPELINE_OK);
  InitializeAndExpect(PIPELINE_OK);
  Play();

  EXPECT_CALL(time_source_, StopTicking()).Times(testing::AnyNumber());
  base::Closure video_renderer_flush_cb;
  EXPECT_CALL(*video_renderer_, Flush(_))
      .WillOnce(SaveArg<0>(&video_renderer_flush_cb));
  EXPECT_CALL(*video_renderer_, StartPlayingFrom(_));

  // This should start flushing the video renderer (due to video stream status
  // change) and should populate the |video_renderer_flush_cb|.
  stream_status_change_cb.Run(video_stream_.get(), false, base::TimeDelta());
  EXPECT_TRUE(video_renderer_flush_cb);
  base::RunLoop().RunUntilIdle();

  bool flush_done = false;

  // Now that video stream change is being handled the RendererImpl::Flush
  // should be postponed, instead of being executed immediately.
  EXPECT_CALL(callbacks_, OnFlushed()).WillOnce(SetBool(&flush_done, true));
  renderer_impl_->Flush(
      base::Bind(&CallbackHelper::OnFlushed, base::Unretained(&callbacks_)));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(flush_done);

  // The renderer_impl_->Flush invoked above should proceed after the first
  // video renderer flush (initiated by the stream status change) completes.
  SetFlushExpectationsForAVRenderers();
  video_renderer_flush_cb.Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(flush_done);
}

// Verify that RendererImpl::Flush is not postponed by waiting for data.
TEST_F(RendererImplTest, FlushDuringVideoReinitWhileWaiting) {
  CreateAudioAndVideoStream();

  StreamStatusChangeCB stream_status_change_cb;
  EXPECT_CALL(*demuxer_, SetStreamStatusChangeCB(_))
      .WillOnce(SaveArg<0>(&stream_status_change_cb));
  SetAudioRendererInitializeExpectations(PIPELINE_OK);
  SetVideoRendererInitializeExpectations(PIPELINE_OK);
  InitializeAndExpect(PIPELINE_OK);
  Play();

  EXPECT_CALL(time_source_, StopTicking()).Times(testing::AnyNumber());
  base::Closure video_renderer_flush_cb;

  // Override the standard buffering state transitions for Flush() and
  // StartPlaying() setup above.
  EXPECT_CALL(*video_renderer_, Flush(_))
      .WillOnce(DoAll(
          SetBufferingState(&video_renderer_client_, BUFFERING_HAVE_NOTHING),
          SaveArg<0>(&video_renderer_flush_cb)));
  EXPECT_CALL(*video_renderer_, StartPlayingFrom(_))
      .WillOnce(RunClosure(base::Bind(&base::DoNothing)));

  // This should start flushing the video renderer (due to video stream status
  // change) and should populate the |video_renderer_flush_cb|.
  stream_status_change_cb.Run(video_stream_.get(), false, base::TimeDelta());
  EXPECT_TRUE(video_renderer_flush_cb);
  base::RunLoop().RunUntilIdle();

  bool flush_done = false;

  // Complete the first half of the track change, it should be stuck in the
  // StartPlaying() state after this.
  EXPECT_CALL(callbacks_, OnFlushed()).WillOnce(SetBool(&flush_done, true));
  video_renderer_flush_cb.Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(flush_done);

  // Ensure that even though we're stuck waiting for have_enough from the
  // video renderer, that our flush still executes immediately.
  SetFlushExpectationsForAVRenderers();
  renderer_impl_->Flush(
      base::Bind(&CallbackHelper::OnFlushed, base::Unretained(&callbacks_)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(flush_done);
}

// Test audio track switching when the RendererImpl is in STATE_FLUSHING/FLUSHED
TEST_F(RendererImplTest, AudioTrackSwitchDuringFlush) {
  CreateAudioAndVideoStream();
  std::unique_ptr<StrictMock<MockDemuxerStream>> primary_audio_stream =
      std::move(audio_stream_);
  CreateAudioStream();
  std::unique_ptr<StrictMock<MockDemuxerStream>> secondary_audio_stream =
      std::move(audio_stream_);
  audio_stream_ = std::move(primary_audio_stream);

  StreamStatusChangeCB stream_status_change_cb;
  EXPECT_CALL(*demuxer_, SetStreamStatusChangeCB(_))
      .WillOnce(SaveArg<0>(&stream_status_change_cb));
  SetAudioRendererInitializeExpectations(PIPELINE_OK);
  SetVideoRendererInitializeExpectations(PIPELINE_OK);
  InitializeAndExpect(PIPELINE_OK);
  Play();

  EXPECT_CALL(time_source_, StopTicking()).Times(testing::AnyNumber());
  EXPECT_CALL(*video_renderer_, Flush(_));

  // Initiate RendererImpl::Flush, but postpone its completion by not calling
  // audio renderer flush callback right away, i.e. pretending audio renderer
  // flush takes a while.
  base::Closure audio_renderer_flush_cb;
  EXPECT_CALL(*audio_renderer_, Flush(_))
      .WillOnce(SaveArg<0>(&audio_renderer_flush_cb));
  EXPECT_CALL(callbacks_, OnFlushed());
  renderer_impl_->Flush(
      base::Bind(&CallbackHelper::OnFlushed, base::Unretained(&callbacks_)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(audio_renderer_flush_cb);

  // Now, while the RendererImpl::Flush is pending, perform an audio track
  // switch. The handling of the track switch will be postponed until after
  // RendererImpl::Flush completes.
  stream_status_change_cb.Run(audio_stream_.get(), false, base::TimeDelta());
  stream_status_change_cb.Run(secondary_audio_stream.get(), true,
                              base::TimeDelta());

  // Ensure that audio track switch occurs after Flush by verifying that the
  // audio renderer is reinitialized with the secondary audio stream.
  EXPECT_CALL(*audio_renderer_,
              Initialize(secondary_audio_stream.get(), _, _, _));

  // Complete the audio renderer flush, thus completing the renderer_impl_ Flush
  // initiated above. This will transition the RendererImpl into the FLUSHED
  // state and will process pending track switch, which should result in the
  // reinitialization of the audio renderer for the secondary audio stream.
  audio_renderer_client_->OnBufferingStateChange(BUFFERING_HAVE_NOTHING);
  audio_renderer_flush_cb.Run();
  base::RunLoop().RunUntilIdle();
}

// Test video track switching when the RendererImpl is in STATE_FLUSHING/FLUSHED
TEST_F(RendererImplTest, VideoTrackSwitchDuringFlush) {
  CreateAudioAndVideoStream();
  std::unique_ptr<StrictMock<MockDemuxerStream>> primary_video_stream =
      std::move(video_stream_);
  CreateVideoStream();
  std::unique_ptr<StrictMock<MockDemuxerStream>> secondary_video_stream =
      std::move(video_stream_);
  video_stream_ = std::move(primary_video_stream);

  StreamStatusChangeCB stream_status_change_cb;
  EXPECT_CALL(*demuxer_, SetStreamStatusChangeCB(_))
      .WillOnce(SaveArg<0>(&stream_status_change_cb));
  SetAudioRendererInitializeExpectations(PIPELINE_OK);
  SetVideoRendererInitializeExpectations(PIPELINE_OK);
  InitializeAndExpect(PIPELINE_OK);
  Play();

  EXPECT_CALL(time_source_, StopTicking()).Times(testing::AnyNumber());
  EXPECT_CALL(*video_renderer_, OnTimeStopped()).Times(testing::AnyNumber());
  EXPECT_CALL(*audio_renderer_, Flush(_));

  // Initiate RendererImpl::Flush, but postpone its completion by not calling
  // video renderer flush callback right away, i.e. pretending video renderer
  // flush takes a while.
  base::Closure video_renderer_flush_cb;
  EXPECT_CALL(*video_renderer_, Flush(_))
      .WillOnce(SaveArg<0>(&video_renderer_flush_cb));
  EXPECT_CALL(callbacks_, OnFlushed());
  renderer_impl_->Flush(
      base::Bind(&CallbackHelper::OnFlushed, base::Unretained(&callbacks_)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(video_renderer_flush_cb);

  // Now, while the RendererImpl::Flush is pending, perform a video track
  // switch. The handling of the track switch will be postponed until after
  // RendererImpl::Flush completes.
  stream_status_change_cb.Run(video_stream_.get(), false, base::TimeDelta());
  stream_status_change_cb.Run(secondary_video_stream.get(), true,
                              base::TimeDelta());

  // Ensure that video track switch occurs after Flush by verifying that the
  // video renderer is reinitialized with the secondary video stream.
  EXPECT_CALL(*video_renderer_,
              Initialize(secondary_video_stream.get(), _, _, _, _));

  // Complete the video renderer flush, thus completing the renderer_impl_ Flush
  // initiated above. This will transition the RendererImpl into the FLUSHED
  // state and will process pending track switch, which should result in the
  // reinitialization of the video renderer for the secondary video stream.
  video_renderer_client_->OnBufferingStateChange(BUFFERING_HAVE_NOTHING);
  video_renderer_flush_cb.Run();
  base::RunLoop().RunUntilIdle();
}

}  // namespace media
