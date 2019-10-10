// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/mixer_input_connection.h"

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/media/audio/mixer_service/conversions.h"
#include "chromecast/media/audio/mixer_service/mixer_service.pb.h"
#include "chromecast/media/cma/backend/mixer/stream_mixer.h"
#include "chromecast/net/io_buffer_pool.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"

namespace chromecast {
namespace media {

namespace {

const int kDefaultQueueSize = 8192;
constexpr base::TimeDelta kDefaultFadeTime =
    base::TimeDelta::FromMilliseconds(5);
constexpr base::TimeDelta kInactivityTimeout = base::TimeDelta::FromSeconds(5);

constexpr int kAudioMessageHeaderSize =
    mixer_service::MixerSocket::kAudioMessageHeaderSize;

std::string AudioContentTypeToString(media::AudioContentType type) {
  switch (type) {
    case media::AudioContentType::kAlarm:
      return "alarm";
    case media::AudioContentType::kCommunication:
      return "communication";
    default:
      return "media";
  }
}

int64_t SamplesToMicroseconds(int64_t samples, int sample_rate) {
  return ::media::AudioTimestampHelper::FramesToTime(samples, sample_rate)
      .InMicroseconds();
}

int GetFillSize(const mixer_service::OutputStreamParams& params) {
  if (params.has_fill_size_frames()) {
    return params.fill_size_frames();
  }
  // Use 10 milliseconds by default.
  return params.sample_rate() / 100;
}

int GetStartThreshold(const mixer_service::OutputStreamParams& params) {
  return std::max(params.start_threshold_frames(), 0);
}

int GetQueueSize(const mixer_service::OutputStreamParams& params) {
  int queue_size = kDefaultQueueSize;
  if (params.has_max_buffered_frames()) {
    queue_size = params.max_buffered_frames();
  }
  int start_threshold = GetStartThreshold(params);
  if (queue_size < start_threshold) {
    queue_size = start_threshold;
    LOG(INFO) << "Increase queue size to " << start_threshold
              << " to match start threshold";
  }
  return queue_size;
}

template <typename T>
bool ValidAudioData(char* data) {
  if (reinterpret_cast<uintptr_t>(data) % sizeof(T) != 0u) {
    LOG(ERROR) << "Misaligned audio data";
    return false;
  }
  return true;
}

template <typename Traits>
void ConvertInterleavedData(int num_channels,
                            char* data,
                            int data_size,
                            float* dest) {
  DCHECK(ValidAudioData<typename Traits::ValueType>(data));
  int num_frames =
      data_size / (sizeof(typename Traits::ValueType) * num_channels);
  typename Traits::ValueType* source =
      reinterpret_cast<typename Traits::ValueType*>(data);
  for (int c = 0; c < num_channels; ++c) {
    float* channel_data = dest + c * num_frames;
    for (int f = 0, read_pos = c; f < num_frames;
         ++f, read_pos += num_channels) {
      channel_data[f] = Traits::ToFloat(source[read_pos]);
    }
  }
}

template <typename Traits>
void ConvertPlanarData(char* data, int data_size, float* dest) {
  DCHECK(ValidAudioData<typename Traits::ValueType>(data));
  int samples = data_size / sizeof(typename Traits::ValueType);
  typename Traits::ValueType* source =
      reinterpret_cast<typename Traits::ValueType*>(data);
  for (int s = 0; s < samples; ++s) {
    dest[s] = Traits::ToFloat(source[s]);
  }
}

int GetFrameCount(net::IOBuffer* buffer) {
  int32_t num_frames;
  memcpy(&num_frames, buffer->data(), sizeof(num_frames));
  return num_frames;
}

int64_t GetTimestamp(net::IOBuffer* buffer) {
  int64_t timestamp;
  memcpy(&timestamp, buffer->data() + sizeof(int32_t), sizeof(timestamp));
  return timestamp;
}

float* GetAudioData(net::IOBuffer* buffer) {
  return reinterpret_cast<float*>(buffer->data() + kAudioMessageHeaderSize);
}

}  // namespace

MixerInputConnection::MixerInputConnection(
    StreamMixer* mixer,
    std::unique_ptr<mixer_service::MixerSocket> socket,
    const mixer_service::OutputStreamParams& params)
    : mixer_(mixer),
      socket_(std::move(socket)),
      fill_size_(GetFillSize(params)),
      num_channels_(params.num_channels()),
      input_samples_per_second_(params.sample_rate()),
      sample_format_(params.sample_format()),
      primary_(params.stream_type() !=
               mixer_service::OutputStreamParams::STREAM_TYPE_SFX),
      device_id_(params.has_device_id()
                     ? params.device_id()
                     : ::media::AudioDeviceDescription::kDefaultDeviceId),
      content_type_(mixer_service::ConvertContentType(params.content_type())),
      playout_channel_(params.channel_selection()),
      io_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      max_queued_frames_(GetQueueSize(params)),
      start_threshold_frames_(GetStartThreshold(params)),
      fader_(this,
             params.has_fade_frames()
                 ? params.fade_frames()
                 : ::media::AudioTimestampHelper::TimeToFrames(
                       kDefaultFadeTime,
                       input_samples_per_second_),
             num_channels_,
             input_samples_per_second_,
             1.0 /* playback_rate */),
      use_start_timestamp_(params.use_start_timestamp()),
      playback_start_timestamp_(use_start_timestamp_ ? INT64_MAX : INT64_MIN),
      playback_start_pts_(INT64_MIN),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();

