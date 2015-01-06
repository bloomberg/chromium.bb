// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_pipeline_device_fake.h"

#include <list>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop_proxy.h"
#include "chromecast/media/cma/backend/audio_pipeline_device.h"
#include "chromecast/media/cma/backend/media_clock_device.h"
#include "chromecast/media/cma/backend/media_component_device.h"
#include "chromecast/media/cma/backend/video_pipeline_device.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/buffers.h"
#include "media/base/video_decoder_config.h"

namespace chromecast {
namespace media {

class MediaClockDeviceFake : public MediaClockDevice {
 public:
  MediaClockDeviceFake();
  ~MediaClockDeviceFake() override;

  // MediaClockDevice implementation.
  State GetState() const override;
  bool SetState(State new_state) override;
  bool ResetTimeline(base::TimeDelta time) override;
  bool SetRate(float rate) override;
  base::TimeDelta GetTime() override;

 private:
  State state_;

  // Media time sampled at STC time |stc_|.
  base::TimeDelta media_time_;
  base::TimeTicks stc_;

  float rate_;

  DISALLOW_COPY_AND_ASSIGN(MediaClockDeviceFake);
};

MediaClockDeviceFake::MediaClockDeviceFake()
    : state_(kStateUninitialized),
      media_time_(::media::kNoTimestamp()) {
  DetachFromThread();
}

MediaClockDeviceFake::~MediaClockDeviceFake() {
}

MediaClockDevice::State MediaClockDeviceFake::GetState() const {
  DCHECK(CalledOnValidThread());
  return state_;
}

bool MediaClockDeviceFake::SetState(State new_state) {
  DCHECK(CalledOnValidThread());
  if (!MediaClockDevice::IsValidStateTransition(state_, new_state))
    return false;

  if (new_state == state_)
    return true;

  state_ = new_state;

  if (state_ == kStateRunning) {
    stc_ = base::TimeTicks::Now();
    DCHECK(media_time_ != ::media::kNoTimestamp());
    return true;
  }

  if (state_ == kStateIdle) {
    media_time_ = ::media::kNoTimestamp();
    return true;
  }

  return true;
}

bool MediaClockDeviceFake::ResetTimeline(base::TimeDelta time) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(state_, kStateIdle);
  media_time_ = time;
  return true;
}

bool MediaClockDeviceFake::SetRate(float rate) {
  DCHECK(CalledOnValidThread());
  if (state_ == kStateRunning) {
    base::TimeTicks now = base::TimeTicks::Now();
    media_time_ = media_time_ + (now - stc_) * rate_;
    stc_ = now;
  }

  rate_ = rate;
  return true;
}

base::TimeDelta MediaClockDeviceFake::GetTime() {
  DCHECK(CalledOnValidThread());
  if (state_ != kStateRunning)
    return media_time_;

  if (media_time_ == ::media::kNoTimestamp())
    return ::media::kNoTimestamp();

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta interpolated_media_time =
      media_time_ + (now - stc_) * rate_;
  return interpolated_media_time;
}


namespace {

// Maximum number of frames that can be buffered.
const size_t kMaxFrameCount = 20;

}  // namespace

class MediaComponentDeviceFake : public MediaComponentDevice {
 public:
  explicit MediaComponentDeviceFake(MediaClockDeviceFake* media_clock_device);
  ~MediaComponentDeviceFake() override;

  // MediaComponentDevice implementation.
  void SetClient(const Client& client) override;
  State GetState() const override;
  bool SetState(State new_state) override;
  bool SetStartPts(base::TimeDelta time) override;
  FrameStatus PushFrame(
      const scoped_refptr<DecryptContext>& decrypt_context,
      const scoped_refptr<DecoderBufferBase>& buffer,
      const FrameStatusCB& completion_cb) override;
  base::TimeDelta GetRenderingTime() const override;
  base::TimeDelta GetRenderingDelay() const override;
  bool GetStatistics(Statistics* stats) const override;

 private:
  struct FakeDecoderBuffer {
    FakeDecoderBuffer();
    ~FakeDecoderBuffer();

    // Buffer size.
    size_t size;

    // Presentation timestamp.
    base::TimeDelta pts;
  };

  void RenderTask();

  MediaClockDeviceFake* const media_clock_device_;
  Client client_;

  State state_;

  // Indicate whether the end of stream has been received.
  bool is_eos_;

  // Media time of the last rendered audio sample.
  base::TimeDelta rendering_time_;

  // Frame decoded/rendered since the pipeline left the idle state.
  uint64 decoded_frame_count_;
  uint64 decoded_byte_count_;

