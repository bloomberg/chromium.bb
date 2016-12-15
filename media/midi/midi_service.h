// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_SERVICE_H_
#define MEDIA_MIDI_MIDI_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "media/midi/midi_export.h"
#include "media/midi/midi_manager.h"

namespace midi {

// Manages MidiManager backends.  This class expects to be constructed and
// destructed on the browser main thread, but methods can be called on both
// the main thread and the I/O thread.
class MIDI_EXPORT MidiService final {
 public:
  // |MidiManager| can be explicitly specified in the constructor for testing.
  explicit MidiService(std::unique_ptr<MidiManager> manager = nullptr);
  ~MidiService();

  // Called on the browser main thread to notify the I/O thread will stop and
  // the instance will be destructed on the main thread soon.
  void Shutdown();

  // A client calls StartSession() to receive and send MIDI data.
  void StartSession(MidiManagerClient* client);

  // A client calls EndSession() to stop receiving MIDI data.
  void EndSession(MidiManagerClient* client);

  // A client calls DispatchSendMidiData() to send MIDI data.
  virtual void DispatchSendMidiData(MidiManagerClient* client,
                                    uint32_t port_index,
                                    const std::vector<uint8_t>& data,
                                    double timestamp);

  std::unique_ptr<MidiManager> manager_;
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(MidiService);
};

}  // namespace midi

#endif  // MEDIA_MIDI_MIDI_SERVICE_H_
