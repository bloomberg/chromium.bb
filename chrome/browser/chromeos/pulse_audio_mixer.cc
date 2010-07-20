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
// TODO(davej): Check if we need some thread safety mechanism (will someone be
// calling GetVolume while another process is calling SetVolume?)

namespace {

const int kInvalidDeviceId = -1;

// Used for passing custom data to the PulseAudio callbacks.
struct CallbackWrapper {
  pa_threaded_mainloop* mainloop;
  void* data;
};

}  // namespace

// AudioInfo contains all the values we care about when getting info for a
// Sink (output device) used by GetAudioInfo()
struct PulseAudioMixer::AudioInfo {
  pa_cvolume cvolume;
  bool muted;
};

PulseAudioMixer::PulseAudioMixer()
    : device_id_(kInvalidDeviceId),
      last_channels_(0),
      mixer_state_(UNINITIALIZED),
      pa_context_(NULL),
      pa_mainloop_(NULL),
      thread_(NULL) {
}

PulseAudioMixer::~PulseAudioMixer() {
  PulseAudioFree();
  thread_->Stop();
}

bool PulseAudioMixer::Init(InitDoneCallback* callback) {
  // Just start up worker thread, then post the task of starting up, which can
  // block for 200-500ms, so best not to do it on this thread.
  if (mixer_state_ != UNINITIALIZED)
    return false;

  mixer_state_ = INITIALIZING;
  if (thread_ == NULL) {
    thread_.reset(new base::Thread("PulseAudioMixer"));
    if (!thread_->Start()) {
      thread_.reset();
      return false;
    }
  }
  thread_->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &PulseAudioMixer::DoInit,
                        callback));
  return true;
}

double PulseAudioMixer::GetVolumeDb() const {
  if (!PulseAudioValid())
    return pa_sw_volume_to_dB(0);  // this returns -inf
  AudioInfo data;
  GetAudioInfo(&data);
  return pa_sw_volume_to_dB(data.cvolume.values[0]);
}

void PulseAudioMixer::GetVolumeDbAsync(GetVolumeCallback* callback,
                                       void* user) {
  if (!PulseAudioValid())
    return;
  thread_->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this,
                        &PulseAudioMixer::DoGetVolume,
                        callback, user));
}

void PulseAudioMixer::SetVolumeDb(double vol_db) {
  if (!PulseAudioValid())
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
  pa_threaded_mainloop_lock(pa_mainloop_);
  pa_op = pa_context_set_sink_volume_by_index(pa_context_, device_id_,
                                              &cvolume, NULL, NULL);
  pa_operation_unref(pa_op);
  pa_threaded_mainloop_unlock(pa_mainloop_);
}

bool PulseAudioMixer::IsMute() const {
  if (!PulseAudioValid())
    return false;
  AudioInfo data;
  GetAudioInfo(&data);
  return data.muted;
}

void PulseAudioMixer::SetMute(bool mute) {
  if (!PulseAudioValid())
    return;
  pa_operation* pa_op;
  pa_threaded_mainloop_lock(pa_mainloop_);
  pa_op = pa_context_set_sink_mute_by_index(pa_context_, device_id_,
                                            mute ? 1 : 0, NULL, NULL);
  pa_operation_unref(pa_op);
  pa_threaded_mainloop_unlock(pa_mainloop_);
}

bool PulseAudioMixer::IsValid() const {
  if (mixer_state_ == READY)
    return true;
  if (!pa_context_)
    return false;
  if (pa_context_get_state(pa_context_) != PA_CONTEXT_READY)
    return false;
  return true;
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

struct ConnectToPulseCallbackData {
  PulseAudioMixer* instance;
  bool connect_done;
};

// static
void PulseAudioMixer::ConnectToPulseCallbackThunk(
    pa_context* context, void* userdata) {
  ConnectToPulseCallbackData* data =
      static_cast<ConnectToPulseCallbackData*>(userdata);
  data->instance->OnConnectToPulseCallback(context, &data->connect_done);
}

void PulseAudioMixer::OnConnectToPulseCallback(
    pa_context* context, bool* connect_done) {
  pa_context_state_t state = pa_context_get_state(context);
  if (state == PA_CONTEXT_READY ||
      state == PA_CONTEXT_FAILED ||
      state == PA_CONTEXT_TERMINATED) {
    // Connection process has reached a terminal state. Wake PulseAudioInit().
    *connect_done = true;
    pa_threaded_mainloop_signal(pa_mainloop_, 0);
  }
}

bool PulseAudioMixer::PulseAudioInit() {
  pa_context_state_t state = PA_CONTEXT_FAILED;

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

    pa_threaded_mainloop_lock(pa_mainloop_);

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

      ConnectToPulseCallbackData data;
      data.instance = this;
      data.connect_done = false;

      pa_context_set_state_callback(pa_context_,
                                    &ConnectToPulseCallbackThunk,
                                    &data);

      if (pa_context_connect(pa_context_, NULL,
                             PA_CONTEXT_NOAUTOSPAWN, NULL) != 0) {
        LOG(ERROR) << "Can't start connection to PulseAudio sound server";
      } else {
        // Wait until we have a completed connection or fail.
        do {
          pa_threaded_mainloop_wait(pa_mainloop_);
        } while (!data.connect_done);

        state = pa_context_get_state(pa_context_);

        if (state == PA_CONTEXT_FAILED) {
          LOG(ERROR) << "PulseAudio context connection failed";
        } else if (state == PA_CONTEXT_TERMINATED) {
          LOG(ERROR) << "PulseAudio connection terminated early";
        } else if (state != PA_CONTEXT_READY) {
          LOG(ERROR) << "Unknown problem connecting to PulseAudio";
        }
      }

      pa_context_set_state_callback(pa_context_, NULL, NULL);
      break;
    }

    pa_threaded_mainloop_unlock(pa_mainloop_);

    if (state != PA_CONTEXT_READY)
      break;

    last_channels_ = 0;
    GetDefaultPlaybackDevice();
    mixer_state_ = READY;

    return true;
  }

  // Failed startup sequence, clean up now.
  PulseAudioFree();
  return false;
}

