// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "chromecast/base/metrics/cast_metrics_test_helper.cc"
#include "chromecast/media/audio/cast_audio_manager.h"
#include "chromecast/media/audio/cast_audio_output_stream.h"
#include "chromecast/media/base/media_message_loop.h"
#include "chromecast/public/media/audio_pipeline_device.h"
#include "chromecast/public/media/cast_decoder_buffer.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/decrypt_context.h"
#include "chromecast/public/media/media_clock_device.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace {
const char kDefaultDeviceId[] = "";

void RunUntilIdle(base::TaskRunner* task_runner) {
  base::WaitableEvent completion_event(false, false);
  task_runner->PostTask(FROM_HERE,
                        base::Bind(&base::WaitableEvent::Signal,
                                   base::Unretained(&completion_event)));
  completion_event.Wait();
}

class FakeClockDevice : public MediaClockDevice {
 public:
  FakeClockDevice() : state_(kStateUninitialized), rate_(0.f) {}
  ~FakeClockDevice() override {}

  State GetState() const override { return state_; }
  bool SetState(State new_state) override {
    state_ = new_state;
    return true;
  }
  bool ResetTimeline(int64_t time_microseconds) override { return true; }
  bool SetRate(float rate) override {
    rate_ = rate;
    return true;
  }
  int64_t GetTimeMicroseconds() override { return 0; }

  float rate() const { return rate_; }

 private:
  State state_;
  float rate_;
};

class FakeAudioPipelineDevice : public AudioPipelineDevice {
 public:
  enum PipelineStatus {
    PIPELINE_STATUS_OK,
    PIPELINE_STATUS_BUSY,
    PIPELINE_STATUS_ERROR
  };

  FakeAudioPipelineDevice()
      : state_(kStateUninitialized),
        volume_multiplier_(1.0f),
        pipeline_status_(PIPELINE_STATUS_OK),
        pushed_frame_count_(0) {}
  ~FakeAudioPipelineDevice() override {}

  // AudioPipelineDevice overrides.
  void SetClient(Client* client) override {}
  bool SetState(State new_state) override {
    state_ = new_state;
    return true;
  }
  State GetState() const override { return state_; }
  bool SetStartPts(int64_t microseconds) override { return false; }
  FrameStatus PushFrame(DecryptContext* decrypt_context,
                        CastDecoderBuffer* buffer,
                        FrameStatusCB* completion_cb) override {
    last_frame_decrypt_context_.reset(decrypt_context);
    last_frame_buffer_.reset(buffer);
    last_frame_completion_cb_.reset(completion_cb);
    ++pushed_frame_count_;

    switch (pipeline_status_) {
      case PIPELINE_STATUS_OK:
        return kFrameSuccess;
      case PIPELINE_STATUS_BUSY:
        return kFramePending;
      case PIPELINE_STATUS_ERROR:
        return kFrameFailed;
      default:
        NOTREACHED();
    }

    // This will never be reached but is necessary for compiler warnings.
    return kFrameFailed;
  }
  RenderingDelay GetRenderingDelay() const override { return RenderingDelay(); }
  bool GetStatistics(Statistics* stats) const override { return false; }
  bool SetConfig(const AudioConfig& config) override {
    config_ = config;
    return true;
  }
  void SetStreamVolumeMultiplier(float multiplier) override {
    volume_multiplier_ = multiplier;
  }

  const AudioConfig& config() const { return config_; }
  float volume_multiplier() const { return volume_multiplier_; }
  void set_pipeline_status(PipelineStatus status) { pipeline_status_ = status; }
  unsigned pushed_frame_count() const { return pushed_frame_count_; }
  DecryptContext* last_frame_decrypt_context() {
    return last_frame_decrypt_context_.get();
  }
  CastDecoderBuffer* last_frame_buffer() { return last_frame_buffer_.get(); }
  FrameStatusCB* last_frame_completion_cb() {
    return last_frame_completion_cb_.get();
  }

 private:
  State state_;
  AudioConfig config_;
  float volume_multiplier_;

  PipelineStatus pipeline_status_;
  unsigned pushed_frame_count_;
  scoped_ptr<DecryptContext> last_frame_decrypt_context_;
  scoped_ptr<CastDecoderBuffer> last_frame_buffer_;
  scoped_ptr<FrameStatusCB> last_frame_completion_cb_;
};

class FakeMediaPipelineBackend : public MediaPipelineBackend {
 public:
  ~FakeMediaPipelineBackend() override {}

