// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/audio_mixer_alsa.h"

#include <cmath>

#include <alsa/asoundlib.h>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"

namespace chromeos {

// Connect to the ALSA mixer using their simple element API.  Init is performed
// asynchronously on the worker thread.
//
// To get a wider range and finer control over volume levels, first the Master
// level is set, then if the PCM element exists, the total level is refined by
// adjusting that as well.  If the PCM element has more volume steps, it allows
// for finer granularity in the total volume.

typedef long alsa_long_t;  // 'long' is required for ALSA API calls.

namespace {

const char kMasterVolume[] = "Master";
const char kPCMVolume[] = "PCM";
const double kDefaultMinVolume = -90.0;
const double kDefaultMaxVolume = 0.0;
const double kPrefVolumeInvalid = -999.0;
const int kPrefMuteOff = 0;
const int kPrefMuteOn = 1;
const int kPrefMuteInvalid = 2;

}  // namespace

AudioMixerAlsa::AudioMixerAlsa()
    : min_volume_(kDefaultMinVolume),
      max_volume_(kDefaultMaxVolume),
      save_volume_(0),
      mixer_state_(UNINITIALIZED),
      alsa_mixer_(NULL),
      elem_master_(NULL),
      elem_pcm_(NULL),
      prefs_(NULL),
      done_event_(true, false) {
}

AudioMixerAlsa::~AudioMixerAlsa() {
  if (thread_ != NULL) {
    {
      base::AutoLock lock(mixer_state_lock_);
      mixer_state_ = SHUTTING_DOWN;
      thread_->message_loop()->PostTask(FROM_HERE,
          NewRunnableMethod(this, &AudioMixerAlsa::FreeAlsaMixer));
    }
    done_event_.Wait();

    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    // A ScopedAllowIO object is required to join the thread when calling Stop.
    // The worker thread should be idle at this time.
    // See http://crosbug.com/11110 for discussion.
    base::ThreadRestrictions::ScopedAllowIO allow_io_for_thread_join;
    thread_->message_loop()->AssertIdle();

    thread_->Stop();
    thread_.reset();
  }
}

void AudioMixerAlsa::Init(InitDoneCallback* callback) {
  DCHECK(callback);
  if (!InitThread()) {
    callback->Run(false);
    delete callback;
    return;
  }
  InitPrefs();

  {
    base::AutoLock lock(mixer_state_lock_);
    if (mixer_state_ == SHUTTING_DOWN)
      return;

    // Post the task of starting up, which may block on the order of ms,
    // so best not to do it on the caller's thread.
    thread_->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &AudioMixerAlsa::DoInit, callback));
  }
}

bool AudioMixerAlsa::InitSync() {
  if (!InitThread())
    return false;
  InitPrefs();
  return InitializeAlsaMixer();
}

double AudioMixerAlsa::GetVolumeDb() const {
  base::AutoLock lock(mixer_state_lock_);
  if (mixer_state_ != READY)
    return kSilenceDb;

  return DoGetVolumeDb_Locked();
}

bool AudioMixerAlsa::GetVolumeLimits(double* vol_min, double* vol_max) {
  base::AutoLock lock(mixer_state_lock_);
  if (mixer_state_ != READY)
    return false;
  if (vol_min)
    *vol_min = min_volume_;
  if (vol_max)
    *vol_max = max_volume_;
  return true;
}

void AudioMixerAlsa::SetVolumeDb(double vol_db) {
  base::AutoLock lock(mixer_state_lock_);
  if (mixer_state_ != READY)
    return;

  if (vol_db < kSilenceDb || isnan(vol_db)) {
    if (isnan(vol_db))
      LOG(WARNING) << "Got request to set volume to NaN";
    vol_db = kSilenceDb;
  }

  DoSetVolumeDb_Locked(vol_db);
  prefs_->SetDouble(prefs::kAudioVolume, vol_db);
}

bool AudioMixerAlsa::IsMute() const {
  base::AutoLock lock(mixer_state_lock_);
  if (mixer_state_ != READY)
    return false;
  return GetElementMuted_Locked(elem_master_);
}

// To indicate the volume is not valid yet, a very low volume value is stored.
// We compare against a slightly higher value in case of rounding errors.
static bool PrefVolumeValid(double volume) {
  return (volume > kPrefVolumeInvalid + 0.1);
}

