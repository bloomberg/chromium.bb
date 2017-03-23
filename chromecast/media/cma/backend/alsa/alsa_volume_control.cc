// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/alsa_volume_control.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chromecast/base/chromecast_switches.h"
#include "media/base/media_switches.h"

#define ALSA_ASSERT(func, ...)                                        \
  do {                                                                \
    int err = alsa_->func(__VA_ARGS__);                               \
    LOG_ASSERT(err >= 0) << #func " error: " << alsa_->StrError(err); \
  } while (0)

namespace chromecast {
namespace media {

namespace {

const char kAlsaDefaultDeviceName[] = "default";
const char kAlsaDefaultVolumeElementName[] = "Master";
const char kAlsaMuteMixerElementName[] = "Mute";

std::string GetSwitchValue(const std::string& name,
                           const std::string& deprecated_name) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(name)
             ? command_line->GetSwitchValueASCII(name)
             : command_line->GetSwitchValueASCII(deprecated_name);
}

}  // namespace

class AlsaVolumeControl::ScopedAlsaMixer {
 public:
  ScopedAlsaMixer(::media::AlsaWrapper* alsa,
                  const std::string& mixer_device_name,
                  const std::string& mixer_element_name)
      : alsa_(alsa) {
    DCHECK(alsa_);
    VLOG(1) << "Opening mixer element \"" << mixer_element_name
            << "\" on device \"" << mixer_device_name << "\"";
    ALSA_ASSERT(MixerOpen, &mixer, 0);
    ALSA_ASSERT(MixerAttach, mixer, mixer_device_name.c_str());
    ALSA_ASSERT(MixerElementRegister, mixer, NULL, NULL);
    ALSA_ASSERT(MixerLoad, mixer);

    snd_mixer_selem_id_t* sid = NULL;
    ALSA_ASSERT(MixerSelemIdMalloc, &sid);
    alsa_->MixerSelemIdSetIndex(sid, 0);
    alsa_->MixerSelemIdSetName(sid, mixer_element_name.c_str());
    element = alsa_->MixerFindSelem(mixer, sid);
    if (!element) {
      LOG(ERROR) << "Simple mixer control element \"" << mixer_element_name
                 << "\" not found.";
    }
    alsa_->MixerSelemIdFree(sid);
  }

  ~ScopedAlsaMixer() { alsa_->MixerClose(mixer); }

  snd_mixer_elem_t* element;
  snd_mixer_t* mixer;