  MediaClockDevice* GetClock() override {
    if (!clock_device_)
      clock_device_.reset(new FakeClockDevice);
    return clock_device_.get();
  }
  AudioPipelineDevice* GetAudio() override {
    if (!audio_device_)
      audio_device_.reset(new FakeAudioPipelineDevice);
    return audio_device_.get();
  }
  VideoPipelineDevice* GetVideo() override {
    NOTREACHED();
    return nullptr;
  }

 private:
  scoped_ptr<FakeClockDevice> clock_device_;
  scoped_ptr<FakeAudioPipelineDevice> audio_device_;
};

class FakeAudioSourceCallback
    : public ::media::AudioOutputStream::AudioSourceCallback {
 public:
  FakeAudioSourceCallback() : error_(false) {}

  bool error() const { return error_; }

  // ::media::AudioOutputStream::AudioSourceCallback overrides.
  int OnMoreData(::media::AudioBus* audio_bus,
                 uint32 total_bytes_delay) override {
    audio_bus->Zero();
    return audio_bus->frames();
  }
  void OnError(::media::AudioOutputStream* stream) override { error_ = true; }

 private:
  bool error_;
};

class FakeAudioManager : public CastAudioManager {
 public:
  FakeAudioManager()
      : CastAudioManager(nullptr), media_pipeline_backend_(nullptr) {}
  ~FakeAudioManager() override {}

  // CastAudioManager overrides.
  scoped_ptr<MediaPipelineBackend> CreateMediaPipelineBackend() override {
    DCHECK(media::MediaMessageLoop::GetTaskRunner()->BelongsToCurrentThread());
    DCHECK(!media_pipeline_backend_);

    scoped_ptr<FakeMediaPipelineBackend> backend(new FakeMediaPipelineBackend);
    // Cache the backend locally to be used by tests.
    media_pipeline_backend_ = backend.get();
    return backend.Pass();
  }
  void ReleaseOutputStream(::media::AudioOutputStream* stream) override {
    DCHECK(media_pipeline_backend_);
    media_pipeline_backend_ = nullptr;
    CastAudioManager::ReleaseOutputStream(stream);
  }

  // Returns the MediaPipelineBackend being used by the AudioOutputStream.
  // Note: here is a valid MediaPipelineBackend only while the stream is open.
  // Returns NULL at all other times.
  FakeMediaPipelineBackend* media_pipeline_backend() {
    return media_pipeline_backend_;
  }

 private:
  FakeMediaPipelineBackend* media_pipeline_backend_;
};

class CastAudioOutputStreamTest : public ::testing::Test {
 public:
  CastAudioOutputStreamTest()
      : format_(::media::AudioParameters::AUDIO_PCM_LINEAR),
        channel_layout_(::media::CHANNEL_LAYOUT_MONO),
        sample_rate_(::media::AudioParameters::kAudioCDSampleRate),
        bits_per_sample_(16),
        frames_per_buffer_(256) {}
  ~CastAudioOutputStreamTest() override {}

 protected:
  void SetUp() override {
    metrics::InitializeMetricsHelperForTesting();

    audio_manager_.reset(new FakeAudioManager);
    audio_task_runner_ = audio_manager_->GetTaskRunner();
    backend_task_runner_ = media::MediaMessageLoop::GetTaskRunner();
  }

  void TearDown() override {
    audio_manager_.reset();
  }

  ::media::AudioParameters GetAudioParams() {
    return ::media::AudioParameters(format_, channel_layout_, sample_rate_,
                                    bits_per_sample_, frames_per_buffer_);
  }
  FakeClockDevice* GetClock() {
    MediaPipelineBackend* backend = audio_manager_->media_pipeline_backend();
    return backend ? static_cast<FakeClockDevice*>(backend->GetClock())
                   : nullptr;
  }
  FakeAudioPipelineDevice* GetAudio() {
    MediaPipelineBackend* backend = audio_manager_->media_pipeline_backend();
    return backend ? static_cast<FakeAudioPipelineDevice*>(backend->GetAudio())
                   : nullptr;
  }

