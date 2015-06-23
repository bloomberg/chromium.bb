// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/logging.h"
#include "base/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "media/base/android/media_codec_audio_decoder.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/media_codec_video_decoder.h"
#include "media/base/android/test_data_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/android/surface_texture.h"

namespace media {

// Helper macro to skip the test if MediaCodecBridge isn't available.
#define SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE()        \
  do {                                                            \
    if (!MediaCodecBridge::IsAvailable()) {                       \
      VLOG(0) << "Could not run test - not supported on device."; \
      return;                                                     \
    }                                                             \
  } while (0)

namespace {

const base::TimeDelta kDefaultTimeout = base::TimeDelta::FromMilliseconds(200);
const base::TimeDelta kAudioFramePeriod = base::TimeDelta::FromMilliseconds(20);
const base::TimeDelta kVideoFramePeriod = base::TimeDelta::FromMilliseconds(20);

class AudioFactory : public TestDataFactory {
 public:
  AudioFactory(const base::TimeDelta& duration);
  DemuxerConfigs GetConfigs() override;

 protected:
  void ModifyAccessUnit(int index_in_chunk, AccessUnit* unit) override;
};

class VideoFactory : public TestDataFactory {
 public:
  VideoFactory(const base::TimeDelta& duration);
  DemuxerConfigs GetConfigs() override;

 protected:
  void ModifyAccessUnit(int index_in_chunk, AccessUnit* unit) override;
};

AudioFactory::AudioFactory(const base::TimeDelta& duration)
    : TestDataFactory("vorbis-packet-%d", duration, kAudioFramePeriod) {
}

DemuxerConfigs AudioFactory::GetConfigs() {
  return TestDataFactory::CreateAudioConfigs(kCodecVorbis, duration_);
}

void AudioFactory::ModifyAccessUnit(int index_in_chunk, AccessUnit* unit) {
  // Vorbis needs 4 extra bytes padding on Android to decode properly. Check
  // NuMediaExtractor.cpp in Android source code.
  uint8 padding[4] = {0xff, 0xff, 0xff, 0xff};
  unit->data.insert(unit->data.end(), padding, padding + 4);
}

VideoFactory::VideoFactory(const base::TimeDelta& duration)
    : TestDataFactory("h264-320x180-frame-%d", duration, kVideoFramePeriod) {
}

DemuxerConfigs VideoFactory::GetConfigs() {
  return TestDataFactory::CreateVideoConfigs(kCodecH264, duration_,
                                             gfx::Size(320, 180));
}

void VideoFactory::ModifyAccessUnit(int index_in_chunk, AccessUnit* unit) {
  // The frames are taken from High profile and some are B-frames.
  // The first 4 frames appear in the file in the following order:
  //
  // Frames:             I P B P
  // Decoding order:     0 1 2 3
  // Presentation order: 0 2 1 4(3)
  //
  // I keep the last PTS to be 3 for simplicity.

  // Swap pts for second and third frames.
  if (index_in_chunk == 1)  // second frame
    unit->timestamp += frame_period_;
  if (index_in_chunk == 2)  // third frame
    unit->timestamp -= frame_period_;

  if (index_in_chunk == 0)
    unit->is_key_frame = true;
}

// Class that computes statistics: number of calls, minimum and maximum values.
// It is used for PTS statistics to verify that playback did actually happen.

template <typename T>
class Minimax {
 public:
  Minimax() : num_values_(0) {}
  ~Minimax() {}

  void AddValue(const T& value) {
    ++num_values_;
    if (value < min_)
      min_ = value;
    else if (max_ < value)
      max_ = value;
  }

  const T& min() const { return min_; }
  const T& max() const { return max_; }
  int num_values() const { return num_values_; }

 private:
  T min_;
  T max_;
  int num_values_;
};

}  // namespace (anonymous)

// The test fixture for MediaCodecDecoder

class MediaCodecDecoderTest : public testing::Test {
 public:
  MediaCodecDecoderTest();
  ~MediaCodecDecoderTest() override;

  // Conditions we wait for.
  bool is_prefetched() const { return is_prefetched_; }
  bool is_stopped() const { return is_stopped_; }
  bool is_starved() const { return is_starved_; }

  // Prefetch callback has to be public.
  void SetPrefetched() { is_prefetched_ = true; }

 protected:
  typedef base::Callback<bool()> Predicate;

  typedef base::Callback<void(const DemuxerData&)> DataAvailableCallback;

  // Waits for condition to become true or for timeout to expire.
  // Returns true if the condition becomes true.
  bool WaitForCondition(const Predicate& condition,
                        const base::TimeDelta& timeout = kDefaultTimeout);

