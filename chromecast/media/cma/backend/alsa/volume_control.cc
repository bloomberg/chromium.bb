// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/public/volume_control.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chromecast/base/init_command_line_shlib.h"
#include "chromecast/base/serializers.h"
#include "chromecast/media/cma/backend/alsa/stream_mixer_alsa.h"

namespace chromecast {
namespace media {

namespace {

const float kDefaultMediaDbFS = -25.0f;
const float kDefaultAlarmDbFS = -20.0f;
const float kDefaultCommunicationDbFS = -25.0f;

const float kMinDbFS = -120.0f;

const char kKeyMediaDbFS[] = "dbfs.media";
const char kKeyAlarmDbFS[] = "dbfs.alarm";
const char kKeyCommunicationDbFS[] = "dbfs.communication";

struct LevelToDb {
  float level;
  float db;
};

const LevelToDb kVolumeMap[] = {{0.0f, kMinDbFS},
                                {0.01f, -58.0f},
                                {0.090909f, -48.0f},
                                {0.818182f, -8.0f},
                                {1.0f, 0.0f}};

float DbFsToScale(float db) {
  if (db <= kMinDbFS) {
    return 0.0f;
  }
  return std::pow(10, db / 20);
}

std::string ContentTypeToDbFSKey(AudioContentType type) {
  switch (type) {
    case AudioContentType::kAlarm:
      return kKeyAlarmDbFS;
    case AudioContentType::kCommunication:
      return kKeyCommunicationDbFS;
    default:
      return kKeyMediaDbFS;
  }
}

class VolumeControlInternal {
 public:
  VolumeControlInternal() : thread_("VolumeControl") {
    stored_values_.SetDouble(kKeyMediaDbFS, kDefaultMediaDbFS);
    stored_values_.SetDouble(kKeyAlarmDbFS, kDefaultAlarmDbFS);
    stored_values_.SetDouble(kKeyCommunicationDbFS, kDefaultCommunicationDbFS);

    auto types = {AudioContentType::kMedia, AudioContentType::kAlarm,
                  AudioContentType::kCommunication};
    double volume;

    storage_path_ = base::GetHomeDir().Append("saved_volumes");
    auto old_stored_data = DeserializeJsonFromFile(storage_path_);
    base::DictionaryValue* old_stored_dict;
    if (old_stored_data && old_stored_data->GetAsDictionary(&old_stored_dict)) {
      for (auto type : types) {
        if (old_stored_dict->GetDouble(ContentTypeToDbFSKey(type), &volume)) {
          stored_values_.SetDouble(ContentTypeToDbFSKey(type), volume);
        }
      }
    }

    for (auto type : types) {
      CHECK(stored_values_.GetDouble(ContentTypeToDbFSKey(type), &volume));
      volumes_[type] = VolumeControl::DbFSToVolume(volume);
      StreamMixerAlsa::Get()->SetVolume(type, DbFsToScale(volume));

      // Note that mute state is not persisted across reboots.
      muted_[type] = false;
    }

    thread_.Start();
  }

  void AddVolumeObserver(VolumeObserver* observer) {
    base::AutoLock lock(observer_lock_);
    volume_observers_.push_back(observer);
  }

  void RemoveVolumeObserver(VolumeObserver* observer) {
    base::AutoLock lock(observer_lock_);
    volume_observers_.erase(std::remove(volume_observers_.begin(),
                                        volume_observers_.end(), observer),
                            volume_observers_.end());
  }

  float GetVolume(AudioContentType type) {
    base::AutoLock lock(volume_lock_);
    return volumes_[type];
  }

  void SetVolume(AudioContentType type, float level) {
    level = std::max(0.0f, std::min(level, 1.0f));
    thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&VolumeControlInternal::SetVolumeOnThread,
                              base::Unretained(this), type, level));
  }

  bool IsMuted(AudioContentType type) {
    base::AutoLock lock(volume_lock_);
    return muted_[type];
  }