  // List of frames not rendered yet.
  std::list<FakeDecoderBuffer> frames_;

  // Indicate whether there is a scheduled rendering task.
  bool scheduled_rendering_task_;

  // Pending frame.
  scoped_refptr<DecoderBufferBase> pending_buffer_;
  FrameStatusCB frame_pushed_cb_;

  base::WeakPtr<MediaComponentDeviceFake> weak_this_;
  base::WeakPtrFactory<MediaComponentDeviceFake> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaComponentDeviceFake);
};

MediaComponentDeviceFake::FakeDecoderBuffer::FakeDecoderBuffer()
  : size(0) {
}

MediaComponentDeviceFake::FakeDecoderBuffer::~FakeDecoderBuffer() {
}

MediaComponentDeviceFake::MediaComponentDeviceFake(
    MediaClockDeviceFake* media_clock_device)
    : media_clock_device_(media_clock_device),
      state_(kStateUninitialized),
      rendering_time_(::media::kNoTimestamp()),
      decoded_frame_count_(0),
      decoded_byte_count_(0),
      scheduled_rendering_task_(false),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
  DetachFromThread();
}

MediaComponentDeviceFake::~MediaComponentDeviceFake() {
}

void MediaComponentDeviceFake::SetClient(const Client& client) {
  DCHECK(CalledOnValidThread());
  client_ = client;
}

MediaComponentDevice::State MediaComponentDeviceFake::GetState() const {
  DCHECK(CalledOnValidThread());
  return state_;
}

bool MediaComponentDeviceFake::SetState(State new_state) {
  DCHECK(CalledOnValidThread());
  if (!MediaComponentDevice::IsValidStateTransition(state_, new_state))
    return false;
  state_ = new_state;

  if (state_ == kStateIdle) {
    // Back to the idle state: reset a bunch of parameters.
    is_eos_ = false;
    rendering_time_ = ::media::kNoTimestamp();
    decoded_frame_count_ = 0;
    decoded_byte_count_ = 0;
    frames_.clear();
    pending_buffer_ = scoped_refptr<DecoderBufferBase>();
    frame_pushed_cb_.Reset();
    return true;
  }

  if (state_ == kStateRunning) {
    if (!scheduled_rendering_task_) {
      scheduled_rendering_task_ = true;
      base::MessageLoopProxy::current()->PostTask(
          FROM_HERE,
          base::Bind(&MediaComponentDeviceFake::RenderTask, weak_this_));
    }
    return true;
  }

  return true;
}

bool MediaComponentDeviceFake::SetStartPts(base::TimeDelta time) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(state_, kStateIdle);
  rendering_time_ = time;
  return true;
}

MediaComponentDevice::FrameStatus MediaComponentDeviceFake::PushFrame(
    const scoped_refptr<DecryptContext>& decrypt_context,
    const scoped_refptr<DecoderBufferBase>& buffer,
    const FrameStatusCB& completion_cb) {
  DCHECK(CalledOnValidThread());
  DCHECK(state_ == kStatePaused || state_ == kStateRunning);
  DCHECK(!is_eos_);
  DCHECK(!pending_buffer_.get());
  DCHECK(buffer.get());

  if (buffer->end_of_stream()) {
    is_eos_ = true;
    return kFrameSuccess;
  }

  if (frames_.size() > kMaxFrameCount) {
    pending_buffer_ = buffer;
    frame_pushed_cb_ = completion_cb;
    return kFramePending;
  }

  FakeDecoderBuffer fake_buffer;
  fake_buffer.size = buffer->data_size();
  fake_buffer.pts = buffer->timestamp();
  frames_.push_back(fake_buffer);
  return kFrameSuccess;
}

base::TimeDelta MediaComponentDeviceFake::GetRenderingTime() const {
  return rendering_time_;
}

base::TimeDelta MediaComponentDeviceFake::GetRenderingDelay() const {
  NOTIMPLEMENTED();
  return ::media::kNoTimestamp();
}

