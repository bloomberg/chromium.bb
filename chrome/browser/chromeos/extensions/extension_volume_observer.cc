// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/extension_volume_observer.h"

#include "chrome/browser/extensions/api/system_private/system_private_api.h"
#include "chromeos/audio/cras_audio_handler.h"

namespace chromeos {

namespace {

// A helper to call the corresponding extensions method.
void DispatchVolumeChangedEvent() {
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  extensions::DispatchVolumeChangedEvent(
      audio_handler->GetOutputVolumePercent(), audio_handler->IsOutputMuted());
}

}  // namespace

ExtensionVolumeObserver::ExtensionVolumeObserver() {
  CrasAudioHandler::Get()->AddAudioObserver(this);
}

ExtensionVolumeObserver::~ExtensionVolumeObserver() {
  if (CrasAudioHandler::IsInitialized())
    CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

void ExtensionVolumeObserver::OnOutputNodeVolumeChanged(uint64_t node_id,
                                                        int volume) {
  DispatchVolumeChangedEvent();
}

void ExtensionVolumeObserver::OnOutputMuteChanged(bool /* mute_on */,
                                                  bool /* system_adjust */) {
  DispatchVolumeChangedEvent();
}

}  // namespace chromeos
