// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/audio_mixer_pulse.h"

#include <pulse/pulseaudio.h>

#include "base/logging.h"
#include "base/task.h"

namespace chromeos {

// Using asynchronous versions of the threaded PulseAudio API, as well
// as a worker thread so gets, sets, and the init sequence do not block the
// calling thread.  GetVolume() and IsMute() can still be called synchronously
// if needed, but take a bit longer (~2ms vs ~0.3ms).
//
// Set calls just return without waiting.  If you must guarantee the value has
// been set before continuing, immediately call the blocking Get version to
// synchronously get the value back.
//
// TODO(davej): Serialize volume/mute to preserve settings when restarting?

namespace {

const int kInvalidDeviceId = -1;

const double kMinVolumeDb = -90.0;
// Choosing 6.0dB here instead of 0dB to give user chance to amplify audio some
// in case sounds or their setup is too quiet for them.
const double kMaxVolumeDb = 6.0;

// Used for passing custom data to the PulseAudio callbacks.
struct CallbackWrapper {
  AudioMixerPulse* instance;
  bool done;
  void* userdata;
};

}  // namespace

// AudioInfo contains all the values we care about when getting info for a
// Sink (output device) used by GetAudioInfo().
struct AudioMixerPulse::AudioInfo {
  pa_cvolume cvolume;
  bool muted;
};

AudioMixerPulse::AudioMixerPulse()
    : device_id_(kInvalidDeviceId),
      last_channels_(0),
      mainloop_lock_count_(0),
      mixer_state_(UNINITIALIZED),
      pa_context_(NULL),
      pa_mainloop_(NULL) {
}

AudioMixerPulse::~AudioMixerPulse() {
  PulseAudioFree();
  if (thread_ != NULL) {
    thread_->Stop();
    thread_.reset();
  }
}

void AudioMixerPulse::Init(InitDoneCallback* callback) {
  DCHECK(callback);
  if (!InitThread()) {
    callback->Run(false);
    delete callback;
    return;
  }

  // Post the task of starting up, which can block for 200-500ms,
  // so best not to do it on the caller's thread.
  thread_->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &AudioMixerPulse::DoInit, callback));
}

bool AudioMixerPulse::InitSync() {
  if (!InitThread())
    return false;
  return PulseAudioInit();
}

double AudioMixerPulse::GetVolumeDb() const {
  if (!MainloopLockIfReady())
    return AudioMixer::kSilenceDb;
  AudioInfo data;
  GetAudioInfo(&data);
  MainloopUnlock();
  return pa_sw_volume_to_dB(data.cvolume.values[0]);
}

bool AudioMixerPulse::GetVolumeLimits(double* vol_min, double* vol_max) {
  if (vol_min)
    *vol_min = kMinVolumeDb;
  if (vol_max)
    *vol_max = kMaxVolumeDb;
  return true;
}

void AudioMixerPulse::SetVolumeDb(double vol_db) {
  if (!MainloopLockIfReady())
    return;

  // last_channels_ determines the number of channels on the main output device,
  // and is used later to set the volume on all channels at once.
  if (!last_channels_) {
    AudioInfo data;
    GetAudioInfo(&data);
    last_channels_ = data.cvolume.channels;
  }

  pa_operation* pa_op;
  pa_cvolume cvolume;
  pa_cvolume_set(&cvolume, last_channels_, pa_sw_volume_from_dB(vol_db));
  pa_op = pa_context_set_sink_volume_by_index(pa_context_, device_id_,
                                              &cvolume, NULL, NULL);
  pa_operation_unref(pa_op);
  MainloopUnlock();
  VLOG(1) << "Set volume to " << vol_db << " dB";
}

bool AudioMixerPulse::IsMute() const {
  if (!MainloopLockIfReady())
    return false;
  AudioInfo data;
  GetAudioInfo(&data);
  MainloopUnlock();
  return data.muted;
}

void AudioMixerPulse::SetMute(bool mute) {
  if (!MainloopLockIfReady())
    return;
  pa_operation* pa_op;
  pa_op = pa_context_set_sink_mute_by_index(pa_context_, device_id_,
                                            mute ? 1 : 0, NULL, NULL);
  pa_operation_unref(pa_op);
  MainloopUnlock();
  VLOG(1) << "Set mute to " << mute;
}

AudioMixer::State AudioMixerPulse::GetState() const {
  base::AutoLock lock(mixer_state_lock_);
  // If we think it's ready, verify it is actually so.
  if ((mixer_state_ == READY) &&
      (pa_context_get_state(pa_context_) != PA_CONTEXT_READY))
    mixer_state_ = IN_ERROR;
  return mixer_state_;
}

////////////////////////////////////////////////////////////////////////////////
// Private functions follow

void AudioMixerPulse::DoInit(InitDoneCallback* callback) {
  bool success = PulseAudioInit();
  callback->Run(success);
  delete callback;
}