void AudioMixerAlsa::SetMute(bool mute) {
  base::AutoLock lock(mixer_state_lock_);
  if (mixer_state_ != READY)
    return;

  // Set volume to minimum on mute, since switching the element off does not
  // always mute as it should.

  // TODO(davej): Remove save_volume_ and setting volume to minimum if
  // switching the element off can be guaranteed to mute it.  Currently mute
  // is done by setting the volume to min_volume_.

  bool old_value = GetElementMuted_Locked(elem_master_);

  if (old_value != mute) {
    if (mute) {
      save_volume_ = DoGetVolumeDb_Locked();
      DoSetVolumeDb_Locked(min_volume_);
    } else {
      DoSetVolumeDb_Locked(save_volume_);
    }
  }

  SetElementMuted_Locked(elem_master_, mute);
  prefs_->SetInteger(prefs::kAudioMute, mute ? kPrefMuteOn : kPrefMuteOff);
}

AudioMixer::State AudioMixerAlsa::GetState() const {
  base::AutoLock lock(mixer_state_lock_);
  // If we think it's ready, verify it is actually so.
  if ((mixer_state_ == READY) && (alsa_mixer_ == NULL))
    mixer_state_ = IN_ERROR;
  return mixer_state_;
}

// static
void AudioMixerAlsa::RegisterPrefs(PrefService* local_state) {
  if (!local_state->FindPreference(prefs::kAudioVolume))
    local_state->RegisterDoublePref(prefs::kAudioVolume, kPrefVolumeInvalid);
  if (!local_state->FindPreference(prefs::kAudioMute))
    local_state->RegisterIntegerPref(prefs::kAudioMute, kPrefMuteInvalid);
}

////////////////////////////////////////////////////////////////////////////////
// Private functions follow

void AudioMixerAlsa::DoInit(InitDoneCallback* callback) {
  bool success = InitializeAlsaMixer();

  if (success) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this, &AudioMixerAlsa::RestoreVolumeMuteOnUIThread));
  }

  if (callback) {
    callback->Run(success);
    delete callback;
  }
}

bool AudioMixerAlsa::InitThread() {
  base::AutoLock lock(mixer_state_lock_);

  if (mixer_state_ != UNINITIALIZED)
    return false;

  if (thread_ == NULL) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    thread_.reset(new base::Thread("AudioMixerAlsa"));
    if (!thread_->Start()) {
      thread_.reset();
      return false;
    }
  }

  mixer_state_ = INITIALIZING;
  return true;
}

void AudioMixerAlsa::InitPrefs() {
  prefs_ = g_browser_process->local_state();
}

bool AudioMixerAlsa::InitializeAlsaMixer() {
  base::AutoLock lock(mixer_state_lock_);
  if (mixer_state_ != INITIALIZING)
    return false;

  int err;
  snd_mixer_t* handle = NULL;
  const char* card = "default";

  if ((err = snd_mixer_open(&handle, 0)) < 0) {
    LOG(ERROR) << "ALSA mixer " << card << " open error: " << snd_strerror(err);
    return false;
  }

  if ((err = snd_mixer_attach(handle, card)) < 0) {
    LOG(ERROR) << "ALSA Attach to card " << card << " failed: "
               << snd_strerror(err);
    snd_mixer_close(handle);
    return false;
  }

  // Verify PCM can be opened, which also instantiates the PCM mixer element
  // which is needed for finer volume control and for muting by setting to zero.
  // If it fails, we can still try to use the mixer as best we can.
  snd_pcm_t* pcm_out_handle;
  if ((err = snd_pcm_open(&pcm_out_handle,
                          card,
                          SND_PCM_STREAM_PLAYBACK,
                          0)) >= 0) {
    snd_pcm_close(pcm_out_handle);
  } else {
    LOG(WARNING) << "ALSA PCM open: " << snd_strerror(err);
  }

  if ((err = snd_mixer_selem_register(handle, NULL, NULL)) < 0) {
    LOG(ERROR) << "ALSA mixer register error: " << snd_strerror(err);
    snd_mixer_close(handle);
    return false;
  }

  if ((err = snd_mixer_load(handle)) < 0) {
    LOG(ERROR) << "ALSA mixer " << card << " load error: %s"
               << snd_strerror(err);
    snd_mixer_close(handle);
    return false;
  }

  VLOG(1) << "Opened ALSA mixer " << card << " OK";

  elem_master_ = FindElementWithName_Locked(handle, kMasterVolume);
  if (elem_master_) {
    alsa_long_t long_lo = static_cast<alsa_long_t>(kDefaultMinVolume * 100);
    alsa_long_t long_hi = static_cast<alsa_long_t>(kDefaultMaxVolume * 100);
    err = snd_mixer_selem_get_playback_dB_range(
        elem_master_, &long_lo, &long_hi);
    if (err != 0) {
      LOG(WARNING) << "snd_mixer_selem_get_playback_dB_range() failed "
                   << "for master: " << snd_strerror(err);
      snd_mixer_close(handle);
      return false;
    }
    min_volume_ = static_cast<double>(long_lo) / 100.0;
    max_volume_ = static_cast<double>(long_hi) / 100.0;
  } else {
    LOG(ERROR) << "Cannot find 'Master' ALSA mixer element on " << card;
    snd_mixer_close(handle);
    return false;
  }

  elem_pcm_ = FindElementWithName_Locked(handle, kPCMVolume);
  if (elem_pcm_) {
    alsa_long_t long_lo = static_cast<alsa_long_t>(kDefaultMinVolume * 100);
    alsa_long_t long_hi = static_cast<alsa_long_t>(kDefaultMaxVolume * 100);
    err = snd_mixer_selem_get_playback_dB_range(elem_pcm_, &long_lo, &long_hi);
    if (err != 0) {
      LOG(WARNING) << "snd_mixer_selem_get_playback_dB_range() failed for PCM: "
                   << snd_strerror(err);
      snd_mixer_close(handle);
      return false;
    }
    min_volume_ += static_cast<double>(long_lo) / 100.0;
    max_volume_ += static_cast<double>(long_hi) / 100.0;
  }

  VLOG(1) << "ALSA volume range is " << min_volume_ << " dB to "
          << max_volume_ << " dB";

  alsa_mixer_ = handle;
  mixer_state_ = READY;
  return true;
}