void MediaComponentDeviceFake::RenderTask() {
  scheduled_rendering_task_ = false;

  if (state_ != kStateRunning)
    return;

  base::TimeDelta media_time = media_clock_device_->GetTime();
  if (media_time == ::media::kNoTimestamp()) {
    scheduled_rendering_task_ = true;
    base::MessageLoopProxy::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&MediaComponentDeviceFake::RenderTask, weak_this_),
        base::TimeDelta::FromMilliseconds(50));
    return;
  }

  while (!frames_.empty() && frames_.front().pts <= media_time) {
    rendering_time_ = frames_.front().pts;
    decoded_frame_count_++;
    decoded_byte_count_ += frames_.front().size;
    frames_.pop_front();
    if (pending_buffer_.get()) {
      FakeDecoderBuffer fake_buffer;
      fake_buffer.size = pending_buffer_->data_size();
      fake_buffer.pts = pending_buffer_->timestamp();
      frames_.push_back(fake_buffer);
      pending_buffer_ = scoped_refptr<DecoderBufferBase>();
      base::ResetAndReturn(&frame_pushed_cb_).Run(kFrameSuccess);
    }
  }

  if (frames_.empty() && is_eos_) {
    if (!client_.eos_cb.is_null())
      client_.eos_cb.Run();
    return;
  }

  scheduled_rendering_task_ = true;
  base::MessageLoopProxy::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&MediaComponentDeviceFake::RenderTask, weak_this_),
      base::TimeDelta::FromMilliseconds(50));
}

bool MediaComponentDeviceFake::GetStatistics(Statistics* stats) const {
  if (state_ != kStateRunning)
    return false;

  // Note: what is returned here is not the number of samples but the number of
  // frames. The value is different for audio.
  stats->decoded_bytes = decoded_byte_count_;
  stats->decoded_samples = decoded_frame_count_;
  stats->dropped_samples = 0;
  return true;
}


class AudioPipelineDeviceFake : public AudioPipelineDevice {
 public:
  explicit AudioPipelineDeviceFake(MediaClockDeviceFake* media_clock_device);
  ~AudioPipelineDeviceFake() override;

  // AudioPipelineDevice implementation.
  void SetClient(const Client& client) override;
  State GetState() const override;
  bool SetState(State new_state) override;
  bool SetStartPts(base::TimeDelta time) override;
  FrameStatus PushFrame(
      const scoped_refptr<DecryptContext>& decrypt_context,
      const scoped_refptr<DecoderBufferBase>& buffer,
      const FrameStatusCB& completion_cb) override;
  base::TimeDelta GetRenderingTime() const override;
  base::TimeDelta GetRenderingDelay() const override;
  bool SetConfig(const ::media::AudioDecoderConfig& config) override;
  void SetStreamVolumeMultiplier(float multiplier) override;
  bool GetStatistics(Statistics* stats) const override;

 private:
  scoped_ptr<MediaComponentDeviceFake> fake_pipeline_;

  ::media::AudioDecoderConfig config_;

  DISALLOW_COPY_AND_ASSIGN(AudioPipelineDeviceFake);
};

AudioPipelineDeviceFake::AudioPipelineDeviceFake(
    MediaClockDeviceFake* media_clock_device)
  : fake_pipeline_(new MediaComponentDeviceFake(media_clock_device)) {
  DetachFromThread();
}

AudioPipelineDeviceFake::~AudioPipelineDeviceFake() {
}

void AudioPipelineDeviceFake::SetClient(const Client& client) {
  fake_pipeline_->SetClient(client);
}

MediaComponentDevice::State AudioPipelineDeviceFake::GetState() const {
  return fake_pipeline_->GetState();
}

bool AudioPipelineDeviceFake::SetState(State new_state) {
  bool success = fake_pipeline_->SetState(new_state);
  if (!success)
    return false;

  if (new_state == kStateIdle) {
    DCHECK(config_.IsValidConfig());
  }
  if (new_state == kStateUninitialized) {
    config_ = ::media::AudioDecoderConfig();
  }
  return true;
}

bool AudioPipelineDeviceFake::SetStartPts(base::TimeDelta time) {
  return fake_pipeline_->SetStartPts(time);
}

MediaComponentDevice::FrameStatus AudioPipelineDeviceFake::PushFrame(
    const scoped_refptr<DecryptContext>& decrypt_context,
    const scoped_refptr<DecoderBufferBase>& buffer,
    const FrameStatusCB& completion_cb) {
  return fake_pipeline_->PushFrame(decrypt_context, buffer, completion_cb);
}

base::TimeDelta AudioPipelineDeviceFake::GetRenderingTime() const {
  return fake_pipeline_->GetRenderingTime();
}

base::TimeDelta AudioPipelineDeviceFake::GetRenderingDelay() const {
  return fake_pipeline_->GetRenderingDelay();
}

bool AudioPipelineDeviceFake::SetConfig(
    const ::media::AudioDecoderConfig& config) {
  DCHECK(CalledOnValidThread());
  if (!config.IsValidConfig())
    return false;
  config_ = config;
  return true;
}

