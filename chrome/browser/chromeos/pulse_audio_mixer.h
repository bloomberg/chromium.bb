// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PULSE_AUDIO_MIXER_H_
#define CHROME_BROWSER_CHROMEOS_PULSE_AUDIO_MIXER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/scoped_ptr.h"
#include "base/thread.h"

struct pa_context;
struct pa_cvolume;
struct pa_threaded_mainloop;
struct pa_operation;
struct pa_sink_info;

namespace chromeos {

class PulseAudioMixer {
 public:
  PulseAudioMixer();
  ~PulseAudioMixer();

  // Non-blocking, connect to PulseAudio and find a default device, and call
  // callback when complete with success code.
  typedef Callback1<bool>::Type InitDoneCallback;
  bool Init(InitDoneCallback* callback);

  // Blocking call. Returns a default of -inf on error.
  double GetVolumeDb() const;

  // Non-blocking, volume sent in as first param to callback
  typedef Callback2<double, void*>::Type GetVolumeCallback;
  void GetVolumeDbAsync(GetVolumeCallback* callback, void* user);

  // Non-blocking call.
  void SetVolumeDb(double vol_db);

  // Gets the mute state of the default device (true == mute). Blocking call.
  // Returns a default of false on error.
  bool IsMute() const;

  // Non-Blocking call.
  void SetMute(bool mute);

  // Call any time to see if we have a valid working connection to PulseAudio.
  // Non-blocking call.
  bool IsValid() const;

 private:
  struct AudioInfo;

  enum State {
    UNINITIALIZED = 0,
    INITIALIZING,
    READY,
    SHUTTING_DOWN
  };

  // These are the tasks to be run in the background on the worker thread.
  void DoInit(InitDoneCallback* callback);
  void DoGetVolume(GetVolumeCallback* callback, void* user);

  static void ConnectToPulseCallbackThunk(pa_context* c, void* userdata);
  void OnConnectToPulseCallback(pa_context* c, bool* connect_done);

  // This goes through sequence of connecting to the default PulseAudio server.
  // We will block until we either have a valid connection or something failed.
  // If a connection is lost for some reason, delete and recreate the object.
  bool PulseAudioInit();

  // PulseAudioFree.  Disconnect from server.
  void PulseAudioFree();

  // Check if the PA system is ready for communication, as well as if a default
  // device is available to talk to.  This can return false if we lose the
  // connection, even after an original successful init.
  bool PulseAudioValid() const;

  // Iterates the PA mainloop and only returns once an operation has completed
  // (successfully or unsuccessfully).  This call only blocks the worker thread.
  void CompleteOperationAndUnlock(pa_operation* pa_op) const;

  // For now, this just gets the first device returned from the enumeration
  // request.  This will be the 'default' or 'master' device that all further
  // actions are taken on.  Blocking call.
  void GetDefaultPlaybackDevice();
  static void EnumerateDevicesCallback(pa_context* unused,
                                       const pa_sink_info* sink_info,
                                       int eol,
                                       void* userdata);
  void OnEnumerateDevices(const pa_sink_info* sink_info, int eol);

  // Get the info we're interested in from the default device. Currently this
  // is an array of volumes, and the mute state.  Blocking call.
  void GetAudioInfo(AudioInfo* info) const;
  static void GetAudioInfoCallback(pa_context* unused,
                                   const pa_sink_info* sink_info,
                                   int eol,
                                   void* userdata);

  // The PulseAudio index of the main device being used.
  mutable int device_id_;

  // Set to the number of channels on the main device.
  int last_channels_;
  State mixer_state_;

  // Cached contexts for use in PulseAudio calls.
  pa_context* pa_context_;
  pa_threaded_mainloop* pa_mainloop_;

  scoped_ptr<base::Thread> thread_;

  DISALLOW_COPY_AND_ASSIGN(PulseAudioMixer);
};

}  // namespace chromeos

DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::PulseAudioMixer);

#endif  // CHROME_BROWSER_CHROMEOS_PULSE_AUDIO_MIXER_H_