  // Synchronous utility functions.
  ::media::AudioOutputStream* CreateStream() {
    ::media::AudioOutputStream* stream = nullptr;

    base::WaitableEvent completion_event(false, false);
    audio_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&CastAudioOutputStreamTest::CreateStreamOnAudioThread,
                   base::Unretained(this), GetAudioParams(), &stream,
                   &completion_event));
    completion_event.Wait();

    return stream;
  }
  bool OpenStream(::media::AudioOutputStream* stream) {
    DCHECK(stream);

    bool success = false;
    base::WaitableEvent completion_event(false, false);
    audio_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&CastAudioOutputStreamTest::OpenStreamOnAudioThread,
                   base::Unretained(this), stream, &success,
                   &completion_event));
    completion_event.Wait();

    // Drain the backend task runner so that appropriate states are set on
    // the backend pipeline devices.
    RunUntilIdle(backend_task_runner_.get());
    return success;
  }
  void CloseStream(::media::AudioOutputStream* stream) {
    audio_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(&::media::AudioOutputStream::Close,
                                            base::Unretained(stream)));
    RunUntilIdle(audio_task_runner_.get());
    RunUntilIdle(backend_task_runner_.get());
    // Backend task runner may have posted more tasks to the audio task runner.
    // So we need to drain it once more.
    RunUntilIdle(audio_task_runner_.get());
  }
  void StartStream(
      ::media::AudioOutputStream* stream,
      ::media::AudioOutputStream::AudioSourceCallback* source_callback) {
    audio_task_runner_->PostTask(
        FROM_HERE, base::Bind(&::media::AudioOutputStream::Start,
                              base::Unretained(stream), source_callback));
    // Drain the audio task runner twice so that tasks posted by
    // media::AudioOutputStream::Start are run as well.
    RunUntilIdle(audio_task_runner_.get());
    RunUntilIdle(audio_task_runner_.get());
    // Drain the backend task runner so that appropriate states are set on
    // the backend pipeline devices.
    RunUntilIdle(backend_task_runner_.get());
    // Drain the audio task runner again to run the tasks posted by the
    // backend on audio task runner.
    RunUntilIdle(audio_task_runner_.get());
  }
  void StopStream(::media::AudioOutputStream* stream) {
    audio_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(&::media::AudioOutputStream::Stop,
                                            base::Unretained(stream)));
    RunUntilIdle(audio_task_runner_.get());
    // Drain the backend task runner so that appropriate states are set on
    // the backend pipeline devices.
    RunUntilIdle(backend_task_runner_.get());
  }
  void SetStreamVolume(::media::AudioOutputStream* stream, double volume) {
    audio_task_runner_->PostTask(
        FROM_HERE, base::Bind(&::media::AudioOutputStream::SetVolume,
                              base::Unretained(stream), volume));
    RunUntilIdle(audio_task_runner_.get());
    // Drain the backend task runner so that appropriate states are set on
    // the backend pipeline devices.
    RunUntilIdle(backend_task_runner_.get());
  }
  double GetStreamVolume(::media::AudioOutputStream* stream) {
    double volume = 0.0;
    audio_task_runner_->PostTask(
        FROM_HERE, base::Bind(&::media::AudioOutputStream::GetVolume,
                              base::Unretained(stream), &volume));
    RunUntilIdle(audio_task_runner_.get());
    // No need to drain the backend task runner because getting the volume
    // does not involve posting any task to the backend.
    return volume;
  }

  void CreateStreamOnAudioThread(const ::media::AudioParameters& audio_params,
                                 ::media::AudioOutputStream** stream,
                                 base::WaitableEvent* completion_event) {
    DCHECK(audio_task_runner_->BelongsToCurrentThread());
    *stream = audio_manager_->MakeAudioOutputStream(GetAudioParams(),
                                                    kDefaultDeviceId);
    completion_event->Signal();
  }
  void OpenStreamOnAudioThread(::media::AudioOutputStream* stream,
                               bool* success,
                               base::WaitableEvent* completion_event) {
    DCHECK(audio_task_runner_->BelongsToCurrentThread());
    *success = stream->Open();
    completion_event->Signal();
  }

  scoped_ptr<FakeAudioManager> audio_manager_;
  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> backend_task_runner_;

  // AudioParameters used to create AudioOutputStream.
  // Tests can modify these parameters before calling CreateStream.
  ::media::AudioParameters::Format format_;
  ::media::ChannelLayout channel_layout_;
  int sample_rate_;
  int bits_per_sample_;
  int frames_per_buffer_;
};

