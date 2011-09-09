// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/audio_mixer_alsa.h"

#include <unistd.h>

#include <algorithm>
#include <cmath>

#include <alsa/asoundlib.h>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"

typedef long alsa_long_t;  // 'long' is required for ALSA API calls.

using std::max;
using std::min;
using std::string;

namespace chromeos {

namespace {

// Name of the ALSA card to which we connect.
const char kCardName[] = "default";

// Mixer element names.  We'll use the first master element from the list that
// exists.
const char* const kMasterElementNames[] = {
  "Master",   // x86
  "Digital",  // ARM
};
const char kPCMElementName[] = "PCM";

// Default minimum and maximum volume (before we've loaded the actual range from
// ALSA), in decibels.
const double kDefaultMinVolumeDb = -90.0;
const double kDefaultMaxVolumeDb = 0.0;

// Default value assigned to the pref when it's first created, in decibels.
const double kDefaultVolumeDb = -10.0;

// Values used for muted preference.
const int kPrefMuteOff = 0;
const int kPrefMuteOn = 1;

// Number of seconds that we'll sleep between each connection attempt.
const int kConnectionRetrySleepSec = 1;

// Connection attempt number (1-indexed) for which we'll log an error if we're
// still failing.  This is set high enough to give the ALSA modules some time to
// be loaded into the kernel.  We want to log an error eventually if something
// is broken, but we don't want to continue spamming the log indefinitely.
const int kConnectionAttemptToLogFailure = 10;

}  // namespace

AudioMixerAlsa::AudioMixerAlsa()
    : min_volume_db_(kDefaultMinVolumeDb),
      max_volume_db_(kDefaultMaxVolumeDb),
      volume_db_(kDefaultVolumeDb),
      is_muted_(false),
      apply_is_pending_(true),
      alsa_mixer_(NULL),
      pcm_element_(NULL),
      prefs_(NULL),
      disconnected_event_(true, false),
      num_connection_attempts_(0) {
}

AudioMixerAlsa::~AudioMixerAlsa() {
  if (!thread_.get())
    return;
  DCHECK(MessageLoop::current() != thread_->message_loop());

  thread_->message_loop()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &AudioMixerAlsa::Disconnect));
  disconnected_event_.Wait();

  base::ThreadRestrictions::ScopedAllowIO allow_io_for_thread_join;
  thread_->Stop();
  thread_.reset();
}

void AudioMixerAlsa::Init() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  prefs_ = g_browser_process->local_state();
  volume_db_ = prefs_->GetDouble(prefs::kAudioVolume);
  is_muted_ = prefs_->GetInteger(prefs::kAudioMute);

  DCHECK(!thread_.get()) << "Init() called twice";
  thread_.reset(new base::Thread("AudioMixerAlsa"));
  CHECK(thread_->Start());
  thread_->message_loop()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &AudioMixerAlsa::Connect));
}

bool AudioMixerAlsa::IsInitialized() {
  base::AutoLock lock(lock_);
  return alsa_mixer_ != NULL;
}

void AudioMixerAlsa::GetVolumeLimits(double* min_volume_db,
                                     double* max_volume_db) {
  base::AutoLock lock(lock_);
  if (min_volume_db)
    *min_volume_db = min_volume_db_;
  if (max_volume_db)
    *max_volume_db = max_volume_db_;
}

double AudioMixerAlsa::GetVolumeDb() {
  base::AutoLock lock(lock_);
  return volume_db_;
}

void AudioMixerAlsa::SetVolumeDb(double volume_db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  {
    base::AutoLock lock(lock_);
    if (isnan(volume_db)) {
      LOG(WARNING) << "Got request to set volume to NaN";
      volume_db = min_volume_db_;
    } else {
      volume_db = min(max(volume_db, min_volume_db_), max_volume_db_);
    }
  }

  prefs_->SetDouble(prefs::kAudioVolume, volume_db);
  prefs_->ScheduleSavePersistentPrefs();

  base::AutoLock lock(lock_);
  volume_db_ = volume_db;
  if (!apply_is_pending_)
    thread_->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &AudioMixerAlsa::ApplyState));
}

