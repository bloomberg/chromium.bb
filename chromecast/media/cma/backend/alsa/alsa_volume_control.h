// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ALSA_ALSA_VOLUME_CONTROL_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ALSA_ALSA_VOLUME_CONTROL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "media/audio/alsa/alsa_wrapper.h"

namespace chromecast {
namespace media {

// Handles setting the volume and mute state on the appropriate ALSA mixer
// elements (based on command-line args); also detects changes to the mute state
// or volume and informs a delegate.
// Must be created on an IO thread, and all methods must be called on that
// thread.
class AlsaVolumeControl : public base::MessageLoopForIO::Watcher {
 public:
  class Delegate {
   public:
    // Called whenever the ALSA volume or mute state have changed.
    // Unfortunately it is not possible in all cases to differentiate between
    // a volume change and a mute change, so the two events must be combined.
    virtual void OnAlsaVolumeOrMuteChange(float new_volume, bool new_mute) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  explicit AlsaVolumeControl(Delegate* delegate);
  ~AlsaVolumeControl() override;

  // Returns the value that you would get if you called GetVolume() after
  // SetVolume(volume).
  float VolumeThroughAlsa(float volume);

  // Returns the current ALSA volume (0 <= volume <= 1).
  float GetVolume();

  // Sets the ALSA volume to |level|; (0 <= level <= 1);
  void SetVolume(float level);

  // Returns |true| if ALSA is currently muted.
  bool IsMuted();

  // Sets the ALSA mute state to |muted|.
  void SetMuted(bool muted);

 private:
  class ScopedAlsaMixer;

  static std::string GetVolumeElementName();
  static std::string GetVolumeDeviceName();
  static std::string GetMuteElementName(::media::AlsaWrapper* alsa,
                                        const std::string& mixer_card_name,
                                        const std::string& mixer_element_name,
                                        const std::string& mute_card_name);
  static std::string GetMuteDeviceName();

  static int VolumeOrMuteChangeCallback(snd_mixer_elem_t* elem,
                                        unsigned int mask);

  void RefreshMixerFds(ScopedAlsaMixer* mixer);

  // base::MessageLoopForIO::Watcher implementation:
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  void OnVolumeOrMuteChanged();
  Delegate* const delegate_;

  const std::unique_ptr<::media::AlsaWrapper> alsa_;
  const std::string volume_mixer_device_name_;
  const std::string volume_mixer_element_name_;
  const std::string mute_mixer_device_name_;
  const std::string mute_mixer_element_name_;

  long volume_range_min_;  // NOLINT(runtime/int)
  long volume_range_max_;  // NOLINT(runtime/int)

  std::unique_ptr<ScopedAlsaMixer> volume_mixer_;
  std::unique_ptr<ScopedAlsaMixer> mute_mixer_;
  ScopedAlsaMixer* mute_mixer_ptr_;

  std::vector<std::unique_ptr<base::MessageLoopForIO::FileDescriptorWatcher>>
      file_descriptor_watchers_;

  DISALLOW_COPY_AND_ASSIGN(AlsaVolumeControl);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ALSA_ALSA_VOLUME_CONTROL_H_
