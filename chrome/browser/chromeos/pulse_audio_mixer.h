// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PULSE_AUDIO_MIXER_H_
#define CHROME_BROWSER_CHROMEOS_PULSE_AUDIO_MIXER_H_

#include "base/basictypes.h"

struct pa_context;
struct pa_cvolume;
struct pa_mainloop;
struct pa_operation;
struct pa_sink_info;

namespace chromeos {

class PulseAudioMixer {
 public:
  PulseAudioMixer();
  ~PulseAudioMixer();

  // These public functions all return true if the operation completed
  // successfully, or false if not set up to talk with the default audio device.

  // Connect to PulseAudio and find a default device.  Blocking call.
  // TODO(davej): Remove blocking waits in PA.
  bool Init();

  // Blocking call. Returns a default of -inf on error.
  double GetVolumeDb() const;

  // Blocking call.
  // TODO(davej): Remove blocking waits in PA.
  void SetVolumeDb(double vol_db);

  // Gets the mute state of the default device (true == mute). Blocking call.
  // Returns a default of false on error.
  bool IsMute() const;

  // Blocking call.
  // TODO(davej): Remove blocking waits in PA.
  void SetMute(bool mute);

  // Blocking call.
  // TODO(davej): Remove blocking waits in PA.
  void ToggleMute();

  // Call any time to see if we have a valid working connection to PulseAudio.
  // Non-blocking call.
  bool IsValid() const;

 private:
  struct AudioInfo;

  // This goes through sequence of connecting to the default PulseAudio server.
  // We will block until we either have a valid connection or something failed.
  // It's safe to call again, in case a connection is lost for some reason
  // (e.g. if other functions fail, or IsValid() is false).
  // TODO(davej): Remove blocking waits in PA.
  bool PulseAudioInit();

  // PulseAudioFree.  Disconnect from server.
  void PulseAudioFree();

  // Check if the PA system is ready for communication, as well as if a default
  // device is available to talk to.  This can return false if we lose the
  // connection, even after an original successful init.
  bool PulseAudioValid() const;

  // Iterates the PA mainloop and only returns once an operation has completed
  // (successfully or unsuccessfully).  This blocking call is needed for
  // synchronous getting of data.
  // TODO(davej): This function will go away after doing asynchronous versions
  // so we don't have to block when not necessary.
  bool CompleteOperationHelper(pa_operation* pa_op);

  // For now, this just gets the first device returned from the enumeration
  // request.  This will be the 'default' or 'master' device that all further
  // actions are taken on.  Blocking call.
  int GetDefaultPlaybackDevice();
  static void EnumerateDevicesCallback(pa_context* unused,
                                       const pa_sink_info* sink_info,
                                       int eol,
                                       void* userdata);

  // Get the info we're interested in from the default device. Currently this
  // is an array of volumes, and the mute state.  Blocking call.
  bool GetAudioInfo(AudioInfo* info) const;
  static void GetAudioInfoCallback(pa_context* unused,
                                   const pa_sink_info* sink_info,
                                   int eol,
                                   void* userdata);

  // The PulseAudio index of the main device being used.
  int device_id_;

  // Cached contexts for use in PulseAudio calls.
  pa_mainloop* pa_mainloop_;
  pa_context* pa_context_;

  // Set if context connect succeeded so we can safely disconnect later.
  bool connect_started_;

  // Set to the number of channels on the main device.
  int last_channels_;

  DISALLOW_COPY_AND_ASSIGN(PulseAudioMixer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PULSE_AUDIO_MIXER_H_