  LOG(INFO) << "Create " << this << " (" << device_id_
            << "), content type: " << AudioContentTypeToString(content_type_)
            << ", fill size: " << fill_size_
            << ", channel count: " << num_channels_
            << ", start threshold: " << start_threshold_frames_
            << ", socket: " << socket_.get();
  DCHECK(mixer_);
  DCHECK(socket_);
  CHECK_GT(num_channels_, 0);
  CHECK_GT(input_samples_per_second_, 0);
  DCHECK_LE(start_threshold_frames_, max_queued_frames_);

  socket_->SetDelegate(this);

  pcm_completion_task_ =
      base::BindRepeating(&MixerInputConnection::PostPcmCompletion, weak_this_);
  eos_task_ = base::BindRepeating(&MixerInputConnection::PostEos, weak_this_);
  ready_for_playback_task_ = base::BindRepeating(
      &MixerInputConnection::PostAudioReadyForPlayback, weak_this_);

  int converted_buffer_size =
      kAudioMessageHeaderSize + num_channels_ * sizeof(float) * fill_size_;
  buffer_pool_ = base::MakeRefCounted<IOBufferPool>(
      converted_buffer_size, std::numeric_limits<size_t>::max(),
      true /* threadsafe */);
  buffer_pool_->Preallocate(start_threshold_frames_ / fill_size_ + 1);
  if (sample_format_ == mixer_service::SAMPLE_FORMAT_FLOAT_P) {
    // No format conversion needed, so just use the received buffers directly.
    socket_->UseBufferPool(buffer_pool_);
  }

  mixer_->AddInput(this);

  inactivity_timer_.Start(FROM_HERE, kInactivityTimeout, this,
                          &MixerInputConnection::OnInactivityTimeout);
}

MixerInputConnection::~MixerInputConnection() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  LOG(INFO) << "Delete " << this;
}

bool MixerInputConnection::HandleMetadata(
    const mixer_service::Generic& message) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  inactivity_timer_.Reset();

  if (message.has_set_start_timestamp()) {
    RestartPlaybackAt(message.set_start_timestamp().start_timestamp(),
                      message.set_start_timestamp().start_pts());
  }
  if (message.has_set_stream_volume()) {
    mixer_->SetVolumeMultiplier(this, message.set_stream_volume().volume());
  }
  if (message.has_set_playback_rate()) {
    SetMediaPlaybackRate(message.set_playback_rate().playback_rate());
  }
  if (message.has_set_paused()) {
    SetPaused(message.set_paused().paused());
  }
  return true;
}

