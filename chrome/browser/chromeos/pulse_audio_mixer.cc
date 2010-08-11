// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/pulse_audio_mixer.h"

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

// Used for passing custom data to the PulseAudio callbacks.
struct CallbackWrapper {
  PulseAudioMixer* instance;
  bool done;
  void* userdata;
};

}  // namespace

// AudioInfo contains all the values we care about when getting info for a
// Sink (output device) used by GetAudioInfo().
struct PulseAudioMixer::AudioInfo {
  pa_cvolume cvolume;
  bool muted;
};

PulseAudioMixer::PulseAudioMixer()
    : device_id_(kInvalidDeviceId),
      last_channels_(0),
      mainloop_lock_count_(0),
      mixer_state_lock_(),
      mixer_state_(UNINITIALIZED),
      pa_context_(NULL),
      pa_mainloop_(NULL),
      thread_(NULL) {
}

PulseAudioMixer::~PulseAudioMixer() {
  PulseAudioFree();
  thread_->Stop();
  thread_.reset();
}

bool PulseAudioMixer::Init(InitDoneCallback* callback) {
  if (!InitThread())
    return false;

  // Post the task of starting up, which can block for 200-500ms,
  // so best not to do it on the caller's thread.
  thread_->message_loop()->PostTask(FROM_HERE,
    NewRunnableMethod(this, &PulseAudioMixer::DoInit, callback));
  return true;
}

bool PulseAudioMixer::InitSync() {
  if (!InitThread())
    return false;
  return PulseAudioInit();
}

double PulseAudioMixer::GetVolumeDb() const {
  if (!MainloopLockIfReady())
    return pa_sw_volume_to_dB(0);  // this returns -inf.
  AudioInfo data;
  GetAudioInfo(&data);
  MainloopUnlock();
  return pa_sw_volume_to_dB(data.cvolume.values[0]);
}

bool PulseAudioMixer::GetVolumeDbAsync(GetVolumeCallback* callback,
                                       void* user) {
  if (CheckState() != READY)
    return false;
  thread_->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this,
                        &PulseAudioMixer::DoGetVolume,
                        callback, user));
  return true;
}

void PulseAudioMixer::SetVolumeDb(double vol_db) {
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
}

bool PulseAudioMixer::IsMute() const {
  if (!MainloopLockIfReady())
    return false;
  AudioInfo data;
  GetAudioInfo(&data);
  MainloopUnlock();
  return data.muted;
}

void PulseAudioMixer::SetMute(bool mute) {
  if (!MainloopLockIfReady())
    return;
  pa_operation* pa_op;
  pa_op = pa_context_set_sink_mute_by_index(pa_context_, device_id_,
                                            mute ? 1 : 0, NULL, NULL);
  pa_operation_unref(pa_op);
  MainloopUnlock();
}

PulseAudioMixer::State PulseAudioMixer::CheckState() const {
  AutoLock lock(mixer_state_lock_);
  // If we think it's ready, verify it is actually so.
  if ((mixer_state_ == READY) &&
      (pa_context_get_state(pa_context_) != PA_CONTEXT_READY))
    mixer_state_ = IN_ERROR;
  return mixer_state_;
}

////////////////////////////////////////////////////////////////////////////////
// Private functions follow

void PulseAudioMixer::DoInit(InitDoneCallback* callback) {
  bool success = PulseAudioInit();
  callback->Run(success);
  delete callback;
}

void PulseAudioMixer::DoGetVolume(GetVolumeCallback* callback,
                                  void* user) {
  callback->Run(GetVolumeDb(), user);
  delete callback;
}

bool PulseAudioMixer::InitThread() {
  if (thread_ == NULL) {
    thread_.reset(new base::Thread("PulseAudioMixer"));
    if (!thread_->Start()) {
      thread_.reset();
      return false;
    }
  }
  return true;
}

// static
void PulseAudioMixer::ConnectToPulseCallbackThunk(
    pa_context* context, void* userdata) {
  CallbackWrapper* data =
      static_cast<CallbackWrapper*>(userdata);
  data->instance->OnConnectToPulseCallback(context, &data->done);
}

