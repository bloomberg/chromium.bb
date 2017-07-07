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
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chromecast/base/init_command_line_shlib.h"
#include "chromecast/base/serializers.h"
#include "chromecast/media/cma/backend/alsa/alsa_features.h"
#include "chromecast/media/cma/backend/alsa/alsa_volume_control.h"
#include "chromecast/media/cma/backend/alsa/cast_audio_json.h"
#include "chromecast/media/cma/backend/alsa/post_processing_pipeline_parser.h"
#include "chromecast/media/cma/backend/alsa/stream_mixer_alsa.h"
#include "chromecast/media/cma/backend/alsa/volume_map.h"

namespace chromecast {
namespace media {

namespace {

constexpr float kDefaultMediaDbFS = -25.0f;
constexpr float kDefaultAlarmDbFS = -20.0f;
constexpr float kDefaultCommunicationDbFS = -25.0f;

constexpr float kMinDbFS = -120.0f;

constexpr char kKeyMediaDbFS[] = "dbfs.media";
constexpr char kKeyAlarmDbFS[] = "dbfs.alarm";
constexpr char kKeyCommunicationDbFS[] = "dbfs.communication";
constexpr char kKeyDefaultVolume[] = "default_volume";

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

class VolumeControlInternal : public AlsaVolumeControl::Delegate {
 public:
  VolumeControlInternal()
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
    } else {
      // If saved_volumes does not exist, use per device default if it exists.
      auto cast_audio_config =
          DeserializeJsonFromFile(base::FilePath(kCastAudioJsonFilePath));
      const base::DictionaryValue* cast_audio_dict;
      if (cast_audio_config &&
          cast_audio_config->GetAsDictionary(&cast_audio_dict)) {
        const base::DictionaryValue* default_volume_dict;
        if (cast_audio_dict && cast_audio_dict->GetDictionary(
                                   kKeyDefaultVolume, &default_volume_dict)) {
          for (auto type : types) {
            if (default_volume_dict->GetDouble(ContentTypeToDbFSKey(type),
                                               &volume)) {
              stored_values_.SetDouble(ContentTypeToDbFSKey(type), volume);
              LOG(INFO) << "Setting default volume for "
                        << ContentTypeToDbFSKey(type) << " to " << volume;
            }
          }
        }
      }
    }

    base::Thread::Options options;
    options.message_loop_type = base::MessageLoop::TYPE_IO;
    thread_.StartWithOptions(options);

    thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&VolumeControlInternal::InitializeOnThread,
                              base::Unretained(this)));
    initialize_complete_event_.Wait();
  }

  ~VolumeControlInternal() override = default;

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
        FROM_HERE,
        base::Bind(&VolumeControlInternal::SetVolumeOnThread,
                   base::Unretained(this), type, level, false /* from_alsa */));
  }

  bool IsMuted(AudioContentType type) {
    base::AutoLock lock(volume_lock_);
    return muted_[type];
  }

  void SetMuted(AudioContentType type, bool muted) {
    thread_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&VolumeControlInternal::SetMutedOnThread,
                   base::Unretained(this), type, muted, false /* from_alsa */));
  }

  void SetOutputLimit(AudioContentType type, float limit) {
    if (BUILDFLAG(ALSA_OWNS_VOLUME)) {
      return;
    }
    limit = std::max(0.0f, std::min(limit, 1.0f));
    StreamMixerAlsa::Get()->SetOutputLimit(
        type, DbFsToScale(VolumeControl::VolumeToDbFS(limit)));
  }

 private:
  void InitializeOnThread() {
    DCHECK(thread_.task_runner()->BelongsToCurrentThread());
    alsa_volume_control_ = base::MakeUnique<AlsaVolumeControl>(this);

    double dbfs;
    for (auto type : {AudioContentType::kMedia, AudioContentType::kAlarm,
                      AudioContentType::kCommunication}) {
      CHECK(stored_values_.GetDouble(ContentTypeToDbFSKey(type), &dbfs));
      volumes_[type] = VolumeControl::DbFSToVolume(dbfs);
      if (BUILDFLAG(ALSA_OWNS_VOLUME)) {
        // If ALSA owns volume, our internal mixer should not apply any scaling
        // multiplier.
        StreamMixerAlsa::Get()->SetVolume(type, 1.0f);
      } else {
        StreamMixerAlsa::Get()->SetVolume(type, DbFsToScale(dbfs));
      }

      // Note that mute state is not persisted across reboots.
      muted_[type] = false;
    }

    if (BUILDFLAG(ALSA_OWNS_VOLUME)) {
      // If ALSA owns the volume, then read the current volume and mute state
      // from the ALSA mixer element(s).
      volumes_[AudioContentType::kMedia] = alsa_volume_control_->GetVolume();
      muted_[AudioContentType::kMedia] = alsa_volume_control_->IsMuted();
    } else {
      // Otherwise, make sure the ALSA mixer element correctly reflects the
      // current volume state.
      alsa_volume_control_->SetVolume(volumes_[AudioContentType::kMedia]);
      alsa_volume_control_->SetMuted(false);
    }

    initialize_complete_event_.Signal();
  }

  void SetVolumeOnThread(AudioContentType type, float level, bool from_alsa) {
    DCHECK(thread_.task_runner()->BelongsToCurrentThread());
    DCHECK(!from_alsa || type == AudioContentType::kMedia);
    {
      base::AutoLock lock(volume_lock_);
      if (from_alsa && alsa_volume_control_->VolumeThroughAlsa(
                           volumes_[AudioContentType::kMedia]) == level) {
        return;
      }
      if (level == volumes_[type]) {
        return;
      }
      volumes_[type] = level;
    }

    float dbfs = VolumeControl::VolumeToDbFS(level);
    if (!BUILDFLAG(ALSA_OWNS_VOLUME)) {
      StreamMixerAlsa::Get()->SetVolume(type, DbFsToScale(dbfs));
    }

    if (!from_alsa && type == AudioContentType::kMedia) {
      alsa_volume_control_->SetVolume(level);
    }

    {
      base::AutoLock lock(observer_lock_);
      for (VolumeObserver* observer : volume_observers_) {
        observer->OnVolumeChange(type, level);
      }
    }

    stored_values_.SetDouble(ContentTypeToDbFSKey(type), dbfs);
    SerializeJsonToFile(storage_path_, stored_values_);
  }

  void SetMutedOnThread(AudioContentType type, bool muted, bool from_alsa) {
    DCHECK(thread_.task_runner()->BelongsToCurrentThread());
    {
      base::AutoLock lock(volume_lock_);
      if (muted == muted_[type]) {
        return;
      }
      muted_[type] = muted;
    }

    if (!BUILDFLAG(ALSA_OWNS_VOLUME)) {
      StreamMixerAlsa::Get()->SetMuted(type, muted);
    }

    if (!from_alsa && type == AudioContentType::kMedia) {
      alsa_volume_control_->SetMuted(muted);
    }

    {
      base::AutoLock lock(observer_lock_);
      for (VolumeObserver* observer : volume_observers_) {
        observer->OnMuteChange(type, muted);
      }
    }
  }

  // AlsaVolumeControl::Delegate implementation:
  void OnAlsaVolumeOrMuteChange(float new_volume, bool new_mute) override {
    DCHECK(thread_.task_runner()->BelongsToCurrentThread());
    SetVolumeOnThread(AudioContentType::kMedia, new_volume,
                      true /* from_alsa */);
    SetMutedOnThread(AudioContentType::kMedia, new_mute, true /* from_alsa */);
  }

  base::FilePath storage_path_;
  base::DictionaryValue stored_values_;

  base::Lock volume_lock_;
  std::map<AudioContentType, float> volumes_;
  std::map<AudioContentType, bool> muted_;

  base::Lock observer_lock_;
  std::vector<VolumeObserver*> volume_observers_;

  base::Thread thread_;
  base::WaitableEvent initialize_complete_event_;

  std::unique_ptr<AlsaVolumeControl> alsa_volume_control_;

  DISALLOW_COPY_AND_ASSIGN(VolumeControlInternal);
};

base::LazyInstance<VolumeControlInternal>::Leaky g_volume_control =
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
  return g_volume_map.Get().VolumeToDbFS(volume);
}

// static
float VolumeControl::DbFSToVolume(float db) {
  return g_volume_map.Get().DbFSToVolume(db);
}

}  // namespace media
}  // namespace chromecast
