// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/android/loopback_audio_manager.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/internal/android/prebuilt/things/include/pio/i2s_device.h"
#include "chromecast/internal/android/prebuilt/things/include/pio/peripheral_manager_client.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/media/decoder_config.h"

#define RUN_ON_FEEDER_THREAD(method, ...)                        \
  if (!feeder_thread_.task_runner()->BelongsToCurrentThread()) { \
    POST_TASK_TO_FEEDER_THREAD(method, ##__VA_ARGS__);           \
    return;                                                      \
  }

#define POST_TASK_TO_FEEDER_THREAD(method, ...)                \
  feeder_thread_.task_runner()->PostTask(                      \
      FROM_HERE, base::BindOnce(&LoopbackAudioManager::method, \
                                base::Unretained(this), ##__VA_ARGS__));

namespace chromecast {
namespace media {

namespace {

const int kBufferLenMs = 20;
const int kMsPerSecond = 1000;
const int64_t kNsecPerSecond = 1000000000LL;
const int64_t kTimestampUpdatePeriodNsec = 10 * kNsecPerSecond;
const int64_t kInvalidTimestamp = std::numeric_limits<int64_t>::min();

class LoopbackAudioManagerInstance : public LoopbackAudioManager {
 public:
  LoopbackAudioManagerInstance() {}
  ~LoopbackAudioManagerInstance() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(LoopbackAudioManagerInstance);
};

base::LazyInstance<LoopbackAudioManagerInstance>::DestructorAtExit
    loopback_audio_manager_instance = LAZY_INSTANCE_INITIALIZER;

}  // namespace

LoopbackAudioManager::LoopbackAudioManager()
    : last_timestamp_nsec_(kInvalidTimestamp),
      last_frame_position_(0),
      frame_count_(0),
      feeder_thread_("CMA_Backend_Loopback"),
      loopback_running_(false),
      loopback_disabled_(false) {
  std::string i2s_name;
  AI2sEncoding i2s_encoding;
  GetI2sFlags(&i2s_name, &i2s_encoding);

  if (!loopback_disabled_) {
    // Open I2S device.
    APeripheralManagerClient* client = APeripheralManagerClient_new();
    DCHECK(client);

    int err = APeripheralManagerClient_openI2sDevice(
        client, i2s_name.c_str(), i2s_encoding, i2s_channels_, i2s_rate_,
        AI2S_FLAG_DIRECTION_IN, &i2s_);
    DCHECK_EQ(err, 0);

    // Spin up loopback thread.
    base::Thread::Options options;
    options.priority = base::ThreadPriority::REALTIME_AUDIO;
    CHECK(feeder_thread_.StartWithOptions(options));
  }
}

LoopbackAudioManager::~LoopbackAudioManager() {
  DCHECK(loopback_observers_.empty());
  feeder_thread_.Stop();

  // thread_.Stop() makes sure the thread is stopped before return.
  // It's okay to clean up after feeder_thread_ is stopped.
  AI2sDevice_delete(i2s_);
}

// static
LoopbackAudioManager* LoopbackAudioManager::Get() {
  return loopback_audio_manager_instance.Pointer();
}

void LoopbackAudioManager::CalibrateTimestamp(int64_t frame_position) {
  // Determine if a new calibration timestamp is needed.
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  int64_t time_sec = static_cast<int64_t>(ts.tv_sec);
  int64_t time_nsec = static_cast<int64_t>(ts.tv_nsec);
  time_nsec += time_sec * kNsecPerSecond;
  if (last_timestamp_nsec_ != kInvalidTimestamp &&
      time_nsec - last_timestamp_nsec_ < kTimestampUpdatePeriodNsec) {
    return;
  }

  // Validate that timestamp is close to interpolated estimate.
  int64_t old_last_position = last_frame_position_;
  int64_t old_last_timestamp = last_timestamp_nsec_;
  int success;
  int ret = AI2sDevice_getInputTimestamp(i2s_, &last_frame_position_,
                                         &last_timestamp_nsec_, &success);
  // An error occurred during the call.
  if (ret != 0) {
    LOG(ERROR) << "GetTimestamp call unsuccessful on loopback input";
    return;
  }

  // The system is unable to provide a timestamp, but the call succeceded.
  if (!success) {
    return;
  }
  int64_t delta_frames = last_frame_position_ - old_last_position;
  int64_t delta_nsecs = kNsecPerSecond * delta_frames / i2s_rate_;
  int64_t expected_timestamp = old_last_timestamp + delta_nsecs;
  int64_t ts_diff_micros = (last_timestamp_nsec_ - expected_timestamp) / 1000;
  if (ts_diff_micros > 1000) {
    LOG(WARNING) << "Updated timestamp, interpolated timestamp drifted by "
                 << ts_diff_micros << " microseconds";
  }
}

int64_t LoopbackAudioManager::GetInterpolatedTimestamp(int64_t frame_position) {
  CalibrateTimestamp(frame_position);
  int64_t delta_frames =
      frame_position - last_frame_position_ -
      audio_buffer_size_ / (bytes_per_sample_ * i2s_channels_);
  int64_t delta_nsecs = kNsecPerSecond * delta_frames / i2s_rate_;
  return last_timestamp_nsec_ + delta_nsecs;
}

void LoopbackAudioManager::GetI2sFlags(std::string* i2s_name,
                                       AI2sEncoding* i2s_encoding) {
  DCHECK(i2s_name);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  *i2s_name = command_line->GetSwitchValueNative(switches::kLoopbackI2sBusName);
  i2s_rate_ = GetSwitchValueInt(switches::kLoopbackI2sRateHz, -1);
  i2s_channels_ = GetSwitchValueInt(switches::kLoopbackI2sChannels, -1);
  int i2s_bits = GetSwitchValueInt(switches::kLoopbackI2sBits, -1);

  // If all loopback configuration flags are unset, assume loopback should be
  // disabled.
  if (i2s_name->empty() && i2s_rate_ == -1 && i2s_bits == -1 &&
      i2s_channels_ == -1) {
    loopback_disabled_ = true;
  } else {
    LOG_IF(DFATAL, i2s_name->empty())
        << "Flag --" << switches::kLoopbackI2sBusName << " is required.";
    LOG_IF(DFATAL, i2s_rate_ == -1)
        << "Flag --" << switches::kLoopbackI2sRateHz << " is required.";
    LOG_IF(DFATAL, i2s_bits == -1)
        << "Flag --" << switches::kLoopbackI2sBits << " is required.";
    LOG_IF(DFATAL, i2s_channels_ == -1)
        << "Flag --" << switches::kLoopbackI2sChannels << " is required.";

    switch (i2s_bits) {
      case 16:
        *i2s_encoding = AI2S_ENCODING_PCM_16_BIT;
        cast_audio_format_ = kSampleFormatS16;
        bytes_per_sample_ = 2;
        break;
      case 24:
        *i2s_encoding = AI2S_ENCODING_PCM_24_BIT;
        cast_audio_format_ = kSampleFormatS24;
        bytes_per_sample_ = 4;  // 24-bits algined to 32 bits
        break;
      case 32:
        *i2s_encoding = AI2S_ENCODING_PCM_32_BIT;
        cast_audio_format_ = kSampleFormatS32;
        bytes_per_sample_ = 4;
        break;
      default:
        LOG(FATAL)
            << "Invalid number of bits specified.  Must be one of {16, 24, 32}";
    }
  }

  // 20ms of audio
  audio_buffer_size_ =
      i2s_rate_ * bytes_per_sample_ * i2s_channels_ * kBufferLenMs / kMsPerSecond;
}

void LoopbackAudioManager::StopLoopback() {
  loopback_running_ = false;
}

void LoopbackAudioManager::StartLoopback() {
  DCHECK(feeder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(!loopback_running_);
  last_timestamp_nsec_ = kInvalidTimestamp;

  // Maintain sample count for interpolation.
  frame_count_ = 0;
  loopback_running_ = true;
  RunLoopback();
}

void LoopbackAudioManager::RunLoopback() {
  DCHECK(feeder_thread_.task_runner()->BelongsToCurrentThread());

  if (!loopback_running_) {
    return;
  }

  // Read bytes from I2S device.
  int bytes_read;
  uint8_t data[audio_buffer_size_];
  int ret = AI2sDevice_read(i2s_, data, 0, audio_buffer_size_, &bytes_read);
  if (ret != 0) {
    LOG(ERROR) << "Failed to read loopback audio";
    return;
  }
  CHECK_EQ(audio_buffer_size_, bytes_read);
  frame_count_ += bytes_read / (bytes_per_sample_ * i2s_channels_);

  // Get high-resolution timestamp.
  int64_t timestamp_ns = GetInterpolatedTimestamp(frame_count_);

  // Post data and timestamp.
  for (auto* observer : loopback_observers_) {
    observer->OnLoopbackAudio(timestamp_ns / 1000, cast_audio_format_,
                              i2s_rate_, i2s_channels_, data,
                              audio_buffer_size_);
  }

  POST_TASK_TO_FEEDER_THREAD(RunLoopback);
}

void LoopbackAudioManager::AddLoopbackAudioObserver(
    CastMediaShlib::LoopbackAudioObserver* observer) {
  if (loopback_disabled_) {
    return;
  }
  DCHECK(observer);
  RUN_ON_FEEDER_THREAD(AddLoopbackAudioObserver, observer);

  DCHECK(std::find(loopback_observers_.begin(), loopback_observers_.end(),
                   observer) == loopback_observers_.end());
  loopback_observers_.push_back(observer);
  if (loopback_observers_.size() == 1) {
    StartLoopback();
  }
}

void LoopbackAudioManager::RemoveLoopbackAudioObserver(
    CastMediaShlib::LoopbackAudioObserver* observer) {
  if (loopback_disabled_) {
    return;
  }
  DCHECK(observer);
  RUN_ON_FEEDER_THREAD(RemoveLoopbackAudioObserver, observer);

  DCHECK(std::find(loopback_observers_.begin(), loopback_observers_.end(),
                   observer) != loopback_observers_.end());
  loopback_observers_.erase(std::remove(loopback_observers_.begin(),
                                        loopback_observers_.end(), observer),
                            loopback_observers_.end());
  observer->OnRemoved();
  if (loopback_observers_.empty()) {
    StopLoopback();
  }
}

}  // namespace media
}  // namespace chromecast