TEST_F(CastAudioOutputStreamTest, Format) {
  ::media::AudioParameters::Format format[] = {
      //::media::AudioParameters::AUDIO_PCM_LINEAR,
      ::media::AudioParameters::AUDIO_PCM_LOW_LATENCY};
  for (size_t i = 0; i < arraysize(format); ++i) {
    format_ = format[i];
    ::media::AudioOutputStream* stream = CreateStream();
    ASSERT_TRUE(stream);
    EXPECT_TRUE(OpenStream(stream));

    FakeAudioPipelineDevice* audio_device = GetAudio();
    ASSERT_TRUE(audio_device);
    const AudioConfig& audio_config = audio_device->config();
    EXPECT_EQ(kCodecPCM, audio_config.codec);
    EXPECT_EQ(kSampleFormatS16, audio_config.sample_format);
    EXPECT_FALSE(audio_config.is_encrypted);

    CloseStream(stream);
  }
}

TEST_F(CastAudioOutputStreamTest, ChannelLayout) {
  ::media::ChannelLayout layout[] = {::media::CHANNEL_LAYOUT_MONO,
                                     ::media::CHANNEL_LAYOUT_STEREO};
  for (size_t i = 0; i < arraysize(layout); ++i) {
    channel_layout_ = layout[i];
    ::media::AudioOutputStream* stream = CreateStream();
    ASSERT_TRUE(stream);
    EXPECT_TRUE(OpenStream(stream));

    FakeAudioPipelineDevice* audio_device = GetAudio();
    ASSERT_TRUE(audio_device);
    const AudioConfig& audio_config = audio_device->config();
    EXPECT_EQ(::media::ChannelLayoutToChannelCount(channel_layout_),
              audio_config.channel_number);

    CloseStream(stream);
  }
}

TEST_F(CastAudioOutputStreamTest, SampleRate) {
  sample_rate_ = ::media::AudioParameters::kAudioCDSampleRate;
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  EXPECT_TRUE(OpenStream(stream));

  FakeAudioPipelineDevice* audio_device = GetAudio();
  ASSERT_TRUE(audio_device);
  const AudioConfig& audio_config = audio_device->config();
  EXPECT_EQ(sample_rate_, audio_config.samples_per_second);

  CloseStream(stream);
}

TEST_F(CastAudioOutputStreamTest, BitsPerSample) {
  bits_per_sample_ = 16;
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  EXPECT_TRUE(OpenStream(stream));

  FakeAudioPipelineDevice* audio_device = GetAudio();
  ASSERT_TRUE(audio_device);
  const AudioConfig& audio_config = audio_device->config();
  EXPECT_EQ(bits_per_sample_ / 8, audio_config.bytes_per_channel);

  CloseStream(stream);
}

TEST_F(CastAudioOutputStreamTest, DeviceState) {
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  EXPECT_FALSE(GetAudio());

  EXPECT_TRUE(OpenStream(stream));
  AudioPipelineDevice* audio_device = GetAudio();
  ASSERT_TRUE(audio_device);
  FakeClockDevice* clock_device = GetClock();
  ASSERT_TRUE(clock_device);
  EXPECT_EQ(AudioPipelineDevice::kStateIdle, audio_device->GetState());
  EXPECT_EQ(MediaClockDevice::kStateIdle, clock_device->GetState());
  EXPECT_EQ(1.f, clock_device->rate());

  scoped_ptr<FakeAudioSourceCallback> source_callback(
      new FakeAudioSourceCallback);
  StartStream(stream, source_callback.get());
  EXPECT_EQ(AudioPipelineDevice::kStateRunning, audio_device->GetState());
  EXPECT_EQ(MediaClockDevice::kStateRunning, clock_device->GetState());
  EXPECT_EQ(1.f, clock_device->rate());

  StopStream(stream);
  EXPECT_EQ(AudioPipelineDevice::kStateRunning, audio_device->GetState());
  EXPECT_EQ(MediaClockDevice::kStateRunning, clock_device->GetState());
  EXPECT_EQ(0.f, clock_device->rate());

  CloseStream(stream);
  EXPECT_FALSE(GetAudio());
}