bool MixerInputConnection::HandleAudioData(char* data,
                                           int size,
                                           int64_t timestamp) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  inactivity_timer_.Reset();

  const int frame_size =
      num_channels_ * mixer_service::GetSampleSizeBytes(sample_format_);
  if (size % frame_size != 0) {
    LOG(ERROR) << this
               << ": audio data size is not an integer number of frames";
    OnConnectionError();
    return false;
  }
  auto buffer = buffer_pool_->GetBuffer();
  int32_t num_frames = size / frame_size;
  size_t converted_size =
      num_frames * num_channels_ * sizeof(float) + kAudioMessageHeaderSize;
  if (converted_size > buffer_pool_->buffer_size()) {
    LOG(ERROR) << "Got unexpectedly large audio data of size " << size
               << " from " << this;
    LOG(ERROR) << "Size would be " << converted_size << " but buffers are only "
               << buffer_pool_->buffer_size();
    OnConnectionError();
    return false;
  }

  DCHECK_EQ(sizeof(int32_t), 4u);
  memcpy(buffer->data(), &num_frames, sizeof(int32_t));
  memcpy(buffer->data() + sizeof(int32_t), &timestamp, sizeof(timestamp));

  float* dest =
      reinterpret_cast<float*>(buffer->data() + kAudioMessageHeaderSize);
  switch (sample_format_) {
    case mixer_service::SAMPLE_FORMAT_INT16_I:
      ConvertInterleavedData<::media::SignedInt16SampleTypeTraits>(
          num_channels_, data, size, dest);
      break;
    case mixer_service::SAMPLE_FORMAT_INT32_I:
      ConvertInterleavedData<::media::SignedInt32SampleTypeTraits>(
          num_channels_, data, size, dest);
      break;
    case mixer_service::SAMPLE_FORMAT_FLOAT_I:
      ConvertInterleavedData<::media::Float32SampleTypeTraits>(
          num_channels_, data, size, dest);
      break;
    case mixer_service::SAMPLE_FORMAT_INT16_P:
      ConvertPlanarData<::media::SignedInt16SampleTypeTraits>(data, size, dest);
      break;
    case mixer_service::SAMPLE_FORMAT_INT32_P:
      ConvertPlanarData<::media::SignedInt32SampleTypeTraits>(data, size, dest);
      break;
    case mixer_service::SAMPLE_FORMAT_FLOAT_P:
      memcpy(dest, data, size);
      break;
    default:
      NOTREACHED() << "Unhandled sample format " << sample_format_;
  }

  WritePcm(std::move(buffer));
  return true;
}

bool MixerInputConnection::HandleAudioBuffer(
    scoped_refptr<net::IOBuffer> buffer,
    char* data,
    int size,
    int64_t timestamp) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  inactivity_timer_.Reset();

  DCHECK_EQ(data - buffer->data(), kAudioMessageHeaderSize);
  DCHECK_EQ(sample_format_, mixer_service::SAMPLE_FORMAT_FLOAT_P);

  int32_t num_frames = size / (sizeof(float) * num_channels_);
  DCHECK_EQ(sizeof(int32_t), 4u);
  memcpy(buffer->data(), &num_frames, sizeof(int32_t));

  WritePcm(std::move(buffer));
  return true;
}

void MixerInputConnection::OnConnectionError() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  socket_.reset();
  weak_factory_.InvalidateWeakPtrs();

  LOG(INFO) << "Remove " << this;
  bool remove_self = false;
  {
    base::AutoLock lock(lock_);
    pending_data_ = nullptr;
    state_ = State::kRemoved;
    remove_self = mixer_error_;
  }

  if (remove_self) {
    mixer_->RemoveInput(this);
  }
}

void MixerInputConnection::OnInactivityTimeout() {
  LOG(INFO) << "Timed out " << this << " due to inactivity";
  OnConnectionError();
}