void AudioMixerAlsa::FreeAlsaMixer() {
  if (alsa_mixer_) {
    snd_mixer_close(alsa_mixer_);
    alsa_mixer_ = NULL;
  }
  done_event_.Signal();
}

void AudioMixerAlsa::DoSetVolumeMute(double pref_volume, int pref_mute) {
  base::AutoLock lock(mixer_state_lock_);
  if (mixer_state_ != READY)
    return;

  // If volume or mute are invalid, set them now to the current actual values.
  if (!PrefVolumeValid(pref_volume))
    pref_volume = DoGetVolumeDb_Locked();
  bool mute = false;
  if (pref_mute == kPrefMuteInvalid)
    mute = GetElementMuted_Locked(elem_master_);
  else
    mute = (pref_mute == kPrefMuteOn) ? true : false;

  VLOG(1) << "Setting volume to " << pref_volume << " and mute to " << mute;

  if (mute) {
    save_volume_ = pref_volume;
    DoSetVolumeDb_Locked(min_volume_);
  } else {
    DoSetVolumeDb_Locked(pref_volume);
  }

  SetElementMuted_Locked(elem_master_, mute);
}

void AudioMixerAlsa::RestoreVolumeMuteOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // This happens during init, so set the volume off the UI thread.
  int mute = prefs_->GetInteger(prefs::kAudioMute);
  double vol = prefs_->GetDouble(prefs::kAudioVolume);
  {
    base::AutoLock lock(mixer_state_lock_);
    if (mixer_state_ == SHUTTING_DOWN)
      return;
    thread_->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &AudioMixerAlsa::DoSetVolumeMute, vol, mute));
  }
}

double AudioMixerAlsa::DoGetVolumeDb_Locked() const {
  double vol_total = 0.0;
  if (!GetElementVolume_Locked(elem_master_, &vol_total))
    return 0.0;

  double vol_pcm = 0.0;
  if (elem_pcm_ && GetElementVolume_Locked(elem_pcm_, &vol_pcm))
    vol_total += vol_pcm;

  return vol_total;
}

void AudioMixerAlsa::DoSetVolumeDb_Locked(double vol_db) {
  double actual_vol = 0.0;

  // If a PCM volume slider exists, then first set the Master volume to the
  // nearest volume >= requested volume, then adjust PCM volume down to get
  // closer to the requested volume.
  if (elem_pcm_) {
    SetElementVolume_Locked(elem_master_, vol_db, &actual_vol, 0.9999f);
    SetElementVolume_Locked(elem_pcm_, vol_db - actual_vol, NULL, 0.5f);
  } else {
    SetElementVolume_Locked(elem_master_, vol_db, NULL, 0.5f);
  }
}

snd_mixer_elem_t* AudioMixerAlsa::FindElementWithName_Locked(
    snd_mixer_t* handle,
    const char* element_name) const {
  snd_mixer_selem_id_t* sid;

  // Using id_malloc/id_free API instead of id_alloca since the latter gives the
  // warning: the address of 'sid' will always evaluate as 'true'.
  if (snd_mixer_selem_id_malloc(&sid))
    return NULL;

  snd_mixer_selem_id_set_index(sid, 0);
  snd_mixer_selem_id_set_name(sid, element_name);
  snd_mixer_elem_t* elem = snd_mixer_find_selem(handle, sid);
  if (!elem) {
    LOG(ERROR) << "ALSA unable to find simple control "
               << snd_mixer_selem_id_get_name(sid);
  }

  snd_mixer_selem_id_free(sid);
  return elem;
}