bool AudioMixerPulse::InitThread() {
  base::AutoLock lock(mixer_state_lock_);

  if (mixer_state_ != UNINITIALIZED)
    return false;

  if (thread_ == NULL) {
    thread_.reset(new base::Thread("AudioMixerPulse"));
    if (!thread_->Start()) {
      thread_.reset();
      return false;
    }
  }
  mixer_state_ = INITIALIZING;
  return true;
}

// static
void AudioMixerPulse::ConnectToPulseCallbackThunk(
    pa_context* context, void* userdata) {
  CallbackWrapper* data =
      static_cast<CallbackWrapper*>(userdata);
  data->instance->OnConnectToPulseCallback(context, &data->done);
}

void AudioMixerPulse::OnConnectToPulseCallback(
    pa_context* context, bool* connect_done) {
  pa_context_state_t state = pa_context_get_state(context);
  if (state == PA_CONTEXT_READY ||
      state == PA_CONTEXT_FAILED ||
      state == PA_CONTEXT_TERMINATED) {
    // Connection process has reached a terminal state. Wake PulseAudioInit().
    *connect_done = true;
    MainloopSignal();
  }
}

bool AudioMixerPulse::PulseAudioInit() {
  pa_context_state_t state = PA_CONTEXT_FAILED;

  {
    base::AutoLock lock(mixer_state_lock_);
    if (mixer_state_ != INITIALIZING)
      return false;

    pa_mainloop_ = pa_threaded_mainloop_new();
    if (!pa_mainloop_) {
      LOG(ERROR) << "Can't create PulseAudio mainloop";
      mixer_state_ = UNINITIALIZED;
      return false;
    }

    if (pa_threaded_mainloop_start(pa_mainloop_) != 0) {
      LOG(ERROR) << "Can't start PulseAudio mainloop";
      pa_threaded_mainloop_free(pa_mainloop_);
      mixer_state_ = UNINITIALIZED;
      return false;
    }
  }

  while (true) {
    // Create connection to default server.
    if (!MainloopSafeLock())
      return false;

    while (true) {
      pa_mainloop_api* pa_mlapi = pa_threaded_mainloop_get_api(pa_mainloop_);
      if (!pa_mlapi) {
        LOG(ERROR) << "Can't get PulseAudio mainloop api";
        break;
      }
      // This one takes the most time if run at app startup.
      pa_context_ = pa_context_new(pa_mlapi, "ChromeAudio");
      if (!pa_context_) {
        LOG(ERROR) << "Can't create new PulseAudio context";
        break;
      }

      MainloopUnlock();
      if (!MainloopSafeLock())
        return false;

      CallbackWrapper data = {this, false, NULL};
      pa_context_set_state_callback(pa_context_,
                                    &ConnectToPulseCallbackThunk,
                                    &data);

      if (pa_context_connect(pa_context_, NULL,
                             PA_CONTEXT_NOAUTOSPAWN, NULL) != 0) {
        LOG(ERROR) << "Can't start connection to PulseAudio sound server";
      } else {
        // Wait until we have a completed connection or fail.
        do {
          MainloopWait();
        } while (!data.done);

        state = pa_context_get_state(pa_context_);

        if (state == PA_CONTEXT_FAILED) {
          LOG(ERROR) << "PulseAudio connection failed (daemon not running?)";
        } else if (state == PA_CONTEXT_TERMINATED) {
          LOG(ERROR) << "PulseAudio connection terminated early";
        } else if (state != PA_CONTEXT_READY) {
          LOG(ERROR) << "Unknown problem connecting to PulseAudio";
        }
      }

      pa_context_set_state_callback(pa_context_, NULL, NULL);
      break;
    }

    MainloopUnlock();

    if (state != PA_CONTEXT_READY)
      break;

    if (!MainloopSafeLock())
      return false;
    GetDefaultPlaybackDevice();
    MainloopUnlock();

    if (device_id_ == kInvalidDeviceId)
      break;

    {
      base::AutoLock lock(mixer_state_lock_);
      if (mixer_state_ == SHUTTING_DOWN)
        return false;
      mixer_state_ = READY;
    }

    return true;
  }

  // Failed startup sequence, clean up now.
  PulseAudioFree();
  return false;
}

void AudioMixerPulse::PulseAudioFree() {
  {
    base::AutoLock lock(mixer_state_lock_);
    if (!pa_mainloop_)
      mixer_state_ = UNINITIALIZED;
    if ((mixer_state_ == UNINITIALIZED) || (mixer_state_ == SHUTTING_DOWN))
      return;

    // If still initializing on another thread, this will cause it to exit.
    mixer_state_ = SHUTTING_DOWN;
  }

  DCHECK(pa_mainloop_);

  MainloopLock();
  if (pa_context_) {
    pa_context_disconnect(pa_context_);
    pa_context_unref(pa_context_);
    pa_context_ = NULL;
  }
  MainloopUnlock();

  pa_threaded_mainloop_stop(pa_mainloop_);
  pa_threaded_mainloop_free(pa_mainloop_);
  pa_mainloop_ = NULL;

  {
    base::AutoLock lock(mixer_state_lock_);
    mixer_state_ = UNINITIALIZED;
  }
}

