// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/android/volume_control_android.h"

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
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chromecast/base/init_command_line_shlib.h"
#include "chromecast/base/serializers.h"
#include "chromecast/media/cma/backend/android/audio_sink_manager.h"

namespace chromecast {
namespace media {

namespace {

// TODO(ckuiper): figure out where this should be in Android.
const char kCastAudioConfigFilePath[] = "/etc/cast_audio.json";

constexpr float kDefaultMediaDbFS = -25.0f;
constexpr float kDefaultAlarmDbFS = -20.0f;
constexpr float kDefaultCommunicationDbFS = -25.0f;

constexpr float kMinDbFS = -120.0f;

constexpr char kKeyMediaDbFS[] = "dbfs.media";
constexpr char kKeyAlarmDbFS[] = "dbfs.alarm";
constexpr char kKeyCommunicationDbFS[] = "dbfs.communication";
constexpr char kKeyVolumeMap[] = "volume_map";
constexpr char kKeyLevel[] = "level";
constexpr char kKeyDb[] = "db";

const VolumeMap::LevelToDb kDefaultVolumeMap[] = {{0.0f, kMinDbFS},
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

base::LazyInstance<VolumeMap>::Leaky g_volume_map = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<VolumeControlAndroid>::Leaky g_volume_control =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

VolumeMap::VolumeMap() {
  auto cast_audio_config =
      DeserializeJsonFromFile(base::FilePath(kCastAudioConfigFilePath));
  const base::DictionaryValue* cast_audio_dict;
  if (!cast_audio_config ||
      !cast_audio_config->GetAsDictionary(&cast_audio_dict)) {
    LOG(WARNING) << "No cast audio config found; using default volume map.";
    volume_map_.insert(volume_map_.end(), kDefaultVolumeMap,
                       kDefaultVolumeMap + arraysize(kDefaultVolumeMap));
    return;
  }

  const base::ListValue* volume_map_list;
  if (!cast_audio_dict->GetList(kKeyVolumeMap, &volume_map_list)) {
    LOG(WARNING) << "No volume map found; using default volume map.";
    volume_map_.insert(volume_map_.end(), kDefaultVolumeMap,
                       kDefaultVolumeMap + arraysize(kDefaultVolumeMap));
    return;
  }

  double prev_level = -1.0;
  for (size_t i = 0; i < volume_map_list->GetSize(); ++i) {
    const base::DictionaryValue* volume_map_entry;
    CHECK(volume_map_list->GetDictionary(i, &volume_map_entry));

    double level;
    CHECK(volume_map_entry->GetDouble(kKeyLevel, &level));
    CHECK_GE(level, 0.0);
    CHECK_LE(level, 1.0);
    CHECK_GT(level, prev_level);
    prev_level = level;

    double db;
    CHECK(volume_map_entry->GetDouble(kKeyDb, &db));
    CHECK_LE(db, 0.0);
    if (level == 1.0) {
      CHECK_EQ(db, 0.0);
    }

    volume_map_.push_back({level, db});
  }

  if (volume_map_.empty()) {
    LOG(FATAL) << "No entries in volume map.";
    return;
  }

  if (volume_map_[0].level > 0.0) {
    volume_map_.insert(volume_map_.begin(), {0.0, kMinDbFS});
  }

  if (volume_map_.rbegin()->level < 1.0) {
    volume_map_.push_back({1.0, 0.0});
  }
}

VolumeMap::~VolumeMap() {}

float VolumeMap::VolumeToDbFS(float volume) {
  if (volume <= volume_map_[0].level) {
    return volume_map_[0].db;
  }
  for (size_t i = 1; i < volume_map_.size(); ++i) {
    if (volume < volume_map_[i].level) {
      const float x_range = volume_map_[i].level - volume_map_[i - 1].level;
      const float y_range = volume_map_[i].db - volume_map_[i - 1].db;
      const float x_pos = volume - volume_map_[i - 1].level;

      return volume_map_[i - 1].db + x_pos * y_range / x_range;
    }
  }
  return volume_map_[volume_map_.size() - 1].db;
}

float VolumeMap::DbFSToVolume(float db) {
  if (db <= volume_map_[0].db) {
    return volume_map_[0].level;
  }
  for (size_t i = 1; i < volume_map_.size(); ++i) {
    if (db < volume_map_[i].db) {
      const float x_range = volume_map_[i].db - volume_map_[i - 1].db;
      const float y_range = volume_map_[i].level - volume_map_[i - 1].level;
      const float x_pos = db - volume_map_[i - 1].db;

      return volume_map_[i - 1].level + x_pos * y_range / x_range;
    }
  }
  return volume_map_[volume_map_.size() - 1].level;
}

VolumeControlAndroid::VolumeControlAndroid()
    : thread_("VolumeControl"),
      initialize_complete_event_(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::NOT_SIGNALED) {
  // Load volume map to check that the config file is correct.
  g_volume_map.Get();

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

  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  thread_.StartWithOptions(options);

  thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&VolumeControlAndroid::InitializeOnThread,
                            base::Unretained(this)));
  initialize_complete_event_.Wait();
}

VolumeControlAndroid::~VolumeControlAndroid() {}

void VolumeControlAndroid::AddVolumeObserver(VolumeObserver* observer) {
  base::AutoLock lock(observer_lock_);
  volume_observers_.push_back(observer);
}

void VolumeControlAndroid::RemoveVolumeObserver(VolumeObserver* observer) {
  base::AutoLock lock(observer_lock_);
  volume_observers_.erase(
      std::remove(volume_observers_.begin(), volume_observers_.end(), observer),
      volume_observers_.end());
}

float VolumeControlAndroid::GetVolume(AudioContentType type) {
  base::AutoLock lock(volume_lock_);
  return volumes_[type];
}

void VolumeControlAndroid::SetVolume(AudioContentType type, float level) {
  level = std::max(0.0f, std::min(level, 1.0f));
  thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&VolumeControlAndroid::SetVolumeOnThread,
                            base::Unretained(this), type, level));
}