TEST_F(CastAudioOutputStreamTest, PushFrame) {
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  EXPECT_TRUE(OpenStream(stream));

  FakeAudioPipelineDevice* audio_device = GetAudio();
  ASSERT_TRUE(audio_device);
  // Verify initial state.
  EXPECT_EQ(0u, audio_device->pushed_frame_count());
  EXPECT_FALSE(audio_device->last_frame_decrypt_context());
  EXPECT_FALSE(audio_device->last_frame_buffer());
  EXPECT_FALSE(audio_device->last_frame_completion_cb());

  scoped_ptr<FakeAudioSourceCallback> source_callback(
      new FakeAudioSourceCallback);
  StartStream(stream, source_callback.get());
  StopStream(stream);

  // Verify that the stream pushed frames to the backend.
  EXPECT_LT(0u, audio_device->pushed_frame_count());
  // DecryptContext is always NULL becuase of "raw" audio.
  EXPECT_FALSE(audio_device->last_frame_decrypt_context());
  EXPECT_TRUE(audio_device->last_frame_buffer());
  EXPECT_TRUE(audio_device->last_frame_completion_cb());

  // Verify decoder buffer.
  ::media::AudioParameters audio_params = GetAudioParams();
  const size_t expected_frame_size =
      static_cast<size_t>(audio_params.GetBytesPerBuffer());
  const CastDecoderBuffer* buffer = audio_device->last_frame_buffer();
  EXPECT_TRUE(buffer->data());
  EXPECT_EQ(expected_frame_size, buffer->data_size());
  EXPECT_FALSE(buffer->decrypt_config());  // Null because of raw audio.
  EXPECT_FALSE(buffer->end_of_stream());

  // No error must be reported to source callback.
  EXPECT_FALSE(source_callback->error());

  CloseStream(stream);
}

TEST_F(CastAudioOutputStreamTest, DeviceBusy) {
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  EXPECT_TRUE(OpenStream(stream));

  FakeAudioPipelineDevice* audio_device = GetAudio();
  ASSERT_TRUE(audio_device);
  audio_device->set_pipeline_status(
      FakeAudioPipelineDevice::PIPELINE_STATUS_BUSY);

  scoped_ptr<FakeAudioSourceCallback> source_callback(
      new FakeAudioSourceCallback);
  StartStream(stream, source_callback.get());

  // Make sure that one frame was pushed.
  EXPECT_EQ(1u, audio_device->pushed_frame_count());
  // No error must be reported to source callback.
  EXPECT_FALSE(source_callback->error());

  // Sleep for a few frames and verify that more frames were not pushed
  // because the backend device was busy.
  ::media::AudioParameters audio_params = GetAudioParams();
  base::TimeDelta pause = audio_params.GetBufferDuration() * 5;
  base::PlatformThread::Sleep(pause);
  RunUntilIdle(audio_task_runner_.get());
  RunUntilIdle(backend_task_runner_.get());
  EXPECT_EQ(1u, audio_device->pushed_frame_count());

  // Unblock the pipeline and verify that PushFrame resumes.
  audio_device->set_pipeline_status(
      FakeAudioPipelineDevice::PIPELINE_STATUS_OK);
  audio_device->last_frame_completion_cb()->Run(
      MediaComponentDevice::kFrameSuccess);
  base::PlatformThread::Sleep(pause);
  RunUntilIdle(audio_task_runner_.get());
  RunUntilIdle(backend_task_runner_.get());
  EXPECT_LT(1u, audio_device->pushed_frame_count());
  EXPECT_FALSE(source_callback->error());

  StopStream(stream);
  CloseStream(stream);
}

TEST_F(CastAudioOutputStreamTest, DeviceError) {
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  EXPECT_TRUE(OpenStream(stream));

  FakeAudioPipelineDevice* audio_device = GetAudio();
  ASSERT_TRUE(audio_device);
  audio_device->set_pipeline_status(
      FakeAudioPipelineDevice::PIPELINE_STATUS_ERROR);

  scoped_ptr<FakeAudioSourceCallback> source_callback(
      new FakeAudioSourceCallback);
  StartStream(stream, source_callback.get());

  // Make sure that AudioOutputStream attempted to push the initial frame.
  EXPECT_LT(0u, audio_device->pushed_frame_count());
  // AudioOutputStream must report error to source callback.
  EXPECT_TRUE(source_callback->error());

  StopStream(stream);
  CloseStream(stream);
}

TEST_F(CastAudioOutputStreamTest, Volume) {
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  ASSERT_TRUE(OpenStream(stream));
  FakeAudioPipelineDevice* audio_device = GetAudio();
  ASSERT_TRUE(audio_device);

  double volume = GetStreamVolume(stream);
  EXPECT_EQ(1.0, volume);
  EXPECT_EQ(1.0f, audio_device->volume_multiplier());

  SetStreamVolume(stream, 0.5);
  volume = GetStreamVolume(stream);
  EXPECT_EQ(0.5, volume);
  EXPECT_EQ(0.5f, audio_device->volume_multiplier());

  CloseStream(stream);
}

}  // namespace
}  // namespace media
}  // namespace chromecast