  void SetDataFactory(scoped_ptr<TestDataFactory> factory) {
    data_factory_ = factory.Pass();
  }

  DemuxerConfigs GetConfigs() {
    // ASSERT_NE does not compile here because it expects void return value.
    EXPECT_NE(nullptr, data_factory_.get());
    return data_factory_->GetConfigs();
  }

  void CreateAudioDecoder();
  void CreateVideoDecoder();
  void SetVideoSurface();

  // Decoder callbacks.
  void OnDataRequested();
  void OnStarvation() { is_starved_ = true; }
  void OnStopDone() { is_stopped_ = true; }
  void OnError() {}
  void OnUpdateCurrentTime(base::TimeDelta now_playing,
                           base::TimeDelta last_buffered) {
    pts_stat_.AddValue(now_playing);
  }
  void OnVideoSizeChanged(const gfx::Size& video_size) {}
  void OnVideoCodecCreated() {}

  scoped_ptr<MediaCodecDecoder> decoder_;
  scoped_ptr<TestDataFactory> data_factory_;
  Minimax<base::TimeDelta> pts_stat_;

 private:
  bool is_timeout_expired() const { return is_timeout_expired_; }
  void SetTimeoutExpired(bool value) { is_timeout_expired_ = value; }

  base::MessageLoop message_loop_;
  bool is_timeout_expired_;

  bool is_prefetched_;
  bool is_stopped_;
  bool is_starved_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  DataAvailableCallback data_available_cb_;
  scoped_refptr<gfx::SurfaceTexture> surface_texture_;

  DISALLOW_COPY_AND_ASSIGN(MediaCodecDecoderTest);
};

MediaCodecDecoderTest::MediaCodecDecoderTest()
    : is_timeout_expired_(false),
      is_prefetched_(false),
      is_stopped_(false),
      is_starved_(false),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {
}

MediaCodecDecoderTest::~MediaCodecDecoderTest() {}

bool MediaCodecDecoderTest::WaitForCondition(const Predicate& condition,
                                             const base::TimeDelta& timeout) {
  // Let the message_loop_ process events.
  // We start the timer and RunUntilIdle() until it signals.

  SetTimeoutExpired(false);

  base::Timer timer(false, false);
  timer.Start(FROM_HERE, timeout,
              base::Bind(&MediaCodecDecoderTest::SetTimeoutExpired,
                         base::Unretained(this), true));

  do {
    if (condition.Run()) {
      timer.Stop();
      return true;
    }
    message_loop_.RunUntilIdle();
  } while (!is_timeout_expired());

  DCHECK(!timer.IsRunning());
  return false;
}

void MediaCodecDecoderTest::CreateAudioDecoder() {
  decoder_ = scoped_ptr<MediaCodecDecoder>(new MediaCodecAudioDecoder(
      task_runner_, base::Bind(&MediaCodecDecoderTest::OnDataRequested,
                               base::Unretained(this)),
      base::Bind(&MediaCodecDecoderTest::OnStarvation, base::Unretained(this)),
      base::Bind(&MediaCodecDecoderTest::OnStopDone, base::Unretained(this)),
      base::Bind(&MediaCodecDecoderTest::OnError, base::Unretained(this)),
      base::Bind(&MediaCodecDecoderTest::OnUpdateCurrentTime,
                 base::Unretained(this))));

  data_available_cb_ = base::Bind(&MediaCodecDecoder::OnDemuxerDataAvailable,
                                  base::Unretained(decoder_.get()));
}

void MediaCodecDecoderTest::CreateVideoDecoder() {
  decoder_ = scoped_ptr<MediaCodecDecoder>(new MediaCodecVideoDecoder(
      task_runner_, base::Bind(&MediaCodecDecoderTest::OnDataRequested,
                               base::Unretained(this)),
      base::Bind(&MediaCodecDecoderTest::OnStarvation, base::Unretained(this)),
      base::Bind(&MediaCodecDecoderTest::OnStopDone, base::Unretained(this)),
      base::Bind(&MediaCodecDecoderTest::OnError, base::Unretained(this)),
      base::Bind(&MediaCodecDecoderTest::OnUpdateCurrentTime,
                 base::Unretained(this)),
      base::Bind(&MediaCodecDecoderTest::OnVideoSizeChanged,
                 base::Unretained(this)),
      base::Bind(&MediaCodecDecoderTest::OnVideoCodecCreated,
                 base::Unretained(this))));

  data_available_cb_ = base::Bind(&MediaCodecDecoder::OnDemuxerDataAvailable,
                                  base::Unretained(decoder_.get()));
}

void MediaCodecDecoderTest::OnDataRequested() {
  if (!data_factory_)
    return;

  DemuxerData data;
  base::TimeDelta delay;
  data_factory_->CreateChunk(&data, &delay);

  task_runner_->PostDelayedTask(FROM_HERE, base::Bind(data_available_cb_, data),
                                delay);
}

void MediaCodecDecoderTest::SetVideoSurface() {
  surface_texture_ = gfx::SurfaceTexture::Create(0);
  gfx::ScopedJavaSurface surface(surface_texture_.get());
  ASSERT_NE(nullptr, decoder_.get());
  MediaCodecVideoDecoder* video_decoder =
      static_cast<MediaCodecVideoDecoder*>(decoder_.get());
  video_decoder->SetPendingSurface(surface.Pass());
}

TEST_F(MediaCodecDecoderTest, AudioPrefetch) {
  CreateAudioDecoder();

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(500);
  SetDataFactory(scoped_ptr<TestDataFactory>(new AudioFactory(duration)));

  decoder_->Prefetch(base::Bind(&MediaCodecDecoderTest::SetPrefetched,
                                base::Unretained(this)));

  EXPECT_TRUE(WaitForCondition(base::Bind(&MediaCodecDecoderTest::is_prefetched,
                                          base::Unretained(this))));
}

TEST_F(MediaCodecDecoderTest, VideoPrefetch) {
  CreateVideoDecoder();

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(500);
  SetDataFactory(scoped_ptr<VideoFactory>(new VideoFactory(duration)));

  decoder_->Prefetch(base::Bind(&MediaCodecDecoderTest::SetPrefetched,
                                base::Unretained(this)));

  EXPECT_TRUE(WaitForCondition(base::Bind(&MediaCodecDecoderTest::is_prefetched,
                                          base::Unretained(this))));
}

TEST_F(MediaCodecDecoderTest, AudioConfigureNoParams) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  CreateAudioDecoder();

