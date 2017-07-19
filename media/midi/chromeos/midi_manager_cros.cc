// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/chromeos/midi_manager_cros.h"

#include "media/midi/midi_manager_alsa.h"
#include "media/midi/midi_switches.h"

namespace midi {

MidiManagerCros::MidiManagerCros(MidiService* service) : MidiManager(service) {}

MidiManagerCros::~MidiManagerCros() = default;

MidiManager* MidiManager::Create(MidiService* service) {
  // Note: Because of crbug.com/719489, chrome://flags does not affect
  // base::FeatureList::IsEnabled when you build target_os="chromeos" on Linux.
  if (base::FeatureList::IsEnabled(features::kMidiManagerCros))
    return new MidiManagerCros(service);
  return new MidiManagerAlsa(service);
}

}  // namespace midi
