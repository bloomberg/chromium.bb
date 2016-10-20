// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTENSION_VOLUME_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTENSION_VOLUME_OBSERVER_H_

#include "base/macros.h"
#include "chromeos/audio/cras_audio_handler.h"

namespace chromeos {

// Dispatches extension events in response to volume events.
class ExtensionVolumeObserver : public CrasAudioHandler::AudioObserver {
 public:
  // This class registers/unregisters itself as an observer in ctor/dtor.
  ExtensionVolumeObserver();
  ~ExtensionVolumeObserver() override;

  // CrasAudioHandler::AudioObserver:
  void OnOutputNodeVolumeChanged(uint64_t node_id, int volume) override;
  void OnOutputMuteChanged(bool mute_on, bool system_adjust) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionVolumeObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTENSION_VOLUME_OBSERVER_H_
