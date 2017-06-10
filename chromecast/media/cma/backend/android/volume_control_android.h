// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_VOLUME_CONTROL_ANDROID_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_VOLUME_CONTROL_ANDROID_H_

#include <map>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chromecast/media/cma/backend/android/audio_sink_manager.h"

namespace chromecast {
namespace media {

// TODO(ckuiper): This logic is similar to the one in alsa/volume_control.cc.
// Consolidate it.
class VolumeMap {
 public:
  struct LevelToDb {
    float level;
    float db;
  };

  VolumeMap();
  ~VolumeMap();

  float VolumeToDbFS(float volume);
  float DbFSToVolume(float db);

 private:
  std::vector<LevelToDb> volume_map_;

  DISALLOW_COPY_AND_ASSIGN(VolumeMap);
};

// TODO(ckuiper): This logic is similar to the one in alsa/volume_control.cc.
// Consolidate it.
class VolumeControlAndroid {
 public:
  VolumeControlAndroid();
  ~VolumeControlAndroid();

  void AddVolumeObserver(VolumeObserver* observer);
  void RemoveVolumeObserver(VolumeObserver* observer);
  float GetVolume(AudioContentType type);
  void SetVolume(AudioContentType type, float level);
  bool IsMuted(AudioContentType type);
  void SetMuted(AudioContentType type, bool muted);
  void SetOutputLimit(AudioContentType type, float limit);

 private:
  void InitializeOnThread();
  void SetVolumeOnThread(AudioContentType type, float level);
  void SetMutedOnThread(AudioContentType type, bool muted);

  base::FilePath storage_path_;
  base::DictionaryValue stored_values_;

  base::Lock volume_lock_;
  std::map<AudioContentType, float> volumes_;
  std::map<AudioContentType, bool> muted_;

  base::Lock observer_lock_;
  std::vector<VolumeObserver*> volume_observers_;

  base::Thread thread_;
  base::WaitableEvent initialize_complete_event_;

  DISALLOW_COPY_AND_ASSIGN(VolumeControlAndroid);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_VOLUME_CONTROL_ANDROID_H_
