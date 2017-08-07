// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_LOOPBACK_AUDIO_MANAGER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_LOOPBACK_AUDIO_MANAGER_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/threading/thread.h"
#include "chromecast/internal/android/prebuilt/things/include/pio/i2s_device.h"
#include "chromecast/public/cast_media_shlib.h"

struct AI2sDevice;

namespace chromecast {
namespace media {

class LoopbackAudioManager {
 public:
  // Get singleton instance of Loopback Audio Manager.
  static LoopbackAudioManager* Get();

  // Adds a loopback audio observer.
  void AddLoopbackAudioObserver(
      CastMediaShlib::LoopbackAudioObserver* observer);

  // Removes a loopback audio observer.
  void RemoveLoopbackAudioObserver(
      CastMediaShlib::LoopbackAudioObserver* observer);

 protected:
  LoopbackAudioManager();
  virtual ~LoopbackAudioManager();

 private:
  void StartLoopback();
  void StopLoopback();
  void RunLoopback();
  void CalibrateTimestamp(int64_t frame_position);
  int64_t GetInterpolatedTimestamp(int64_t frame_position);
  void GetI2sFlags(std::string* i2s_name, AI2sEncoding* i2s_encoding);

  std::vector<CastMediaShlib::LoopbackAudioObserver*> loopback_observers_;

  AI2sDevice* i2s_;

  int64_t last_timestamp_nsec_;
  int64_t last_frame_position_;
  int64_t frame_count_;

  // Initialized based on the values of castshell flags.
  int audio_buffer_size_;
  int bytes_per_sample_;
  int i2s_rate_;
  int i2s_channels_;
  SampleFormat cast_audio_format_;

  // Thread that feeds audio data into the observer from the loopback.  The
  // calls made on this thread are blocking.
  base::Thread feeder_thread_;
  bool loopback_running_;
  bool loopback_disabled_;

  DISALLOW_COPY_AND_ASSIGN(LoopbackAudioManager);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_LOOPBACK_AUDIO_MANAGER_H_