bool AudioMixerAlsa::IsMuted() {
  base::AutoLock lock(lock_);
  return is_muted_;
}

void AudioMixerAlsa::SetMuted(bool muted) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  prefs_->SetInteger(prefs::kAudioMute, muted ? kPrefMuteOn : kPrefMuteOff);
  prefs_->ScheduleSavePersistentPrefs();
  base::AutoLock lock(lock_);
  is_muted_ = muted;
  if (!apply_is_pending_)
    thread_->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &AudioMixerAlsa::ApplyState));
}

// static
void AudioMixerAlsa::RegisterPrefs(PrefService* local_state) {
  // TODO(derat): Store audio volume percent instead of decibels.
  if (!local_state->FindPreference(prefs::kAudioVolume))
    local_state->RegisterDoublePref(prefs::kAudioVolume,
                                    kDefaultVolumeDb,
                                    PrefService::UNSYNCABLE_PREF);
  if (!local_state->FindPreference(prefs::kAudioMute))
    local_state->RegisterIntegerPref(prefs::kAudioMute,
                                     kPrefMuteOff,
                                     PrefService::UNSYNCABLE_PREF);
}

void AudioMixerAlsa::Connect() {
  DCHECK(MessageLoop::current() == thread_->message_loop());
  DCHECK(!alsa_mixer_);

  if (disconnected_event_.IsSignaled())
    return;

  if (!ConnectInternal()) {
    thread_->message_loop()->PostDelayedTask(FROM_HERE,
        NewRunnableMethod(this, &AudioMixerAlsa::Connect),
        kConnectionRetrySleepSec * 1000);
  }
}

