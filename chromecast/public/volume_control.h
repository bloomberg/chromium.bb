// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_VOLUME_CONTROL_H_
#define CHROMECAST_PUBLIC_VOLUME_CONTROL_H_

#include <string>
#include <vector>

#include "chromecast_export.h"

namespace chromecast {
namespace media {

// Audio content types for volume control. Each content type has a separate
// volume and mute state.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chromecast.media
enum class AudioContentType {
  kMedia,          // Normal audio playback; also used for system sound effects.
  kAlarm,          // Alarm sounds.
  kCommunication,  // Voice communication, eg assistant TTS.
};

// Observer for volume/mute state changes. This is useful to detect volume
// changes that occur outside of cast_shell. Add/RemoveVolumeObserver() must not
// be called synchronously from OnVolumeChange() or OnMuteChange().
class VolumeObserver {
 public:
  // Called whenever the volume changes for a given stream |type|. May be called
  // on an arbitrary thread.
  virtual void OnVolumeChange(AudioContentType type, float new_volume) = 0;

  // Called whenever the mute state changes for a given stream |type|. May be
  // called on an arbitrary thread.
  virtual void OnMuteChange(AudioContentType type, bool new_muted) = 0;

 protected:
  virtual ~VolumeObserver() = default;
};

// Volume control is initialized once when cast_shell starts up, and finalized
// on shutdown. Revoking resources has no effect on volume control. All volume
// control methods are called on the same thread that calls Initialize().
class CHROMECAST_EXPORT VolumeControl {
 public:
  // Initializes platform-specific volume control. Only called when volume
  // control is in an uninitialized state. The implementation of this method
  // should load previously set volume and mute states from persistent storage,
  // so that the volume and mute are preserved across reboots.
  static void Initialize(const std::vector<std::string>& argv)
      __attribute__((__weak__));

  // Tears down platform-specific volume control and returns to the
  // uninitialized state.
  static void Finalize() __attribute__((__weak__));

  // Adds a volume observer.
  static void AddVolumeObserver(VolumeObserver* observer)
      __attribute__((__weak__));
  // Removes a volume observer. After this is called, the implementation must
  // not call any more methods on the observer.
  static void RemoveVolumeObserver(VolumeObserver* observer)
      __attribute__((__weak__));

  // Gets/sets the output volume for a given audio stream |type|. The volume
  // |level| is in the range [0.0, 1.0].
  static float GetVolume(AudioContentType type) __attribute__((__weak__));
  static void SetVolume(AudioContentType type, float level)
      __attribute__((__weak__));

  // Gets/sets the mute state for a given audio stream |type|.
  static bool IsMuted(AudioContentType type) __attribute__((__weak__));
  static void SetMuted(AudioContentType type, bool muted)
      __attribute__((__weak__));

  // Limits the output volume for a given stream |type| to no more than |limit|.
  // This does not affect the logical volume for the stream type; the volume
  // returned by GetVolume() should not change, and no OnVolumeChange() event
  // should be sent to observers.
  static void SetOutputLimit(AudioContentType type, float limit)
      __attribute__((__weak__));

  // Converts a volume level in the range [0.0, 1.0] to/from a volume in dB.
  // The volume in dB should be full-scale (so a volume level of 1.0 would be
  // 0.0 dBFS, and any lower volume level would be negative).
  // May be called from multiple processes.
  static float VolumeToDbFS(float volume) __attribute__((__weak__));
  static float DbFSToVolume(float dbfs) __attribute__((__weak__));
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_VOLUME_CONTROL_H_
