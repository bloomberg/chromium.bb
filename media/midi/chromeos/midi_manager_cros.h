// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_CHROMEOS_MIDI_MANAGER_CROS_H_
#define MEDIA_MIDI_CHROMEOS_MIDI_MANAGER_CROS_H_

#include "media/midi/midi_export.h"
#include "media/midi/midi_manager.h"

namespace midi {

class MIDI_EXPORT MidiManagerCros final : public MidiManager {
 public:
  explicit MidiManagerCros(MidiService* service);
  ~MidiManagerCros() override;

  DISALLOW_COPY_AND_ASSIGN(MidiManagerCros);
};

}  // namespace midi

#endif  // MEDIA_MIDI_CHROMEOS_MIDI_MANAGER_CROS_H_