void PulseAudioMixer::PulseAudioFree() {
  if (!pa_mainloop_)
    return;

  DCHECK_NE(mixer_state_, UNINITIALIZED);
  mixer_state_ = SHUTTING_DOWN;

  if (pa_context_) {
    pa_threaded_mainloop_lock(pa_mainloop_);
    pa_context_disconnect(pa_context_);
    pa_context_unref(pa_context_);
    pa_threaded_mainloop_unlock(pa_mainloop_);
    pa_context_ = NULL;
  }

  pa_threaded_mainloop_stop(pa_mainloop_);
  pa_threaded_mainloop_free(pa_mainloop_);
  pa_mainloop_ = NULL;

  mixer_state_ = UNINITIALIZED;
}

bool PulseAudioMixer::PulseAudioValid() const {
  if (mixer_state_ != READY)
    return false;
  if (!pa_context_) {
    DLOG(ERROR) << "Trying to use PulseAudio when no context";
    return false;
  }
  if (pa_context_get_state(pa_context_) != PA_CONTEXT_READY) {
    LOG(ERROR) << "PulseAudio context not ready ("
               << pa_context_get_state(pa_context_) << ")";
    return false;
  }
  if (device_id_ == kInvalidDeviceId)
    return false;

  return true;
}

void PulseAudioMixer::CompleteOperationAndUnlock(pa_operation* pa_op) const {
  // After starting any operation, this helper checks if it started OK, then
  // waits for it to complete by iterating through the mainloop until the
  // operation is not running anymore.
  CHECK(pa_op);

  while (pa_operation_get_state(pa_op) == PA_OPERATION_RUNNING) {
    pa_threaded_mainloop_wait(pa_mainloop_);
  }
  pa_operation_unref(pa_op);
  pa_threaded_mainloop_unlock(pa_mainloop_);
}

void PulseAudioMixer::GetDefaultPlaybackDevice() {
  DCHECK(pa_context_);
  DCHECK(pa_context_get_state(pa_context_) == PA_CONTEXT_READY);

  pa_threaded_mainloop_lock(pa_mainloop_);
  pa_operation* pa_op = pa_context_get_sink_info_list(pa_context_,
                                        EnumerateDevicesCallback,
                                        this);
  CompleteOperationAndUnlock(pa_op);
  return;
}

void PulseAudioMixer::OnEnumerateDevices(const pa_sink_info* sink_info,
                                         int eol) {
  // If eol is set to a positive number, you're at the end of the list.
  if (eol > 0)
    return;

  // TODO(davej): Should we handle cases of more than one output sink device?
  if (device_id_ == kInvalidDeviceId)
    device_id_ = sink_info->index;

  pa_threaded_mainloop_signal(pa_mainloop_, 0);
}

// static
void PulseAudioMixer::EnumerateDevicesCallback(pa_context* unused,
                                               const pa_sink_info* sink_info,
                                               int eol,
                                               void* userdata) {
  PulseAudioMixer* inst = static_cast<PulseAudioMixer*>(userdata);
  inst->OnEnumerateDevices(sink_info, eol);
}

void PulseAudioMixer::GetAudioInfo(AudioInfo* info) const {
  CallbackWrapper cb_data = {pa_mainloop_, info};
  pa_threaded_mainloop_lock(pa_mainloop_);
  pa_operation* pa_op;
  pa_op = pa_context_get_sink_info_by_index(pa_context_,
                                            device_id_,
                                            GetAudioInfoCallback,
                                            &cb_data);
  CompleteOperationAndUnlock(pa_op);
}

// static
void PulseAudioMixer::GetAudioInfoCallback(pa_context* unused,
                                           const pa_sink_info* sink_info,
                                           int eol,
                                           void* userdata) {
  CallbackWrapper* cb_data = static_cast<CallbackWrapper*>(userdata);
  AudioInfo* data = static_cast<AudioInfo*>(cb_data->data);

  // Copy just the information we care about.
  if (eol == 0) {
    data->cvolume = sink_info->volume;
    data->muted = sink_info->mute ? true : false;
  }
  pa_threaded_mainloop_signal(cb_data->mainloop, 0);
}

}  // namespace chromeos