  void SetMuted(AudioContentType type, bool muted) {
    thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&VolumeControlInternal::SetMutedOnThread,
                              base::Unretained(this), type, muted));
  }

  void SetOutputLimit(AudioContentType type, float limit) {
    limit = std::max(0.0f, std::min(limit, 1.0f));
    StreamMixerAlsa::Get()->SetOutputLimit(
        type, DbFsToScale(VolumeControl::VolumeToDbFS(limit)));
  }

 private:
  void SetVolumeOnThread(AudioContentType type, float level) {
    DCHECK(thread_.task_runner()->BelongsToCurrentThread());
    {
      base::AutoLock lock(volume_lock_);
      if (level == volumes_[type]) {
        return;
      }
      volumes_[type] = level;
    }

    float dbfs = VolumeControl::VolumeToDbFS(level);
    StreamMixerAlsa::Get()->SetVolume(type, DbFsToScale(dbfs));

    {
      base::AutoLock lock(observer_lock_);
      for (VolumeObserver* observer : volume_observers_) {
        observer->OnVolumeChange(type, level);
      }
    }

    stored_values_.SetDouble(ContentTypeToDbFSKey(type), dbfs);
    SerializeJsonToFile(storage_path_, stored_values_);
  }

  void SetMutedOnThread(AudioContentType type, bool muted) {
    {
      base::AutoLock lock(volume_lock_);
      if (muted == muted_[type]) {
        return;
      }
      muted_[type] = muted;
    }

    StreamMixerAlsa::Get()->SetMuted(type, muted);

    {
      base::AutoLock lock(observer_lock_);
      for (VolumeObserver* observer : volume_observers_) {
        observer->OnMuteChange(type, muted);
      }
    }
  }

  base::FilePath storage_path_;
  base::DictionaryValue stored_values_;

  base::Lock volume_lock_;
  std::map<AudioContentType, float> volumes_;
  std::map<AudioContentType, bool> muted_;

  base::Lock observer_lock_;
  std::vector<VolumeObserver*> volume_observers_;

  base::Thread thread_;

  DISALLOW_COPY_AND_ASSIGN(VolumeControlInternal);
};

base::LazyInstance<VolumeControlInternal> g_volume_control =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
void VolumeControl::Initialize(const std::vector<std::string>& argv) {
  chromecast::InitCommandLineShlib(argv);
}

// static
void VolumeControl::Finalize() {
  // Nothing to do.
}

// static
void VolumeControl::AddVolumeObserver(VolumeObserver* observer) {
  g_volume_control.Get().AddVolumeObserver(observer);
}

// static
void VolumeControl::RemoveVolumeObserver(VolumeObserver* observer) {
  g_volume_control.Get().RemoveVolumeObserver(observer);
}

// static
float VolumeControl::GetVolume(AudioContentType type) {
  return g_volume_control.Get().GetVolume(type);
}

// static
void VolumeControl::SetVolume(AudioContentType type, float level) {
  g_volume_control.Get().SetVolume(type, level);
}

// static
bool VolumeControl::IsMuted(AudioContentType type) {
  return g_volume_control.Get().IsMuted(type);
}

// static
void VolumeControl::SetMuted(AudioContentType type, bool muted) {
  g_volume_control.Get().SetMuted(type, muted);
}

// static
void VolumeControl::SetOutputLimit(AudioContentType type, float limit) {
  g_volume_control.Get().SetOutputLimit(type, limit);
}

// static
float VolumeControl::VolumeToDbFS(float volume) {
  if (volume <= kVolumeMap[0].level) {
    return kVolumeMap[0].db;
  }
  for (size_t i = 1; i < arraysize(kVolumeMap); ++i) {
    if (volume < kVolumeMap[i].level) {
      const float x_diff = kVolumeMap[i].level - kVolumeMap[i - 1].level;
      const float y_diff = kVolumeMap[i].db - kVolumeMap[i - 1].db;

      return kVolumeMap[i - 1].db +
             (volume - kVolumeMap[i - 1].level) * y_diff / x_diff;
    }
  }
  return kVolumeMap[arraysize(kVolumeMap) - 1].db;
}

// static
float VolumeControl::DbFSToVolume(float db) {
  if (db <= kVolumeMap[0].db) {
    return kVolumeMap[0].level;
  }
  for (size_t i = 1; i < arraysize(kVolumeMap); ++i) {
    if (db < kVolumeMap[i].db) {
      const float x_diff = kVolumeMap[i].db - kVolumeMap[i - 1].db;
      const float y_diff = kVolumeMap[i].level - kVolumeMap[i - 1].level;

      return kVolumeMap[i - 1].level +
             (db - kVolumeMap[i - 1].db) * y_diff / x_diff;
    }
  }
  return kVolumeMap[arraysize(kVolumeMap) - 1].level;
}

}  // namespace media
}  // namespace chromecast