  // Cannot configure without config parameters.
  EXPECT_EQ(MediaCodecDecoder::CONFIG_FAILURE, decoder_->Configure());
}

TEST_F(MediaCodecDecoderTest, AudioConfigureValidParams) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  CreateAudioDecoder();

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(500);
  scoped_ptr<AudioFactory> factory(new AudioFactory(duration));
  decoder_->SetDemuxerConfigs(factory->GetConfigs());

  EXPECT_EQ(MediaCodecDecoder::CONFIG_OK, decoder_->Configure());
}

TEST_F(MediaCodecDecoderTest, VideoConfigureNoParams) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  CreateVideoDecoder();

  // Cannot configure without config parameters.
  EXPECT_EQ(MediaCodecDecoder::CONFIG_FAILURE, decoder_->Configure());
}

TEST_F(MediaCodecDecoderTest, VideoConfigureNoSurface) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  CreateVideoDecoder();

  // decoder_->Configure() searches back for the key frame.
  // We have to prefetch decoder.

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(500);
  SetDataFactory(scoped_ptr<VideoFactory>(new VideoFactory(duration)));

  decoder_->Prefetch(base::Bind(&MediaCodecDecoderTest::SetPrefetched,
                                base::Unretained(this)));

  EXPECT_TRUE(WaitForCondition(base::Bind(&MediaCodecDecoderTest::is_prefetched,
                                          base::Unretained(this))));

  decoder_->SetDemuxerConfigs(GetConfigs());

  // Surface is not set, Configure() should fail.

  EXPECT_EQ(MediaCodecDecoder::CONFIG_FAILURE, decoder_->Configure());
}

TEST_F(MediaCodecDecoderTest, VideoConfigureInvalidSurface) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  CreateVideoDecoder();

  // decoder_->Configure() searches back for the key frame.
  // We have to prefetch decoder.

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(500);
  SetDataFactory(scoped_ptr<VideoFactory>(new VideoFactory(duration)));

  decoder_->Prefetch(base::Bind(&MediaCodecDecoderTest::SetPrefetched,
                                base::Unretained(this)));

  EXPECT_TRUE(WaitForCondition(base::Bind(&MediaCodecDecoderTest::is_prefetched,
                                          base::Unretained(this))));

  decoder_->SetDemuxerConfigs(GetConfigs());

  // Prepare the surface.
  scoped_refptr<gfx::SurfaceTexture> surface_texture(
      gfx::SurfaceTexture::Create(0));
  gfx::ScopedJavaSurface surface(surface_texture.get());

  // Release the surface texture.
  surface_texture = NULL;

  MediaCodecVideoDecoder* video_decoder =
      static_cast<MediaCodecVideoDecoder*>(decoder_.get());
  video_decoder->SetPendingSurface(surface.Pass());

  EXPECT_EQ(MediaCodecDecoder::CONFIG_FAILURE, decoder_->Configure());
}