void MixerInputConnection::RestartPlaybackAt(int64_t timestamp, int64_t pts) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  LOG(INFO) << this << " RestartPlaybackAt timestamp=" << timestamp
            << " pts=" << pts;
  {
    base::AutoLock lock(lock_);
    playback_start_pts_ = pts;
    playback_start_timestamp_ = timestamp;
    use_start_timestamp_ = true;
    started_ = false;
    queued_frames_ += current_buffer_offset_;
    current_buffer_offset_ = 0;

    while (!queue_.empty()) {
      net::IOBuffer* buffer = queue_.front().get();
      int64_t frames = GetFrameCount(buffer);
      if (GetTimestamp(buffer) +
              SamplesToMicroseconds(frames, input_samples_per_second_) >=
          pts) {
        break;
      }

      queued_frames_ -= frames;
      queue_.pop_front();
    }
  }
}

void MixerInputConnection::SetMediaPlaybackRate(double rate) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  LOG(INFO) << this << " SetMediaPlaybackRate rate=" << rate;
  DCHECK_GT(rate, 0);

  base::AutoLock lock(lock_);
  playback_rate_ = rate;
}

void MixerInputConnection::SetPaused(bool paused) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  LOG(INFO) << (paused ? "Pausing " : "Unpausing ") << this;
  if (paused) {
    inactivity_timer_.Stop();
  } else {
    inactivity_timer_.Start(FROM_HERE, kInactivityTimeout, this,
                            &MixerInputConnection::OnInactivityTimeout);
  }

  {
    base::AutoLock lock(lock_);
    if (paused == paused_) {
      return;
    }

    paused_ = paused;
    mixer_rendering_delay_ = RenderingDelay();
    // Clear start timestamp, since a pause should invalidate the start
    // timestamp anyway. The AV sync code can restart (hard correction) on
    // resume if needed.
    use_start_timestamp_ = false;
    playback_start_timestamp_ = INT64_MIN;
  }
  mixer_->UpdateStreamCounts();
}

int MixerInputConnection::num_channels() {
  return num_channels_;
}
int MixerInputConnection::input_samples_per_second() {
  return input_samples_per_second_;
}
bool MixerInputConnection::primary() {
  return primary_;
}
const std::string& MixerInputConnection::device_id() {
  return device_id_;
}
AudioContentType MixerInputConnection::content_type() {
  return content_type_;
}
int MixerInputConnection::desired_read_size() {
  return fill_size_;
}
int MixerInputConnection::playout_channel() {
  return playout_channel_;
}

bool MixerInputConnection::active() {
  base::AutoLock lock(lock_);
  return !paused_;
}

void MixerInputConnection::WritePcm(scoped_refptr<net::IOBuffer> data) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  RenderingDelay delay;
  bool queued;
  {
    base::AutoLock lock(lock_);
    if (state_ == State::kUninitialized ||
        queued_frames_ + fader_.buffered_frames() >= max_queued_frames_) {
      if (pending_data_) {
        LOG(ERROR) << "Got unexpected audio data for " << this;
      }
      pending_data_ = std::move(data);
      queued = false;
    } else {
      delay = QueueData(std::move(data));
      queued = true;
    }
  }

  if (queued) {
    mixer_service::Generic message;
    message.mutable_push_result()->set_next_playback_timestamp(
        delay.timestamp_microseconds + delay.delay_microseconds);
    socket_->SendProto(message);
  }
}

MixerInputConnection::RenderingDelay MixerInputConnection::QueueData(
    scoped_refptr<net::IOBuffer> data) {
  int frames = GetFrameCount(data.get());
  if (frames == 0) {
    LOG(INFO) << "End of stream for " << this;
    state_ = State::kGotEos;
    if (!started_) {
      io_task_runner_->PostTask(FROM_HERE, ready_for_playback_task_);
    }
  } else if (started_ ||
             GetTimestamp(data.get()) +
                     SamplesToMicroseconds(frames, input_samples_per_second_) >=
                 playback_start_pts_) {
    queued_frames_ += frames;
    queue_.push_back(std::move(data));

    if (!started_ && queued_frames_ >= start_threshold_frames_ &&
        mixer_rendering_delay_.timestamp_microseconds != INT64_MIN) {
      io_task_runner_->PostTask(FROM_HERE, ready_for_playback_task_);
    }
  }
  // Otherwise, drop |data| since it is before the start PTS.

  RenderingDelay delay;
  if (started_ && !paused_) {
    delay = mixer_rendering_delay_;
    delay.delay_microseconds += SamplesToMicroseconds(
        queued_frames_ + extra_delay_frames_, input_samples_per_second_);
  }
  return delay;
}

