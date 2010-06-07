// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/pulse_audio_mixer.h"

#include <pulse/pulseaudio.h>

#include "base/logging.h"

namespace chromeos {

// TODO(davej): Do asynchronous versions by using the threaded PulseAudio API
// so gets, sets, and init sequence do not block calling thread.  GetVolume()
// and IsMute() may still need to be synchronous since caller is asking for the
// value specifically, which we will not know until the operation completes.
// So new asynchronous versions of the Get routines could be added with a
// callback parameter.  Currently, all Get, Set and Init calls are blocking on
// PulseAudio.
//
// Set calls can just return without waiting.  When a set operation completes,
// we could proxy to the UI thread to notify there was a volume change update
// so the new volume can be displayed to the user.
// TODO(davej): Serialize volume/mute to preserve settings when restarting?
// TODO(davej): Check if we need some thread safety mechanism (will someone be
// calling GetVolume while another process is calling SetVolume?)

namespace {

const int kInvalidDeviceId = -1;

}  // namespace

// AudioInfo contains all the values we care about when getting info for a
// Sink (output device) used by GetAudioInfo()
struct PulseAudioMixer::AudioInfo {
  pa_cvolume cvolume;
  bool muted;
};

// This class will set volume using PulseAudio to adjust volume and mute.

PulseAudioMixer::PulseAudioMixer()
    : device_id_(kInvalidDeviceId),
      pa_mainloop_(NULL),
      pa_context_(NULL),
      connect_started_(false),
      last_channels_(0) {
}

PulseAudioMixer::~PulseAudioMixer() {
  PulseAudioFree();
}

bool PulseAudioMixer::Init() {
  // Find main device for changing 'master' default volume.
  if (!PulseAudioInit())
    return false;
  device_id_ = GetDefaultPlaybackDevice();
  last_channels_ = 0;
  LOG(INFO) << "PulseAudioMixer initialized OK";
  return true;
}

double PulseAudioMixer::GetVolumeDb() const {
  AudioInfo data;
  if (!GetAudioInfo(&data))
    return pa_sw_volume_to_dB(0);  // this returns -inf
  return pa_sw_volume_to_dB(data.cvolume.values[0]);
}

void PulseAudioMixer::SetVolumeDb(double vol_db) {
  if (!PulseAudioValid()) {
    DLOG(ERROR) << "Called SetVolume when mixer not valid";
    return;
  }

  // last_channels_ determines the number of channels on the main output device,
  // and is used later to set the volume on all channels at once.
  if (!last_channels_) {
    AudioInfo data;
    if (!GetAudioInfo(&data))
      return;
    last_channels_ = data.cvolume.channels;
  }

  pa_operation* pa_op;
  pa_cvolume cvolume;
  pa_cvolume_set(&cvolume, last_channels_, pa_sw_volume_from_dB(vol_db));
  pa_op = pa_context_set_sink_volume_by_index(pa_context_, device_id_,
                                              &cvolume, NULL, NULL);
  CompleteOperationHelper(pa_op);
}

bool PulseAudioMixer::IsMute() const {
  AudioInfo data;
  if (!GetAudioInfo(&data))
    return false;

  return data.muted;
}

void PulseAudioMixer::SetMute(bool mute) {
  if (!PulseAudioValid()) {
    DLOG(ERROR) << "Called SetMute when mixer not valid";
    return;
  }

  pa_operation* pa_op;
  pa_op = pa_context_set_sink_mute_by_index(pa_context_, device_id_,
                                            mute ? 1 : 0, NULL, NULL);
  CompleteOperationHelper(pa_op);
}

void PulseAudioMixer::ToggleMute() {
  SetMute(!IsMute());
}

bool PulseAudioMixer::IsValid() const {
  if (device_id_ == kInvalidDeviceId)
    return false;
  if (!pa_context_)
    return false;
  if (pa_context_get_state(pa_context_) != PA_CONTEXT_READY)
    return false;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// Private functions that deal with PulseAudio directly

bool PulseAudioMixer::PulseAudioInit() {
  pa_mainloop_api* pa_mlapi;
  pa_context_state_t state;

  // It's OK to call Init again to re-initialize.  This could be useful if
  // !IsValid() and we want to try reconnecting.
  if (pa_mainloop_)
    PulseAudioFree();

  while (true) {
    // Create connection to default server.
    pa_mainloop_ = pa_mainloop_new();
    if (!pa_mainloop_) {
      LOG(ERROR) << "Can't create PulseAudio mainloop";
      break;
    }
    pa_mlapi = pa_mainloop_get_api(pa_mainloop_);
    if (!pa_mlapi) {
      LOG(ERROR) << "Can't get PulseAudio mainloop api";
      break;
    }
    pa_context_ = pa_context_new(pa_mlapi, "ChromeAudio");
    if (!pa_context_) {
      LOG(ERROR) << "Can't create new PulseAudio context";
      break;
    }

    // Using simpler method of just checking state after each iterate.
    // Connect to PulseAudio sound server.
    if (pa_context_connect(pa_context_, NULL,
                           PA_CONTEXT_NOAUTOSPAWN, NULL) != 0) {
      LOG(ERROR) << "Can't start connection to PulseAudio sound server";
      break;
    }

    connect_started_ = true;

    // Wait until we have a completed connection or fail.
    // TODO(davej): Remove blocking waits during init as well.
    do {
      pa_mainloop_iterate(pa_mainloop_, 1, NULL);  // blocking wait
      state = pa_context_get_state(pa_context_);
      if (state == PA_CONTEXT_FAILED) {
        LOG(ERROR) << "PulseAudio context connection failed";
        break;
      }
      if (state == PA_CONTEXT_TERMINATED) {
        LOG(ERROR) << "PulseAudio connection terminated early";
        break;
      }
    } while (state != PA_CONTEXT_READY);

    if (state != PA_CONTEXT_READY)
      break;

    return true;
  }

  // Failed startup sequence, clean up now.
  PulseAudioFree();

  return false;
}

void PulseAudioMixer::PulseAudioFree() {
  if (pa_context_) {
    if (connect_started_)
      pa_context_disconnect(pa_context_);
    pa_context_unref(pa_context_);
    pa_context_ = NULL;
  }
  connect_started_ = false;
  if (pa_mainloop_) {
    pa_mainloop_free(pa_mainloop_);
    pa_mainloop_ = NULL;
  }
}

bool PulseAudioMixer::PulseAudioValid() const {
  if (!pa_context_) {
    DLOG(ERROR) << "Trying to use PulseAudio when no context";
    return false;
  }

  if (pa_context_get_state(pa_context_) != PA_CONTEXT_READY) {
    LOG(ERROR) << "PulseAudio context not ready";
    return false;
  }

  if (device_id_ == kInvalidDeviceId) {
    DLOG(ERROR) << "Trying to use PulseAudio when no device id";
    return false;
  }
  return true;
}

bool PulseAudioMixer::CompleteOperationHelper(pa_operation* pa_op) {
  // After starting any operation, this helper checks if it started OK, then
  // waits for it to complete by iterating through the mainloop until the
  // operation is not running anymore.
  if (!pa_op) {
    LOG(ERROR) << "Failed to start operation";
    return false;
  }
  while (pa_operation_get_state(pa_op) == PA_OPERATION_RUNNING) {
    pa_mainloop_iterate(pa_mainloop_, 1, NULL);
  }
  pa_operation_unref(pa_op);
  return true;
}

int PulseAudioMixer::GetDefaultPlaybackDevice() {
  int device = kInvalidDeviceId;
  pa_operation* pa_op;

  if (!pa_context_) {
    DLOG(ERROR) << "Trying to use PulseAudio when no context";
    return kInvalidDeviceId;
  }

  if (pa_context_get_state(pa_context_) != PA_CONTEXT_READY) {
    LOG(ERROR) << "PulseAudio context not ready";
    return kInvalidDeviceId;
  }

  pa_op = pa_context_get_sink_info_list(pa_context_,
                                        EnumerateDevicesCallback,
                                        &device);
  CompleteOperationHelper(pa_op);
  return device;
}

// static
void PulseAudioMixer::EnumerateDevicesCallback(pa_context* unused,
                                               const pa_sink_info* sink_info,
                                               int eol,
                                               void* userdata) {
  int* pa_device = static_cast<int*>(userdata);

  // If eol is set to a positive number, you're at the end of the list.
  if (eol > 0)
    return;

  // TODO(davej): Should we handle cases of more than one output sink device?
  if (*pa_device == kInvalidDeviceId)
    *pa_device = sink_info->index;
}

bool PulseAudioMixer::GetAudioInfo(AudioInfo* info) const {
  DCHECK(info);
  if (!PulseAudioValid()) {
    DLOG(ERROR) << "Called GetAudioInfo when mixer not valid";
    return false;
  }
  if (!info)
    return false;

  pa_operation* pa_op;
  pa_op = pa_context_get_sink_info_by_index(pa_context_,
                                            device_id_,
                                            GetAudioInfoCallback,
                                            info);

  // Duplicating some code in CompleteOperationHelper because this function
  // needs to stay 'const', and this isn't allowed if that is called.
  if (!pa_op) {
    LOG(ERROR) << "Failed to start operation";
    return false;
  }
  while (pa_operation_get_state(pa_op) == PA_OPERATION_RUNNING) {
    pa_mainloop_iterate(pa_mainloop_, 1, NULL);
  }
  pa_operation_unref(pa_op);
  return true;
}

// static
void PulseAudioMixer::GetAudioInfoCallback(pa_context* unused,
                                           const pa_sink_info* sink_info,
                                           int eol,
                                           void* userdata) {
  if (!userdata) {
    DLOG(ERROR) << "userdata NULL";
    return;
  }
  AudioInfo* data = static_cast<AudioInfo*>(userdata);

  // Copy just the information we care about.
  if (eol == 0) {
    data->cvolume = sink_info->volume;
    data->muted = sink_info->mute ? true : false;
  }
}

}  // namespace chromeos