void PulseAudioMixer::OnConnectToPulseCallback(
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

bool PulseAudioMixer::PulseAudioInit() {
  pa_context_state_t state = PA_CONTEXT_FAILED;

  {
    AutoLock lock(mixer_state_lock_);
    if (mixer_state_ != UNINITIALIZED)
      return false;
    mixer_state_ = INITIALIZING;
  }

  while (true) {
    // Create connection to default server.
    pa_mainloop_ = pa_threaded_mainloop_new();
    if (!pa_mainloop_) {
      LOG(ERROR) << "Can't create PulseAudio mainloop";
      break;
    }

    if (pa_threaded_mainloop_start(pa_mainloop_) != 0) {
      LOG(ERROR) << "Can't start PulseAudio mainloop";
      break;
    }

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

    set_mixer_state(READY);
    return true;
  }

  // Failed startup sequence, clean up now.
  PulseAudioFree();
  return false;
}

void PulseAudioMixer::PulseAudioFree() {
  if (!pa_mainloop_)
    return;

  {
    AutoLock lock(mixer_state_lock_);
    DCHECK_NE(mixer_state_, UNINITIALIZED);
    if (mixer_state_ == SHUTTING_DOWN)
      return;
    // If still initializing on another thread, this will cause it to exit.
    mixer_state_ = SHUTTING_DOWN;
  }

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

  set_mixer_state(UNINITIALIZED);
}

void PulseAudioMixer::CompleteOperation(pa_operation* pa_op,
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
void PulseAudioMixer::GetDefaultPlaybackDevice() {
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

void PulseAudioMixer::OnEnumerateDevices(const pa_sink_info* sink_info,
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
void PulseAudioMixer::EnumerateDevicesCallback(pa_context* unused,
                                               const pa_sink_info* sink_info,
                                               int eol,
                                               void* userdata) {
  CallbackWrapper* data =
      static_cast<CallbackWrapper*>(userdata);
  data->instance->OnEnumerateDevices(sink_info, eol, &data->done);
}

// Must be called with lock held
void PulseAudioMixer::GetAudioInfo(AudioInfo* info) const {
  DCHECK_GT(mainloop_lock_count_, 0);
  CallbackWrapper data = {const_cast<PulseAudioMixer*>(this), false, info};
  pa_operation* pa_op = pa_context_get_sink_info_by_index(pa_context_,
                                                          device_id_,
                                                          GetAudioInfoCallback,
                                                          &data);
  CompleteOperation(pa_op, &data.done);
}

// static
void PulseAudioMixer::GetAudioInfoCallback(pa_context* unused,
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

inline void PulseAudioMixer::MainloopLock() const {
  pa_threaded_mainloop_lock(pa_mainloop_);
  ++mainloop_lock_count_;
}

inline void PulseAudioMixer::MainloopUnlock() const {
  --mainloop_lock_count_;
  pa_threaded_mainloop_unlock(pa_mainloop_);
}

// Must be called with the lock held.
inline void PulseAudioMixer::MainloopWait() const {
  DCHECK_GT(mainloop_lock_count_, 0);
  pa_threaded_mainloop_wait(pa_mainloop_);
}

// Must be called with the lock held.
inline void PulseAudioMixer::MainloopSignal() const {
  DCHECK_GT(mainloop_lock_count_, 0);
  pa_threaded_mainloop_signal(pa_mainloop_, 0);
}

inline bool PulseAudioMixer::MainloopSafeLock() const {
  AutoLock lock(mixer_state_lock_);
  if ((mixer_state_ == SHUTTING_DOWN) || (!pa_mainloop_))
    return false;
  pa_threaded_mainloop_lock(pa_mainloop_);
  ++mainloop_lock_count_;
  return true;
}

inline bool PulseAudioMixer::MainloopLockIfReady() const {
  AutoLock lock(mixer_state_lock_);
  if (mixer_state_ != READY)
    return false;
  if (!pa_mainloop_)
    return false;
  pa_threaded_mainloop_lock(pa_mainloop_);
  ++mainloop_lock_count_;
  return true;
}

}  // namespace chromeos