void MixerInputConnection::InitializeAudioPlayback(
    int read_size,
    RenderingDelay initial_rendering_delay) {
  // Start accepting buffers into the queue.
  bool queued_data = false;
  {
    base::AutoLock lock(lock_);
    if (start_threshold_frames_ == 0) {
      start_threshold_frames_ = read_size + fill_size_;
      LOG(INFO) << this
                << " Updated start threshold: " << start_threshold_frames_;
    }
    mixer_rendering_delay_ = initial_rendering_delay;
    if (state_ == State::kUninitialized) {
      state_ = State::kNormalPlayback;
    } else {
      DCHECK_EQ(state_, State::kRemoved);
    }

    if (pending_data_ &&
        queued_frames_ + fader_.buffered_frames() < max_queued_frames_) {
      last_buffer_delay_ = QueueData(std::move(pending_data_));
      queued_data = true;
    }
  }

  if (queued_data) {
    io_task_runner_->PostTask(FROM_HERE, pcm_completion_task_);
  }
}

void MixerInputConnection::CheckAndStartPlaybackIfNecessary(
    int num_frames,
    int64_t playback_absolute_timestamp) {
  DCHECK(state_ == State::kNormalPlayback || state_ == State::kGotEos);
  DCHECK(!started_);

  const bool have_enough_queued_frames =
      (state_ == State::kGotEos ||
       (queued_frames_ >= start_threshold_frames_ &&
        queued_frames_ >= fader_.FramesNeededFromSource(num_frames)));
  if (!have_enough_queued_frames) {
    return;
  }

  remaining_silence_frames_ = 0;
  if (!use_start_timestamp_ || (queue_.empty() && state_ == State::kGotEos)) {
    // No start timestamp, so start as soon as there are enough queued frames.
    started_ = true;
    return;
  }

  if (playback_absolute_timestamp +
          SamplesToMicroseconds(num_frames, input_samples_per_second_) <
      playback_start_timestamp_) {
    // Haven't reached the start timestamp yet.
    return;
  }

  DCHECK(!queue_.empty());
  // Reset the current buffer offset to 0 so we can ignore it below. We need to
  // do this here because we may not have started playback even after dropping
  // all necessary frames the last time we checked.
  queued_frames_ += current_buffer_offset_;
  current_buffer_offset_ = 0;

  int64_t desired_pts_now = playback_start_pts_ + (playback_absolute_timestamp -
                                                   playback_start_timestamp_) *
                                                      playback_rate_;
  int64_t actual_pts_now = GetTimestamp(queue_.front().get());
  int64_t drop_us = (desired_pts_now - actual_pts_now) / playback_rate_;

  if (drop_us >= 0) {
    LOG(INFO) << this << " Dropping audio, duration = " << drop_us;
    DropAudio(::media::AudioTimestampHelper::TimeToFrames(
        base::TimeDelta::FromMicroseconds(drop_us), input_samples_per_second_));
    // Only start if we still have enough data to do so.
    started_ = (queued_frames_ >= start_threshold_frames_ &&
                queued_frames_ >= fader_.FramesNeededFromSource(num_frames));

    if (started_) {
      int64_t start_pts = GetTimestamp(queue_.front().get()) +
                          SamplesToMicroseconds(current_buffer_offset_,
                                                input_samples_per_second_) *
                              playback_rate_;
      LOG(INFO) << this << " Start playback of PTS " << start_pts << " at "
                << playback_absolute_timestamp;
    }
  } else {
    int64_t silence_duration = -drop_us;
    LOG(INFO) << this << " Adding silence. Duration = " << silence_duration;
    remaining_silence_frames_ = ::media::AudioTimestampHelper::TimeToFrames(
        base::TimeDelta::FromMicroseconds(silence_duration),
        input_samples_per_second_);
    started_ = true;
    LOG(INFO) << this << " Should start playback of PTS " << actual_pts_now
              << " at " << (playback_absolute_timestamp + silence_duration);
  }
}

