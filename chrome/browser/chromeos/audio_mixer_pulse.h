// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_AUDIO_MIXER_PULSE_H_
#define CHROME_BROWSER_CHROMEOS_AUDIO_MIXER_PULSE_H_
#pragma once

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "chrome/browser/chromeos/audio_mixer.h"

struct pa_context;
struct pa_cvolume;
struct pa_threaded_mainloop;
struct pa_operation;
struct pa_sink_info;

namespace chromeos {

class AudioMixerPulse : public AudioMixer {
 public:

  AudioMixerPulse();
  virtual ~AudioMixerPulse();

  // Implementation of AudioMixer
  virtual void Init(InitDoneCallback* callback);
  virtual bool InitSync();
  virtual double GetVolumeDb() const;
  virtual bool GetVolumeLimits(double* vol_min, double* vol_max);
  virtual void SetVolumeDb(double vol_db);
  virtual bool IsMute() const;
  virtual void SetMute(bool mute);
  virtual State GetState() const;

 private:
  struct AudioInfo;

  // The initialization task is run in the background on the worker thread.
  void DoInit(InitDoneCallback* callback);

  static void ConnectToPulseCallbackThunk(pa_context* c, void* userdata);
  void OnConnectToPulseCallback(pa_context* c, bool* connect_done);

  // Helper function to just get our messsage loop thread going.
  bool InitThread();

  // This goes through sequence of connecting to the default PulseAudio server.
  // We will block until we either have a valid connection or something failed.
  // If a connection is lost for some reason, delete and recreate the object.
  bool PulseAudioInit();

  // PulseAudioFree.  Disconnect from server.
  void PulseAudioFree();

  // Iterates the PA mainloop and only returns once an operation has completed
  // (successfully or unsuccessfully) or *done is true.
  void CompleteOperation(pa_operation* pa_op, bool* done) const;

  // For now, this just gets the first device returned from the enumeration
  // request.  This will be the 'default' or 'master' device that all further
  // actions are taken on.  Blocking call.
  void GetDefaultPlaybackDevice();
  static void EnumerateDevicesCallback(pa_context* unused,
                                       const pa_sink_info* sink_info,
                                       int eol,
                                       void* userdata);
  void OnEnumerateDevices(const pa_sink_info* sink_info, int eol, bool* done);

  // Get the info we're interested in from the default device. Currently this
  // is an array of volumes, and the mute state.  Blocking call.
  void GetAudioInfo(AudioInfo* info) const;
  static void GetAudioInfoCallback(pa_context* unused,
                                   const pa_sink_info* sink_info,
                                   int eol,
                                   void* userdata);

  // These call down to PulseAudio's mainloop locking functions
  void MainloopLock() const;
  void MainloopUnlock() const;
  void MainloopWait() const;
  void MainloopSignal() const;

  // Same as Lock(), but we fail if we are shutting down or mainloop invalid.
  bool MainloopSafeLock() const;

  // Lock the mainloop pa_lock_ if mixer_state_ is READY.
  bool MainloopLockIfReady() const;

  // The PulseAudio index of the main device being used.
  int device_id_;

  // Set to the number of channels on the main device.
  int last_channels_;

  // For informational purposes only, just used to assert lock is held.
  mutable int mainloop_lock_count_;

  mutable base::Lock mixer_state_lock_;
  mutable State mixer_state_;

  // Cached contexts for use in PulseAudio calls.
  pa_context* pa_context_;
  pa_threaded_mainloop* pa_mainloop_;

  scoped_ptr<base::Thread> thread_;

  DISALLOW_COPY_AND_ASSIGN(AudioMixerPulse);
};

}  // namespace chromeos

DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::AudioMixerPulse);

#endif  // CHROME_BROWSER_CHROMEOS_AUDIO_MIXER_PULSE_H_