 private:
  ::media::AlsaWrapper* const alsa_;
};

// static
std::string AlsaVolumeControl::GetVolumeElementName() {
  std::string mixer_element_name =
      GetSwitchValue(switches::kAlsaVolumeElementName,
                     switches::kDeprecatedAlsaVolumeElementName);
  if (mixer_element_name.empty()) {
    mixer_element_name = kAlsaDefaultVolumeElementName;
  }
  return mixer_element_name;
}

// static
std::string AlsaVolumeControl::GetVolumeDeviceName() {
  std::string mixer_device_name =
      GetSwitchValue(switches::kAlsaVolumeDeviceName,
                     switches::kDeprecatedAlsaVolumeDeviceName);
  if (!mixer_device_name.empty()) {
    return mixer_device_name;
  }

  // If the output device was overridden, then the mixer should default to
  // that device.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  mixer_device_name =
      command_line->GetSwitchValueASCII(switches::kAlsaOutputDevice);
  if (!mixer_device_name.empty()) {
    return mixer_device_name;
  }
  return kAlsaDefaultDeviceName;
}

// Mixers that are implemented with ALSA's softvol plugin don't have mute
// switches available. This function allows ALSA-based AvSettings to fall back
// on another mixer which solely implements mute for the system.
// static
std::string AlsaVolumeControl::GetMuteElementName(
    ::media::AlsaWrapper* alsa,
    const std::string& mixer_device_name,
    const std::string& mixer_element_name,
    const std::string& mute_device_name) {
  DCHECK(alsa);
  std::string mute_element_name = GetSwitchValue(
      switches::kAlsaMuteElementName, switches::kDeprecatedAlsaMuteElementName);
  if (!mute_element_name.empty()) {
    return mute_element_name;
  }

  ScopedAlsaMixer mixer(alsa, mixer_device_name, mixer_element_name);
  DCHECK(mixer.element);
  if (alsa->MixerSelemHasPlaybackSwitch(mixer.element)) {
    return mixer_element_name;
  }

  ScopedAlsaMixer mute(alsa, mute_device_name, kAlsaMuteMixerElementName);
  if (!mute.element) {
    LOG(WARNING) << "The default ALSA mixer does not have a playback switch "
                    "and a fallback mute element was not found, "
                    "mute will not work.";
    return mixer_element_name;
  }
  if (alsa->MixerSelemHasPlaybackSwitch(mute.element)) {
    return kAlsaMuteMixerElementName;
  }

  LOG(WARNING) << "The default ALSA mixer does not have a playback switch "
                  "and the fallback mute element does not have a playback "
                  "switch, mute will not work.";
  return mixer_element_name;
}

// static
std::string AlsaVolumeControl::GetMuteDeviceName() {
  std::string mute_device_name = GetSwitchValue(
      switches::kAlsaMuteDeviceName, switches::kDeprecatedAlsaMuteDeviceName);
  if (!mute_device_name.empty()) {
    return mute_device_name;
  }

  // If the mute mixer device was not specified directly, use the same device as
  // the volume mixer.
  return GetVolumeDeviceName();
}

AlsaVolumeControl::AlsaVolumeControl(Delegate* delegate)
    : delegate_(delegate),
      alsa_(base::MakeUnique<::media::AlsaWrapper>()),
      volume_mixer_device_name_(GetVolumeDeviceName()),
      volume_mixer_element_name_(GetVolumeElementName()),
      mute_mixer_device_name_(GetMuteDeviceName()),
      mute_mixer_element_name_(GetMuteElementName(alsa_.get(),
                                                  volume_mixer_device_name_,
                                                  volume_mixer_element_name_,
                                                  mute_mixer_device_name_)),
      volume_range_min_(0),
      volume_range_max_(0),
      mute_mixer_ptr_(nullptr) {
  DCHECK(delegate_);
  VLOG(1) << "Volume device = " << volume_mixer_device_name_
          << ", element = " << volume_mixer_element_name_;
  VLOG(1) << "Mute device = " << mute_mixer_device_name_
          << ", element = " << mute_mixer_element_name_;

  volume_mixer_ = base::MakeUnique<ScopedAlsaMixer>(
      alsa_.get(), volume_mixer_device_name_, volume_mixer_element_name_);
  DCHECK(volume_mixer_->element);
  ALSA_ASSERT(MixerSelemGetPlaybackVolumeRange, volume_mixer_->element,
              &volume_range_min_, &volume_range_max_);

  alsa_->MixerElemSetCallback(volume_mixer_->element,
                              &AlsaVolumeControl::VolumeOrMuteChangeCallback);
  alsa_->MixerElemSetCallbackPrivate(volume_mixer_->element,
                                     reinterpret_cast<void*>(this));
  RefreshMixerFds(volume_mixer_.get());

  if (mute_mixer_element_name_ != volume_mixer_element_name_) {
    mute_mixer_ = base::MakeUnique<ScopedAlsaMixer>(
        alsa_.get(), mute_mixer_device_name_, mute_mixer_element_name_);
    DCHECK(mute_mixer_->element);
    mute_mixer_ptr_ = mute_mixer_.get();

    alsa_->MixerElemSetCallback(mute_mixer_->element,
                                &AlsaVolumeControl::VolumeOrMuteChangeCallback);
    alsa_->MixerElemSetCallbackPrivate(mute_mixer_->element,
                                       reinterpret_cast<void*>(this));
    RefreshMixerFds(mute_mixer_.get());
  } else {
    mute_mixer_ptr_ = volume_mixer_.get();
  }
}

AlsaVolumeControl::~AlsaVolumeControl() = default;

float AlsaVolumeControl::VolumeThroughAlsa(float volume) {
  long level = 0;  // NOLINT(runtime/int)
  level = std::round((volume * (volume_range_max_ - volume_range_min_)) +
                     volume_range_min_);
  return static_cast<float>(level - volume_range_min_) /
         static_cast<float>(volume_range_max_ - volume_range_min_);
}

float AlsaVolumeControl::GetVolume() {
  long level = 0;  // NOLINT(runtime/int)
  ALSA_ASSERT(MixerSelemGetPlaybackVolume, volume_mixer_->element,
              SND_MIXER_SCHN_MONO, &level);
  return static_cast<float>(level - volume_range_min_) /
         static_cast<float>(volume_range_max_ - volume_range_min_);
}

void AlsaVolumeControl::SetVolume(float level) {
  float volume = std::round((level * (volume_range_max_ - volume_range_min_)) +
                            volume_range_min_);
  ALSA_ASSERT(MixerSelemSetPlaybackVolumeAll, volume_mixer_->element, volume);
}

bool AlsaVolumeControl::IsMuted() {
  if (!alsa_->MixerSelemHasPlaybackSwitch(mute_mixer_ptr_->element)) {
    LOG(ERROR) << "Mute failed: no mute switch on mixer element.";
    return false;
  }

  bool muted = false;
  for (int32_t channel = 0; channel <= SND_MIXER_SCHN_LAST; ++channel) {
    int channel_enabled = 0;
    int err = alsa_->MixerSelemGetPlaybackSwitch(
        mute_mixer_ptr_->element,
        static_cast<snd_mixer_selem_channel_id_t>(channel), &channel_enabled);
    if (err == 0) {
      muted = muted || (channel_enabled == 0);
    }
  }
  return muted;
}

void AlsaVolumeControl::SetMuted(bool muted) {
  if (!alsa_->MixerSelemHasPlaybackSwitch(mute_mixer_ptr_->element)) {
    LOG(ERROR) << "Mute failed: no mute switch on mixer element.";
    return;
  }
  for (int32_t channel = 0; channel <= SND_MIXER_SCHN_LAST; ++channel) {
    alsa_->MixerSelemSetPlaybackSwitch(
        mute_mixer_ptr_->element,
        static_cast<snd_mixer_selem_channel_id_t>(channel), !muted);
  }
}

void AlsaVolumeControl::RefreshMixerFds(ScopedAlsaMixer* mixer) {
  int num_fds = alsa_->MixerPollDescriptorsCount(mixer->mixer);
  DCHECK_GT(num_fds, 0);
  struct pollfd pfds[num_fds];
  num_fds = alsa_->MixerPollDescriptors(mixer->mixer, pfds, num_fds);
  DCHECK_GT(num_fds, 0);
  for (int i = 0; i < num_fds; ++i) {
    auto watcher =
        base::MakeUnique<base::MessageLoopForIO::FileDescriptorWatcher>(
            FROM_HERE);
    base::MessageLoopForIO::current()->WatchFileDescriptor(
        pfds[i].fd, true /* persistent */, base::MessageLoopForIO::WATCH_READ,
        watcher.get(), this);
    file_descriptor_watchers_.push_back(std::move(watcher));
  }
}

void AlsaVolumeControl::OnFileCanReadWithoutBlocking(int fd) {
  alsa_->MixerHandleEvents(volume_mixer_->mixer);
  if (mute_mixer_) {
    alsa_->MixerHandleEvents(mute_mixer_->mixer);
  }
}

void AlsaVolumeControl::OnFileCanWriteWithoutBlocking(int fd) {
  // Nothing to do.
}

void AlsaVolumeControl::OnVolumeOrMuteChanged() {
  delegate_->OnAlsaVolumeOrMuteChange(GetVolume(), IsMuted());
}

// static
int AlsaVolumeControl::VolumeOrMuteChangeCallback(snd_mixer_elem_t* elem,
                                                  unsigned int mask) {
  if (!(mask & SND_CTL_EVENT_MASK_VALUE))
    return 0;

  AlsaVolumeControl* instance = static_cast<AlsaVolumeControl*>(
      snd_mixer_elem_get_callback_private(elem));
  instance->OnVolumeOrMuteChanged();
  return 0;
}

}  // namespace media
}  // namespace chromecast