void MixerInputConnection::DropAudio(int64_t frames_to_drop) {
  DCHECK_EQ(current_buffer_offset_, 0);

  while (frames_to_drop && !queue_.empty()) {
    int64_t first_buffer_frames = GetFrameCount(queue_.front().get());
    if (first_buffer_frames > frames_to_drop) {
      current_buffer_offset_ = frames_to_drop;
      queued_frames_ -= frames_to_drop;
      frames_to_drop = 0;
      break;
    }

    queued_frames_ -= first_buffer_frames;
    frames_to_drop -= first_buffer_frames;
    queue_.pop_front();
  }

  if (frames_to_drop > 0) {
    LOG(INFO) << this << " Still need to drop " << frames_to_drop << " frames";
  }
}

int MixerInputConnection::FillAudioPlaybackFrames(
    int num_frames,
    RenderingDelay rendering_delay,
    ::media::AudioBus* buffer) {
  DCHECK(buffer);
  DCHECK_EQ(num_channels_, buffer->channels());
  DCHECK_GE(buffer->frames(), num_frames);

  int64_t playback_absolute_timestamp = rendering_delay.delay_microseconds +
                                        rendering_delay.timestamp_microseconds;

  int filled = 0;
  bool queued_more_data = false;
  bool signal_eos = false;
  bool remove_self = false;
  {
    base::AutoLock lock(lock_);

    // Playback start check.
    if (!started_ &&
        (state_ == State::kNormalPlayback || state_ == State::kGotEos)) {
      CheckAndStartPlaybackIfNecessary(num_frames, playback_absolute_timestamp);
    }

    // In normal playback, don't pass data to the fader if we can't satisfy the
    // full request. This will allow us to buffer up more data so we can fully
    // fade in.
    if (state_ == State::kNormalPlayback && started_ &&
        queued_frames_ < fader_.FramesNeededFromSource(num_frames)) {
      LOG_IF(INFO, !zero_fader_frames_) << "Stream underrun for " << this;
      zero_fader_frames_ = true;
    } else {
      LOG_IF(INFO, started_ && zero_fader_frames_)
          << "Stream underrun recovered for " << this;
      zero_fader_frames_ = false;
    }

    DCHECK_GE(remaining_silence_frames_, 0);
    if (remaining_silence_frames_ >= num_frames) {
      remaining_silence_frames_ -= num_frames;
      return 0;
    }

    int write_offset = 0;
    if (remaining_silence_frames_ > 0) {
      buffer->ZeroFramesPartial(0, remaining_silence_frames_);
      filled += remaining_silence_frames_;
      num_frames -= remaining_silence_frames_;
      write_offset = remaining_silence_frames_;
      remaining_silence_frames_ = 0;
    }

    float* channels[num_channels_];
    for (int c = 0; c < num_channels_; ++c) {
      channels[c] = buffer->channel(c) + write_offset;
    }
    filled = fader_.FillFrames(num_frames, rendering_delay, channels);

    mixer_rendering_delay_ = rendering_delay;
    extra_delay_frames_ = num_frames + fader_.buffered_frames();

    // See if we can accept more data into the queue.
    if (pending_data_ &&
        queued_frames_ + fader_.buffered_frames() < max_queued_frames_) {
      last_buffer_delay_ = QueueData(std::move(pending_data_));
      queued_more_data = true;
    }

    // Check if we have played out EOS.
    if (state_ == State::kGotEos && queued_frames_ == 0 &&
        fader_.buffered_frames() == 0) {
      signal_eos = true;
      state_ = State::kSignaledEos;
    }

    // If the caller has removed this source, delete once we have faded out.
    if (state_ == State::kRemoved && fader_.buffered_frames() == 0) {
      remove_self = true;
    }
  }

  if (queued_more_data) {
    io_task_runner_->PostTask(FROM_HERE, pcm_completion_task_);
  }
  if (signal_eos) {
    io_task_runner_->PostTask(FROM_HERE, eos_task_);
  }

  if (remove_self) {
    mixer_->RemoveInput(this);
  }
  return filled;
}