void AudioPipelineDeviceFake::SetStreamVolumeMultiplier(float multiplier) {
  DCHECK(CalledOnValidThread());
}

bool AudioPipelineDeviceFake::GetStatistics(Statistics* stats) const {
  return fake_pipeline_->GetStatistics(stats);
}


class VideoPipelineDeviceFake : public VideoPipelineDevice {
 public:
  explicit VideoPipelineDeviceFake(MediaClockDeviceFake* media_clock_device);
  ~VideoPipelineDeviceFake() override;

  // VideoPipelineDevice implementation.
  void SetClient(const Client& client) override;
  State GetState() const override;
  bool SetState(State new_state) override;
  bool SetStartPts(base::TimeDelta time) override;
  FrameStatus PushFrame(
      const scoped_refptr<DecryptContext>& decrypt_context,
      const scoped_refptr<DecoderBufferBase>& buffer,
      const FrameStatusCB& completion_cb) override;
  base::TimeDelta GetRenderingTime() const override;
  base::TimeDelta GetRenderingDelay() const override;
  void SetVideoClient(const VideoClient& client) override;
  bool SetConfig(const ::media::VideoDecoderConfig& config) override;
  bool GetStatistics(Statistics* stats) const override;

 private:
  scoped_ptr<MediaComponentDeviceFake> fake_pipeline_;

  ::media::VideoDecoderConfig config_;

  DISALLOW_COPY_AND_ASSIGN(VideoPipelineDeviceFake);
};

VideoPipelineDeviceFake::VideoPipelineDeviceFake(
    MediaClockDeviceFake* media_clock_device)
  : fake_pipeline_(new MediaComponentDeviceFake(media_clock_device)) {
  DetachFromThread();
}

VideoPipelineDeviceFake::~VideoPipelineDeviceFake() {
}

void VideoPipelineDeviceFake::SetClient(const Client& client) {
  fake_pipeline_->SetClient(client);
}

MediaComponentDevice::State VideoPipelineDeviceFake::GetState() const {
  return fake_pipeline_->GetState();
}

bool VideoPipelineDeviceFake::SetState(State new_state) {
  bool success = fake_pipeline_->SetState(new_state);
  if (!success)
    return false;

  if (new_state == kStateIdle) {
    DCHECK(config_.IsValidConfig());
  }
  if (new_state == kStateUninitialized) {
    config_ = ::media::VideoDecoderConfig();
  }
  return true;
}

bool VideoPipelineDeviceFake::SetStartPts(base::TimeDelta time) {
  return fake_pipeline_->SetStartPts(time);
}

MediaComponentDevice::FrameStatus VideoPipelineDeviceFake::PushFrame(
    const scoped_refptr<DecryptContext>& decrypt_context,
    const scoped_refptr<DecoderBufferBase>& buffer,
    const FrameStatusCB& completion_cb) {
  return fake_pipeline_->PushFrame(decrypt_context, buffer, completion_cb);
}

base::TimeDelta VideoPipelineDeviceFake::GetRenderingTime() const {
  return fake_pipeline_->GetRenderingTime();
}

base::TimeDelta VideoPipelineDeviceFake::GetRenderingDelay() const {
  return fake_pipeline_->GetRenderingDelay();
}

void VideoPipelineDeviceFake::SetVideoClient(const VideoClient& client) {
}

bool VideoPipelineDeviceFake::SetConfig(
    const ::media::VideoDecoderConfig& config) {
  DCHECK(CalledOnValidThread());
  if (!config.IsValidConfig())
    return false;
  config_ = config;
  return true;
}

bool VideoPipelineDeviceFake::GetStatistics(Statistics* stats) const {
  return fake_pipeline_->GetStatistics(stats);
}


MediaPipelineDeviceFake::MediaPipelineDeviceFake()
    : media_clock_device_(new MediaClockDeviceFake()),
      audio_pipeline_device_(
          new AudioPipelineDeviceFake(media_clock_device_.get())),
      video_pipeline_device_(
          new VideoPipelineDeviceFake(media_clock_device_.get())) {
}

MediaPipelineDeviceFake::~MediaPipelineDeviceFake() {
}

AudioPipelineDevice* MediaPipelineDeviceFake::GetAudioPipelineDevice() const {
  return audio_pipeline_device_.get();
}

VideoPipelineDevice* MediaPipelineDeviceFake::GetVideoPipelineDevice() const {
  return video_pipeline_device_.get();
}

MediaClockDevice* MediaPipelineDeviceFake::GetMediaClockDevice() const {
  return media_clock_device_.get();
}

}  // namespace media
}  // namespace chromecast
