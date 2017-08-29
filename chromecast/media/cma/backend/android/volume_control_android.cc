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
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chromecast/base/init_command_line_shlib.h"
#include "chromecast/base/serializers.h"
#include "jni/VolumeControl_jni.h"

namespace chromecast {
namespace media {

namespace {

constexpr float kMinDbFS = -120.0f;

// TODO(ckuiper): Update this to reflect Android's default volume table.
const VolumeMap::LevelToDb kDefaultVolumeMap[] = {{0.0f, kMinDbFS},
                                                  {0.01f, -58.0f},
                                                  {0.090909f, -48.0f},
                                                  {0.818182f, -8.0f},
                                                  {1.0f, 0.0f}};

base::LazyInstance<VolumeMap>::Leaky g_volume_map = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<VolumeControlAndroid>::Leaky g_volume_control =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

VolumeMap::VolumeMap() {
  // TODO(ckuiper): Load active volume table from Android.
  volume_map_.insert(volume_map_.end(), kDefaultVolumeMap,
                     kDefaultVolumeMap + arraysize(kDefaultVolumeMap));
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
  DCHECK(j_volume_control_.is_null());
  j_volume_control_.Reset(Java_VolumeControl_createVolumeControl(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this)));

  // Load volume map to check that the config file is correct.
  g_volume_map.Get();

  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  thread_.StartWithOptions(options);

  thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VolumeControlAndroid::InitializeOnThread,
                                base::Unretained(this)));
  initialize_complete_event_.Wait();
}

VolumeControlAndroid::~VolumeControlAndroid() {}

// static
bool VolumeControlAndroid::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

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
      FROM_HERE, base::BindOnce(&VolumeControlAndroid::SetVolumeOnThread,
                                base::Unretained(this), type, level));
}

bool VolumeControlAndroid::IsMuted(AudioContentType type) {
  base::AutoLock lock(volume_lock_);
  return muted_[type];
}

void VolumeControlAndroid::SetMuted(AudioContentType type, bool muted) {
  thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VolumeControlAndroid::SetMutedOnThread,
                                base::Unretained(this), type, muted));
}

void VolumeControlAndroid::SetOutputLimit(AudioContentType type, float limit) {
  limit = std::max(0.0f, std::min(limit, 1.0f));
  AudioSinkManager::Get()->SetOutputLimit(type, limit);
}

void VolumeControlAndroid::OnVolumeChange(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint type,
    jfloat level) {
  thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&VolumeControlAndroid::ReportVolumeChangeOnThread,
                     base::Unretained(this), (AudioContentType)type, level));
}

void VolumeControlAndroid::OnMuteChange(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint type,
    jboolean muted) {
  thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&VolumeControlAndroid::ReportMuteChangeOnThread,
                     base::Unretained(this), (AudioContentType)type, muted));
}

void VolumeControlAndroid::InitializeOnThread() {
  DCHECK(thread_.task_runner()->BelongsToCurrentThread());

  for (auto type : {AudioContentType::kMedia, AudioContentType::kAlarm,
                    AudioContentType::kCommunication}) {
    volumes_[type] =
        Java_VolumeControl_getVolume(base::android::AttachCurrentThread(),
                                     j_volume_control_, static_cast<int>(type));
    AudioSinkManager::Get()->SetTypeVolume(type, volumes_[type]);
    muted_[type] =
        Java_VolumeControl_isMuted(base::android::AttachCurrentThread(),
                                   j_volume_control_, static_cast<int>(type));
    LOG(INFO) << __func__ << ": Initial values for"
              << " type=" << static_cast<int>(type) << ": "
              << " volume=" << volumes_[type] << " mute=" << muted_[type];
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

  LOG(INFO) << __func__ << ": level=" << level;
  // Provide the type volume to the sink manager so it can properly calculate
  // the limiter multiplier. The volume is *not* applied by the sink though.
  AudioSinkManager::Get()->SetTypeVolume(type, level);
  // Set proper volume in Android OS.
  Java_VolumeControl_setVolume(base::android::AttachCurrentThread(),
                               j_volume_control_, static_cast<int>(type),
                               level);
  ReportVolumeChangeOnThread(type, level);
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

  Java_VolumeControl_setMuted(base::android::AttachCurrentThread(),
                              j_volume_control_, static_cast<int>(type), muted);

  ReportMuteChangeOnThread(type, muted);
}

void VolumeControlAndroid::ReportVolumeChangeOnThread(AudioContentType type,
                                                      float level) {
  DCHECK(thread_.task_runner()->BelongsToCurrentThread());
  base::AutoLock lock(observer_lock_);
  for (VolumeObserver* observer : volume_observers_) {
    observer->OnVolumeChange(type, level);
  }
}

void VolumeControlAndroid::ReportMuteChangeOnThread(AudioContentType type,
                                                    bool muted) {
  DCHECK(thread_.task_runner()->BelongsToCurrentThread());
  base::AutoLock lock(observer_lock_);
  for (VolumeObserver* observer : volume_observers_) {
    observer->OnMuteChange(type, muted);
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
