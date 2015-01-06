// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/time/time.h"
#include "chromecast/media/cma/backend/audio_pipeline_device.h"
#include "chromecast/media/cma/backend/media_clock_device.h"
#include "chromecast/media/cma/backend/media_pipeline_device.h"
#include "chromecast/media/cma/backend/media_pipeline_device_fake.h"
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
#include "media/base/buffers.h"
#include "media/base/decoder_buffer.h"
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

 private:
  scoped_ptr<MediaPipelineImpl> media_pipeline_;

  DISALLOW_COPY_AND_ASSIGN(AudioVideoPipelineImplTest);
};

AudioVideoPipelineImplTest::AudioVideoPipelineImplTest()
  : media_pipeline_(new MediaPipelineImpl()) {
  scoped_ptr<MediaPipelineDevice> media_pipeline_device(
      new MediaPipelineDeviceFake());
  media_pipeline_->Initialize(kLoadTypeURL, media_pipeline_device.Pass());
  media_pipeline_->SetPlaybackRate(1.0);
}

AudioVideoPipelineImplTest::~AudioVideoPipelineImplTest() {
}

void AudioVideoPipelineImplTest::Initialize(
    const base::Closure& done_cb,
    ::media::PipelineStatus status,
    bool is_audio) {
  if (is_audio) {
    AvPipelineClient client;
    client.eos_cb =
        base::Bind(&AudioVideoPipelineImplTest::OnEos, base::Unretained(this));
    media_pipeline_->GetAudioPipeline()->SetClient(client);
  } else {
    VideoPipelineClient client;
    client.av_pipeline_client.eos_cb =
        base::Bind(&AudioVideoPipelineImplTest::OnEos, base::Unretained(this));
    media_pipeline_->GetVideoPipeline()->SetClient(client);
  }

  ::media::AudioDecoderConfig audio_config(
      ::media::kCodecMP3,
      ::media::kSampleFormatS16,
      ::media::CHANNEL_LAYOUT_STEREO,
      44100,
      NULL, 0, false);
  ::media::VideoDecoderConfig video_config(
      ::media::kCodecH264,
      ::media::H264PROFILE_MAIN,
      ::media::VideoFrame::I420,
      gfx::Size(640, 480),
      gfx::Rect(0, 0, 640, 480),
      gfx::Size(640, 480),
      NULL, 0, false);

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
      frame_generator_provider.Pass());

  ::media::PipelineStatusCB next_task =
      base::Bind(&AudioVideoPipelineImplTest::StartPlaying,
                 base::Unretained(this),
                 done_cb);

  scoped_ptr<CodedFrameProvider> frame_provider_base(frame_provider.release());
  base::Closure task = is_audio ?
      base::Bind(&MediaPipeline::InitializeAudio,
                 base::Unretained(media_pipeline_.get()),
                 audio_config,
                 base::Passed(&frame_provider_base),
                 next_task) :
      base::Bind(&MediaPipeline::InitializeVideo,
                 base::Unretained(media_pipeline_.get()),
                 video_config,
                 base::Passed(&frame_provider_base),
                 next_task);

  base::MessageLoopProxy::current()->PostTask(FROM_HERE, task);
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
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&MediaPipeline::Flush,
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

  scoped_ptr<base::MessageLoop> message_loop(new base::MessageLoop());
  message_loop->PostTask(
      FROM_HERE,
      base::Bind(&AudioVideoPipelineImplTest::Initialize,
                 base::Unretained(this),
                 base::Closure(),
                 ::media::PIPELINE_OK, is_audio));
  message_loop->Run();
};

TEST_F(AudioVideoPipelineImplTest, VideoFullCycleInitToStop) {
  bool is_audio = false;
  task_after_eos_cb_ = base::Bind(
      &AudioVideoPipelineImplTest::Flush, base::Unretained(this),
      base::Closure(), ::media::PIPELINE_OK);

  scoped_ptr<base::MessageLoop> message_loop(new base::MessageLoop());
  message_loop->PostTask(
      FROM_HERE,
      base::Bind(&AudioVideoPipelineImplTest::Initialize,
                 base::Unretained(this),
                 base::Closure(),
                 ::media::PIPELINE_OK, is_audio));
  message_loop->Run();
};

}  // namespace media
}  // namespace chromecast