void AudioMixerPulse::CompleteOperation(pa_operation* pa_op,
                                        bool* done) const {
  // After starting any operation, this helper checks if it started OK, then
  // waits for it to complete by iterating through the mainloop until the
  // operation is not running anymore.
  CHECK(pa_op);

  while (pa_operation_get_state(pa_op) == PA_OPERATION_RUNNING) {
    // If operation still running, but we got what we needed, cancel it now.
    if (*done) {
      pa_operation_cancel(pa_op);
      break;
    }
    MainloopWait();
  }
  pa_operation_unref(pa_op);
}

// Must be called with mainloop lock held
void AudioMixerPulse::GetDefaultPlaybackDevice() {
  DCHECK_GT(mainloop_lock_count_, 0);
  DCHECK(pa_context_);
  DCHECK(pa_context_get_state(pa_context_) == PA_CONTEXT_READY);

  CallbackWrapper data = {this, false, NULL};

  pa_operation* pa_op = pa_context_get_sink_info_list(pa_context_,
                                        EnumerateDevicesCallback,
                                        &data);
  CompleteOperation(pa_op, &data.done);
  return;
}

void AudioMixerPulse::OnEnumerateDevices(const pa_sink_info* sink_info,
                                         int eol, bool* done) {
  if (device_id_ != kInvalidDeviceId)
    return;

  // TODO(davej): Should we handle cases of more than one output sink device?

  // eol is < 0 for error, > 0 for end of list, ==0 while listing.
  if (eol == 0) {
    device_id_ = sink_info->index;
  }
  *done = true;
  MainloopSignal();
}

// static
void AudioMixerPulse::EnumerateDevicesCallback(pa_context* unused,
                                               const pa_sink_info* sink_info,
                                               int eol,
                                               void* userdata) {
  CallbackWrapper* data =
      static_cast<CallbackWrapper*>(userdata);
  data->instance->OnEnumerateDevices(sink_info, eol, &data->done);
}

// Must be called with lock held
void AudioMixerPulse::GetAudioInfo(AudioInfo* info) const {
  DCHECK_GT(mainloop_lock_count_, 0);
  CallbackWrapper data = {const_cast<AudioMixerPulse*>(this), false, info};
  pa_operation* pa_op = pa_context_get_sink_info_by_index(pa_context_,
                                                          device_id_,
                                                          GetAudioInfoCallback,
                                                          &data);
  CompleteOperation(pa_op, &data.done);
}

// static
void AudioMixerPulse::GetAudioInfoCallback(pa_context* unused,
                                           const pa_sink_info* sink_info,
                                           int eol,
                                           void* userdata) {
  CallbackWrapper* data = static_cast<CallbackWrapper*>(userdata);
  AudioInfo* info = static_cast<AudioInfo*>(data->userdata);

  // Copy just the information we care about.
  if (eol == 0) {
    info->cvolume = sink_info->volume;
    info->muted = sink_info->mute ? true : false;
    data->done = true;
  }
  data->instance->MainloopSignal();
}

inline void AudioMixerPulse::MainloopLock() const {
  pa_threaded_mainloop_lock(pa_mainloop_);
  ++mainloop_lock_count_;
}

inline void AudioMixerPulse::MainloopUnlock() const {
  --mainloop_lock_count_;
  pa_threaded_mainloop_unlock(pa_mainloop_);
}

// Must be called with the lock held.
inline void AudioMixerPulse::MainloopWait() const {
  DCHECK_GT(mainloop_lock_count_, 0);
  pa_threaded_mainloop_wait(pa_mainloop_);
}

// Must be called with the lock held.
inline void AudioMixerPulse::MainloopSignal() const {
  DCHECK_GT(mainloop_lock_count_, 0);
  pa_threaded_mainloop_signal(pa_mainloop_, 0);
}

inline bool AudioMixerPulse::MainloopSafeLock() const {
  base::AutoLock lock(mixer_state_lock_);
  if ((mixer_state_ == SHUTTING_DOWN) || (!pa_mainloop_))
    return false;

  pa_threaded_mainloop_lock(pa_mainloop_);
  ++mainloop_lock_count_;
  return true;
}

inline bool AudioMixerPulse::MainloopLockIfReady() const {
  base::AutoLock lock(mixer_state_lock_);
  if (mixer_state_ != READY)
    return false;
  if (!pa_mainloop_)
    return false;
  pa_threaded_mainloop_lock(pa_mainloop_);
  ++mainloop_lock_count_;
  return true;
}

}  // namespace chromeos

