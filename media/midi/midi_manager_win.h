// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_MANAGER_WIN_H_
#define MEDIA_MIDI_MIDI_MANAGER_WIN_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/thread.h"
#include "media/midi/midi_manager.h"

namespace media {

class MidiManagerWin : public MidiManager {
 public:
  MidiManagerWin();
  virtual ~MidiManagerWin();

  // MidiManager implementation.
  virtual void StartInitialization() override;
  virtual void DispatchSendMidiData(MidiManagerClient* client,
                                    uint32 port_index,
                                    const std::vector<uint8>& data,
                                    double timestamp) override;

 private:
  class InDeviceInfo;
  class OutDeviceInfo;
  ScopedVector<InDeviceInfo> in_devices_;
  ScopedVector<OutDeviceInfo> out_devices_;
  base::Thread send_thread_;
  DISALLOW_COPY_AND_ASSIGN(MidiManagerWin);
};

}  // namespace media

#endif  // MEDIA_MIDI_MIDI_MANAGER_WIN_H_