TEST_F(MediaCodecDecoderTest, VideoConfigureValidParams) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  CreateVideoDecoder();

  // decoder_->Configure() searches back for the key frame.
  // We have to prefetch decoder.

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(500);
  SetDataFactory(scoped_ptr<VideoFactory>(new VideoFactory(duration)));

  decoder_->Prefetch(base::Bind(&MediaCodecDecoderTest::SetPrefetched,
                                base::Unretained(this)));

  EXPECT_TRUE(WaitForCondition(base::Bind(&MediaCodecDecoderTest::is_prefetched,
                                          base::Unretained(this))));

  decoder_->SetDemuxerConfigs(GetConfigs());

  SetVideoSurface();

  // Now we can expect Configure() to succeed.

  EXPECT_EQ(MediaCodecDecoder::CONFIG_OK, decoder_->Configure());
}

TEST_F(MediaCodecDecoderTest, AudioStartWithoutConfigure) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  CreateAudioDecoder();

  // Decoder has to be prefetched and configured before the start.

  // Wrong state: not prefetched
  EXPECT_FALSE(decoder_->Start(base::TimeDelta::FromMilliseconds(0)));

  // Do the prefetch.
  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(500);
  SetDataFactory(scoped_ptr<AudioFactory>(new AudioFactory(duration)));

  // Prefetch to avoid starvation at the beginning of playback.
  decoder_->Prefetch(base::Bind(&MediaCodecDecoderTest::SetPrefetched,
                                base::Unretained(this)));

  EXPECT_TRUE(WaitForCondition(base::Bind(&MediaCodecDecoderTest::is_prefetched,
                                          base::Unretained(this))));

  // Still, decoder is not configured.
  EXPECT_FALSE(decoder_->Start(base::TimeDelta::FromMilliseconds(0)));
}

TEST_F(MediaCodecDecoderTest, AudioPlayTillCompletion) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  CreateAudioDecoder();

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(500);
  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(600);

  SetDataFactory(scoped_ptr<AudioFactory>(new AudioFactory(duration)));

  // Prefetch to avoid starvation at the beginning of playback.
  decoder_->Prefetch(base::Bind(&MediaCodecDecoderTest::SetPrefetched,
                                base::Unretained(this)));

  EXPECT_TRUE(WaitForCondition(base::Bind(&MediaCodecDecoderTest::is_prefetched,
                                          base::Unretained(this))));

  decoder_->SetDemuxerConfigs(GetConfigs());

  EXPECT_EQ(MediaCodecDecoder::CONFIG_OK, decoder_->Configure());

  EXPECT_TRUE(decoder_->Start(base::TimeDelta::FromMilliseconds(0)));

  EXPECT_TRUE(WaitForCondition(
      base::Bind(&MediaCodecDecoderTest::is_stopped, base::Unretained(this)),
      timeout));

  EXPECT_TRUE(decoder_->IsStopped());
  EXPECT_TRUE(decoder_->IsCompleted());

  // It is hard to properly estimate minimum and maximum values because
  // reported times are different from PTS.
  EXPECT_EQ(25, pts_stat_.num_values());
}

TEST_F(MediaCodecDecoderTest, VideoPlayTillCompletion) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  CreateVideoDecoder();

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(500);
  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(600);
  SetDataFactory(scoped_ptr<VideoFactory>(new VideoFactory(duration)));

  // Prefetch
  decoder_->Prefetch(base::Bind(&MediaCodecDecoderTest::SetPrefetched,
                                base::Unretained(this)));

  EXPECT_TRUE(WaitForCondition(base::Bind(&MediaCodecDecoderTest::is_prefetched,
                                          base::Unretained(this))));

  decoder_->SetDemuxerConfigs(GetConfigs());

  SetVideoSurface();

  EXPECT_EQ(MediaCodecDecoder::CONFIG_OK, decoder_->Configure());

  EXPECT_TRUE(decoder_->Start(base::TimeDelta::FromMilliseconds(0)));

  EXPECT_TRUE(WaitForCondition(
      base::Bind(&MediaCodecDecoderTest::is_stopped, base::Unretained(this)),
      timeout));

  EXPECT_TRUE(decoder_->IsStopped());
  EXPECT_TRUE(decoder_->IsCompleted());

  EXPECT_EQ(26, pts_stat_.num_values());
  EXPECT_EQ(data_factory_->last_pts(), pts_stat_.max());
}

}  // namespace media