bool AudioMixerAlsa::GetElementVolume_Locked(snd_mixer_elem_t* elem,
                                             double* current_vol) const {
  alsa_long_t long_vol = 0;
  int alsa_result = snd_mixer_selem_get_playback_dB(
      elem, static_cast<snd_mixer_selem_channel_id_t>(0), &long_vol);
  if (alsa_result != 0) {
    LOG(WARNING) << "snd_mixer_selem_get_playback_dB() failed: "
                 << snd_strerror(alsa_result);
    return false;
  }

  *current_vol = static_cast<double>(long_vol) / 100.0;
  return true;
}

bool AudioMixerAlsa::SetElementVolume_Locked(snd_mixer_elem_t* elem,
                                             double new_vol,
                                             double* actual_vol,
                                             double rounding_bias) {
  alsa_long_t vol_lo = 0;
  alsa_long_t vol_hi = 0;
  int alsa_result =
      snd_mixer_selem_get_playback_volume_range(elem, &vol_lo, &vol_hi);
  if (alsa_result != 0) {
    LOG(WARNING) << "snd_mixer_selem_get_playback_volume_range() failed: "
                 << snd_strerror(alsa_result);
    return false;
  }
  alsa_long_t vol_range = vol_hi - vol_lo;
  if (vol_range <= 0)
    return false;

  alsa_long_t db_lo_int = 0;
  alsa_long_t db_hi_int = 0;
  alsa_result =
      snd_mixer_selem_get_playback_dB_range(elem, &db_lo_int, &db_hi_int);
  if (alsa_result != 0) {
    LOG(WARNING) << "snd_mixer_selem_get_playback_dB_range() failed: "
                 << snd_strerror(alsa_result);
    return false;
  }

  double db_lo = static_cast<double>(db_lo_int) / 100.0;
  double db_hi = static_cast<double>(db_hi_int) / 100.0;
  double db_step = static_cast<double>(db_hi - db_lo) / vol_range;
  if (db_step <= 0.0)
    return false;

  if (new_vol < db_lo)
    new_vol = db_lo;

  alsa_long_t value = static_cast<alsa_long_t>(rounding_bias +
      (new_vol - db_lo) / db_step) + vol_lo;
  alsa_result = snd_mixer_selem_set_playback_volume_all(elem, value);
  if (alsa_result != 0) {
    LOG(WARNING) << "snd_mixer_selem_set_playback_volume_all() failed: "
                 << snd_strerror(alsa_result);
    return false;
  }

  VLOG(1) << "Set volume " << snd_mixer_selem_get_name(elem)
          << " to " << new_vol << " ==> " << (value - vol_lo) * db_step + db_lo
          << " dB";

  if (actual_vol) {
    alsa_long_t volume = vol_lo;
    alsa_result = snd_mixer_selem_get_playback_volume(
        elem, static_cast<snd_mixer_selem_channel_id_t>(0), &volume);
    if (alsa_result != 0) {
      LOG(WARNING) << "snd_mixer_selem_get_playback_volume() failed: "
                   << snd_strerror(alsa_result);
      return false;
    }
    *actual_vol = db_lo + (volume - vol_lo) * db_step;

    VLOG(1) << "Actual volume " << snd_mixer_selem_get_name(elem)
            << " now " << *actual_vol << " dB";
  }
  return true;
}

bool AudioMixerAlsa::GetElementMuted_Locked(snd_mixer_elem_t* elem) const {
  int enabled = 0;
  int alsa_result = snd_mixer_selem_get_playback_switch(
      elem, static_cast<snd_mixer_selem_channel_id_t>(0), &enabled);
  if (alsa_result != 0) {
    LOG(WARNING) << "snd_mixer_selem_get_playback_switch() failed: "
                 << snd_strerror(alsa_result);
    return false;
  }
  return (enabled) ? false : true;
}

void AudioMixerAlsa::SetElementMuted_Locked(snd_mixer_elem_t* elem, bool mute) {
  int enabled = mute ? 0 : 1;
  int alsa_result = snd_mixer_selem_set_playback_switch_all(elem, enabled);
  if (alsa_result != 0) {
    LOG(WARNING) << "snd_mixer_selem_set_playback_switch_all() failed: "
                 << snd_strerror(alsa_result);
  } else {
    VLOG(1) << "Set playback switch " << snd_mixer_selem_get_name(elem)
            << " to " << enabled;
  }
}

}  // namespace chromeos