int MixerInputConnection::FillFaderFrames(int num_frames,
                                          RenderingDelay rendering_delay,
                                          float* const* channels) {
  DCHECK(channels);

  if (zero_fader_frames_ || !started_ || paused_ || state_ == State::kRemoved) {
    return 0;
  }

  int num_filled = 0;
  while (num_frames) {
    if (queue_.empty()) {
      return num_filled;
    }

    net::IOBuffer* buffer = queue_.front().get();
    const int buffer_frames = GetFrameCount(buffer);
    const int frames_to_copy =
        std::min(num_frames, buffer_frames - current_buffer_offset_);
    DCHECK(frames_to_copy >= 0 && frames_to_copy <= num_frames)
        << " frames_to_copy=" << frames_to_copy << " num_frames=" << num_frames
        << " buffer_frames=" << buffer_frames << " num_filled=" << num_filled
        << " current_buffer_offset_=" << current_buffer_offset_;

    const float* buffer_samples = GetAudioData(buffer);
    for (int c = 0; c < num_channels_; ++c) {
      const float* buffer_channel = buffer_samples + (buffer_frames * c);
      std::copy_n(buffer_channel + current_buffer_offset_, frames_to_copy,
                  channels[c] + num_filled);
    }

    num_frames -= frames_to_copy;
    queued_frames_ -= frames_to_copy;
    num_filled += frames_to_copy;

    current_buffer_offset_ += frames_to_copy;
    if (current_buffer_offset_ == buffer_frames) {
      queue_.pop_front();
      current_buffer_offset_ = 0;
    }
  }

  return num_filled;
}

void MixerInputConnection::PostPcmCompletion() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  mixer_service::Generic message;
  auto* push_result = message.mutable_push_result();
  {
    base::AutoLock lock(lock_);
    push_result->set_next_playback_timestamp(
        last_buffer_delay_.timestamp_microseconds +
        last_buffer_delay_.delay_microseconds);
  }
  socket_->SendProto(message);
}

void MixerInputConnection::PostEos() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  mixer_service::Generic message;
  message.mutable_eos_played_out();
  socket_->SendProto(message);
}

void MixerInputConnection::PostAudioReadyForPlayback() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  if (audio_ready_for_playback_fired_) {
    return;
  }
  LOG(INFO) << this << " ready for playback";

  mixer_service::Generic message;
  auto* ready_for_playback = message.mutable_ready_for_playback();
  {
    base::AutoLock lock(lock_);
    ready_for_playback->set_delay_microseconds(
        mixer_rendering_delay_.delay_microseconds);
  }
  socket_->SendProto(message);
  audio_ready_for_playback_fired_ = true;
}

void MixerInputConnection::OnAudioPlaybackError(MixerError error) {
  if (error == MixerError::kInputIgnored) {
    LOG(INFO) << "Mixer input " << this
              << " now being ignored due to output sample rate change";
  }

  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MixerInputConnection::PostError, weak_this_, error));

  base::AutoLock lock(lock_);
  mixer_error_ = true;
  if (state_ == State::kRemoved) {
    mixer_->RemoveInput(this);
  }
}

void MixerInputConnection::PostError(MixerError error) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  mixer_service::Generic message;
  message.mutable_error()->set_type(mixer_service::Error::INVALID_STREAM_ERROR);
  socket_->SendProto(message);

  OnConnectionError();
}

void MixerInputConnection::FinalizeAudioPlayback() {
  io_task_runner_->DeleteSoon(FROM_HERE, this);
}

}  // namespace media
}  // namespace chromecast