bool AudioMixerAlsa::ConnectInternal() {
  DCHECK(MessageLoop::current() == thread_->message_loop());
  num_connection_attempts_++;
  int err;
  snd_mixer_t* handle = NULL;

  if ((err = snd_mixer_open(&handle, 0)) < 0) {
    if (num_connection_attempts_ == kConnectionAttemptToLogFailure)
      LOG(WARNING) << "Mixer open error: " << snd_strerror(err);
    return false;
  }

  if ((err = snd_mixer_attach(handle, kCardName)) < 0) {
    if (num_connection_attempts_ == kConnectionAttemptToLogFailure)
      LOG(WARNING) << "Attach to card " << kCardName << " failed: "
                   << snd_strerror(err);
    snd_mixer_close(handle);
    return false;
  }

  // Verify PCM can be opened, which also instantiates the PCM mixer element
  // which is needed for finer volume control and for muting by setting to zero.
  // If it fails, we can still try to use the mixer as best we can.
  snd_pcm_t* pcm_out_handle;
  if ((err = snd_pcm_open(&pcm_out_handle,
                          kCardName,
                          SND_PCM_STREAM_PLAYBACK,
                          0)) >= 0) {
    snd_pcm_close(pcm_out_handle);
  } else {
    if (num_connection_attempts_ == kConnectionAttemptToLogFailure)
      LOG(WARNING) << "PCM open failed: " << snd_strerror(err);
  }

  if ((err = snd_mixer_selem_register(handle, NULL, NULL)) < 0) {
    if (num_connection_attempts_ == kConnectionAttemptToLogFailure)
      LOG(WARNING) << "Mixer register error: " << snd_strerror(err);
    snd_mixer_close(handle);
    return false;
  }

  if ((err = snd_mixer_load(handle)) < 0) {
    if (num_connection_attempts_ == kConnectionAttemptToLogFailure)
      LOG(WARNING) << "Mixer " << kCardName << " load error: %s"
                   << snd_strerror(err);
    snd_mixer_close(handle);
    return false;
  }

  VLOG(1) << "Opened mixer " << kCardName << " successfully";

  double min_volume_db = kDefaultMinVolumeDb;
  double max_volume_db = kDefaultMaxVolumeDb;

  snd_mixer_elem_t* master_element = NULL;
  for (size_t i = 0; i < arraysize(kMasterElementNames); ++i) {
    master_element = FindElementWithName(handle, kMasterElementNames[i]);
    if (master_element)
      break;
  }

  if (!master_element) {
    if (num_connection_attempts_ == kConnectionAttemptToLogFailure)
      LOG(WARNING) << "Unable to find a master element on " << kCardName;
    snd_mixer_close(handle);
    return false;
  }

  alsa_long_t long_low = static_cast<alsa_long_t>(kDefaultMinVolumeDb * 100);
  alsa_long_t long_high = static_cast<alsa_long_t>(kDefaultMaxVolumeDb * 100);
  err = snd_mixer_selem_get_playback_dB_range(
      master_element, &long_low, &long_high);
  if (err != 0) {
    if (num_connection_attempts_ == kConnectionAttemptToLogFailure)
      LOG(WARNING) << "snd_mixer_selem_get_playback_dB_range() failed:"
                   << snd_strerror(err);
    snd_mixer_close(handle);
    return false;
  }
  min_volume_db = static_cast<double>(long_low) / 100.0;
  max_volume_db = static_cast<double>(long_high) / 100.0;

  snd_mixer_elem_t* pcm_element = FindElementWithName(handle, kPCMElementName);
  if (pcm_element) {
    alsa_long_t long_low = static_cast<alsa_long_t>(kDefaultMinVolumeDb * 100);
    alsa_long_t long_high = static_cast<alsa_long_t>(kDefaultMaxVolumeDb * 100);
    err = snd_mixer_selem_get_playback_dB_range(
        pcm_element, &long_low, &long_high);
    if (err != 0) {
      if (num_connection_attempts_ == kConnectionAttemptToLogFailure)
        LOG(WARNING) << "snd_mixer_selem_get_playback_dB_range() failed for "
                     << kPCMElementName << ": " << snd_strerror(err);
      snd_mixer_close(handle);
      return false;
    }
    min_volume_db += static_cast<double>(long_low) / 100.0;
    max_volume_db += static_cast<double>(long_high) / 100.0;
  }

  VLOG(1) << "Volume range is " << min_volume_db << " dB to "
          << max_volume_db << " dB";
  {
    base::AutoLock lock(lock_);
    alsa_mixer_ = handle;
    master_element_ = master_element;
    pcm_element_ = pcm_element;
    min_volume_db_ = min_volume_db;
    max_volume_db_ = max_volume_db;
    volume_db_ = min(max(volume_db_, min_volume_db_), max_volume_db_);
  }

  ApplyState();
  return true;
}

void AudioMixerAlsa::Disconnect() {
  DCHECK(MessageLoop::current() == thread_->message_loop());
  if (alsa_mixer_) {
    snd_mixer_close(alsa_mixer_);
    alsa_mixer_ = NULL;
  }
  disconnected_event_.Signal();
}

void AudioMixerAlsa::ApplyState() {
  DCHECK(MessageLoop::current() == thread_->message_loop());
  if (!alsa_mixer_)
    return;

  bool should_mute = false;
  double new_volume_db = 0;
  {
    base::AutoLock lock(lock_);
    should_mute = is_muted_;
    new_volume_db = should_mute ? min_volume_db_ : volume_db_;
    apply_is_pending_ = false;
  }

  if (pcm_element_) {
    // If a PCM volume slider exists, then first set the Master volume to the
    // nearest volume >= requested volume, then adjust PCM volume down to get
    // closer to the requested volume.
    SetElementVolume(master_element_, new_volume_db, 0.9999f);

    double pcm_volume_db = 0.0;
    double master_volume_db = 0.0;
    if (GetElementVolume(master_element_, &master_volume_db))
      pcm_volume_db = new_volume_db - master_volume_db;
    SetElementVolume(pcm_element_, pcm_volume_db, 0.5f);
  } else {
    SetElementVolume(master_element_, new_volume_db, 0.5f);
  }

  SetElementMuted(master_element_, should_mute);
}

