// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_default.h"
#include "chromecast/media/cma/base/buffering_controller.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "chromecast/media/cma/pipeline/audio_pipeline_impl.h"
#include "chromecast/media/cma/pipeline/av_pipeline_client.h"
#include "chromecast/media/cma/pipeline/media_pipeline_impl.h"
#include "chromecast/media/cma/pipeline/video_pipeline_client.h"
#include "chromecast/media/cma/pipeline/video_pipeline_impl.h"
#include "chromecast/media/cma/test/frame_generator_for_test.h"
#include "chromecast/media/cma/test/mock_frame_provider.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "media/base/video_decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

class AudioVideoPipelineImplTest : public testing::Test {
 public:
  AudioVideoPipelineImplTest();
  ~AudioVideoPipelineImplTest() override;

  void Initialize(const base::Closure& done_cb,
                  ::media::PipelineStatus status,
                  bool is_audio);
  void StartPlaying(const base::Closure& done_cb,
                    ::media::PipelineStatus status);

  void Flush(const base::Closure& done_cb, ::media::PipelineStatus status);
  void Stop(const base::Closure& done_cb, ::media::PipelineStatus status);

  void OnEos();

  base::Closure task_after_eos_cb_;
  scoped_ptr<base::MessageLoop> message_loop_;

 private:
  scoped_ptr<TaskRunnerImpl> task_runner_;
  scoped_ptr<MediaPipelineImpl> media_pipeline_;

  DISALLOW_COPY_AND_ASSIGN(AudioVideoPipelineImplTest);
};

AudioVideoPipelineImplTest::AudioVideoPipelineImplTest()
    : message_loop_(new base::MessageLoop()),
      task_runner_(new TaskRunnerImpl()),
      media_pipeline_(new MediaPipelineImpl()) {
  scoped_ptr<MediaPipelineBackend> backend =
      make_scoped_ptr(new MediaPipelineBackendDefault());

  media_pipeline_->Initialize(kLoadTypeURL, std::move(backend));
  media_pipeline_->SetPlaybackRate(1.0);
}

AudioVideoPipelineImplTest::~AudioVideoPipelineImplTest() {
}

void AudioVideoPipelineImplTest::Initialize(
    const base::Closure& done_cb,
    ::media::PipelineStatus status,
    bool is_audio) {
  ::media::AudioDecoderConfig audio_config(
      ::media::kCodecMP3,
      ::media::kSampleFormatS16,
      ::media::CHANNEL_LAYOUT_STEREO,
      44100,
      ::media::EmptyExtraData(), false);
  std::vector<::media::VideoDecoderConfig> video_configs;
  video_configs.push_back(::media::VideoDecoderConfig(
      ::media::kCodecH264, ::media::H264PROFILE_MAIN,
      ::media::PIXEL_FORMAT_I420, ::media::COLOR_SPACE_UNSPECIFIED,
      gfx::Size(640, 480), gfx::Rect(0, 0, 640, 480), gfx::Size(640, 480),
      ::media::EmptyExtraData(), false));

  // Frame generation on the producer side.
  std::vector<FrameGeneratorForTest::FrameSpec> frame_specs;
  frame_specs.resize(100);
  for (size_t k = 0; k < frame_specs.size() - 1; k++) {
    frame_specs[k].has_config = (k == 0);
    frame_specs[k].timestamp = base::TimeDelta::FromMilliseconds(40) * k;
    frame_specs[k].size = 512;
    frame_specs[k].has_decrypt_config = false;
  }
  frame_specs[frame_specs.size() - 1].is_eos = true;

  scoped_ptr<FrameGeneratorForTest> frame_generator_provider(
      new FrameGeneratorForTest(frame_specs));
  bool provider_delayed_pattern[] = { true, false };
  scoped_ptr<MockFrameProvider> frame_provider(new MockFrameProvider());
  frame_provider->Configure(
      std::vector<bool>(
          provider_delayed_pattern,
          provider_delayed_pattern + arraysize(provider_delayed_pattern)),
      std::move(frame_generator_provider));

  ::media::PipelineStatusCB next_task =
      base::Bind(&AudioVideoPipelineImplTest::StartPlaying,
                 base::Unretained(this),
                 done_cb);

  scoped_ptr<CodedFrameProvider> frame_provider_base(frame_provider.release());

  if (is_audio) {
    AvPipelineClient client;
    client.eos_cb =
        base::Bind(&AudioVideoPipelineImplTest::OnEos, base::Unretained(this));

    base::Closure task = base::Bind(&MediaPipelineImpl::InitializeAudio,
                                    base::Unretained(media_pipeline_.get()),
                                    audio_config,
                                    client,
                                    base::Passed(&frame_provider_base),
                                    next_task);
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, task);
  } else {
    VideoPipelineClient client;
    client.av_pipeline_client.eos_cb =
        base::Bind(&AudioVideoPipelineImplTest::OnEos, base::Unretained(this));

    base::Closure task = base::Bind(&MediaPipelineImpl::InitializeVideo,
                                    base::Unretained(media_pipeline_.get()),
                                    video_configs,
                                    client,
                                    base::Passed(&frame_provider_base),
                                    next_task);
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, task);
  }
}

void AudioVideoPipelineImplTest::StartPlaying(
    const base::Closure& done_cb, ::media::PipelineStatus status) {
  base::TimeDelta start_time = base::TimeDelta::FromMilliseconds(0);

  media_pipeline_->StartPlayingFrom(start_time);
  if (!done_cb.is_null())
    done_cb.Run();
}

void AudioVideoPipelineImplTest::OnEos() {
  task_after_eos_cb_.Run();
}

void AudioVideoPipelineImplTest::Flush(
    const base::Closure& done_cb, ::media::PipelineStatus status) {
  ::media::PipelineStatusCB next_task =
      base::Bind(&AudioVideoPipelineImplTest::Stop, base::Unretained(this),
                 done_cb);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&MediaPipelineImpl::Flush,
                 base::Unretained(media_pipeline_.get()),
                 next_task));
}

void AudioVideoPipelineImplTest::Stop(
    const base::Closure& done_cb, ::media::PipelineStatus status) {
  media_pipeline_->Stop();
  if (!done_cb.is_null())
    done_cb.Run();
  base::MessageLoop::current()->QuitWhenIdle();
}


TEST_F(AudioVideoPipelineImplTest, AudioFullCycleInitToStop) {
  bool is_audio = true;
  task_after_eos_cb_ = base::Bind(
      &AudioVideoPipelineImplTest::Flush, base::Unretained(this),
      base::Closure(), ::media::PIPELINE_OK);

  message_loop_->PostTask(FROM_HERE,
                          base::Bind(&AudioVideoPipelineImplTest::Initialize,
                                     base::Unretained(this), base::Closure(),
                                     ::media::PIPELINE_OK, is_audio));
  message_loop_->Run();
};

TEST_F(AudioVideoPipelineImplTest, VideoFullCycleInitToStop) {
  bool is_audio = false;
  task_after_eos_cb_ = base::Bind(
      &AudioVideoPipelineImplTest::Flush, base::Unretained(this),
      base::Closure(), ::media::PIPELINE_OK);

  message_loop_->PostTask(FROM_HERE,
                          base::Bind(&AudioVideoPipelineImplTest::Initialize,
                                     base::Unretained(this), base::Closure(),
                                     ::media::PIPELINE_OK, is_audio));
  message_loop_->Run();
};

}  // namespace media
}  // namespace chromecast