bool VolumeControlAndroid::IsMuted(AudioContentType type) {
  base::AutoLock lock(volume_lock_);
  return muted_[type];
}

void VolumeControlAndroid::SetMuted(AudioContentType type, bool muted) {
  thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&VolumeControlAndroid::SetMutedOnThread,
                            base::Unretained(this), type, muted));
}

void VolumeControlAndroid::SetOutputLimit(AudioContentType type, float limit) {
  limit = std::max(0.0f, std::min(limit, 1.0f));
  AudioSinkManager::Get()->SetOutputLimit(
      type, DbFsToScale(VolumeControl::VolumeToDbFS(limit)));
}

void VolumeControlAndroid::InitializeOnThread() {
  DCHECK(thread_.task_runner()->BelongsToCurrentThread());

  double dbfs;
  for (auto type : {AudioContentType::kMedia, AudioContentType::kAlarm,
                    AudioContentType::kCommunication}) {
    CHECK(stored_values_.GetDouble(ContentTypeToDbFSKey(type), &dbfs));
    volumes_[type] = VolumeControl::DbFSToVolume(dbfs);
    AudioSinkManager::Get()->SetVolume(type, DbFsToScale(dbfs));

    // Note that mute state is not persisted across reboots.
    muted_[type] = false;
  }
  initialize_complete_event_.Signal();
}

void VolumeControlAndroid::SetVolumeOnThread(AudioContentType type,
                                             float level) {
  DCHECK(thread_.task_runner()->BelongsToCurrentThread());
  {
    base::AutoLock lock(volume_lock_);
    if (level == volumes_[type]) {
      return;
    }
    volumes_[type] = level;
  }

  float dbfs = VolumeControl::VolumeToDbFS(level);
  LOG(INFO) << __func__ << ": level=" << level << " -> dbfs=" << dbfs;
  AudioSinkManager::Get()->SetVolume(type, DbFsToScale(dbfs));

  {
    base::AutoLock lock(observer_lock_);
    for (VolumeObserver* observer : volume_observers_) {
      observer->OnVolumeChange(type, level);
    }
  }

  stored_values_.SetDouble(ContentTypeToDbFSKey(type), dbfs);
  SerializeJsonToFile(storage_path_, stored_values_);
}

void VolumeControlAndroid::SetMutedOnThread(AudioContentType type, bool muted) {
  DCHECK(thread_.task_runner()->BelongsToCurrentThread());
  {
    base::AutoLock lock(volume_lock_);
    if (muted == muted_[type]) {
      return;
    }
    muted_[type] = muted;
  }

  AudioSinkManager::Get()->SetMuted(type, muted);

  {
    base::AutoLock lock(observer_lock_);
    for (VolumeObserver* observer : volume_observers_) {
      observer->OnMuteChange(type, muted);
    }
  }
}

//
// Implementation of VolumeControl as defined in public/volume_control.h
//

// static
void VolumeControl::Initialize(const std::vector<std::string>& argv) {
  // Nothing to do.
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
  return g_volume_map.Get().VolumeToDbFS(volume);
}

// static
float VolumeControl::DbFSToVolume(float db) {
  return g_volume_map.Get().DbFSToVolume(db);
}

}  // namespace media
}  // namespace chromecast