snd_mixer_elem_t* AudioMixerAlsa::FindElementWithName(
    snd_mixer_t* handle, const string& element_name) const {
  DCHECK(MessageLoop::current() == thread_->message_loop());
  snd_mixer_selem_id_t* sid = NULL;

  // Using id_malloc/id_free API instead of id_alloca since the latter gives the
  // warning: the address of 'sid' will always evaluate as 'true'.
  if (snd_mixer_selem_id_malloc(&sid))
    return NULL;

  snd_mixer_selem_id_set_index(sid, 0);
  snd_mixer_selem_id_set_name(sid, element_name.c_str());
  snd_mixer_elem_t* element = snd_mixer_find_selem(handle, sid);
  if (!element)
    VLOG(1) << "Unable to find control " << snd_mixer_selem_id_get_name(sid);

  snd_mixer_selem_id_free(sid);
  return element;
}

bool AudioMixerAlsa::GetElementVolume(snd_mixer_elem_t* element,
                                      double* current_volume_db) {
  DCHECK(MessageLoop::current() == thread_->message_loop());
  alsa_long_t long_volume = 0;
  int alsa_result = snd_mixer_selem_get_playback_dB(
      element, static_cast<snd_mixer_selem_channel_id_t>(0), &long_volume);
  if (alsa_result != 0) {
    LOG(WARNING) << "snd_mixer_selem_get_playback_dB() failed: "
                 << snd_strerror(alsa_result);
    return false;
  }
  *current_volume_db = static_cast<double>(long_volume) / 100.0;
  return true;
}

bool AudioMixerAlsa::SetElementVolume(snd_mixer_elem_t* element,
                                      double new_volume_db,
                                      double rounding_bias) {
  DCHECK(MessageLoop::current() == thread_->message_loop());
  alsa_long_t volume_low = 0;
  alsa_long_t volume_high = 0;
  int alsa_result = snd_mixer_selem_get_playback_volume_range(
      element, &volume_low, &volume_high);
  if (alsa_result != 0) {
    LOG(WARNING) << "snd_mixer_selem_get_playback_volume_range() failed: "
                 << snd_strerror(alsa_result);
    return false;
  }
  alsa_long_t volume_range = volume_high - volume_low;
  if (volume_range <= 0)
    return false;

  alsa_long_t db_low_int = 0;
  alsa_long_t db_high_int = 0;
  alsa_result =
      snd_mixer_selem_get_playback_dB_range(element, &db_low_int, &db_high_int);
  if (alsa_result != 0) {
    LOG(WARNING) << "snd_mixer_selem_get_playback_dB_range() failed: "
                 << snd_strerror(alsa_result);
    return false;
  }

  double db_low = static_cast<double>(db_low_int) / 100.0;
  double db_high = static_cast<double>(db_high_int) / 100.0;
  double db_step = static_cast<double>(db_high - db_low) / volume_range;
  if (db_step <= 0.0)
    return false;

  if (new_volume_db < db_low)
    new_volume_db = db_low;

  alsa_long_t value = static_cast<alsa_long_t>(
      rounding_bias + (new_volume_db - db_low) / db_step) + volume_low;
  alsa_result = snd_mixer_selem_set_playback_volume_all(element, value);
  if (alsa_result != 0) {
    LOG(WARNING) << "snd_mixer_selem_set_playback_volume_all() failed: "
                 << snd_strerror(alsa_result);
    return false;
  }

  VLOG(1) << "Set volume " << snd_mixer_selem_get_name(element)
          << " to " << new_volume_db << " ==> "
          << (value - volume_low) * db_step + db_low << " dB";

  return true;
}

void AudioMixerAlsa::SetElementMuted(snd_mixer_elem_t* element, bool mute) {
  DCHECK(MessageLoop::current() == thread_->message_loop());
  int alsa_result = snd_mixer_selem_set_playback_switch_all(element, !mute);
  if (alsa_result != 0) {
    LOG(WARNING) << "snd_mixer_selem_set_playback_switch_all() failed: "
                 << snd_strerror(alsa_result);
  } else {
    VLOG(1) << "Set playback switch " << snd_mixer_selem_get_name(element)
            << " to " << mute;
  }
}

}  // namespace chromeos
