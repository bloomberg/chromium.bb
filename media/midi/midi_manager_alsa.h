// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_MANAGER_ALSA_H_
#define MEDIA_MIDI_MIDI_MANAGER_ALSA_H_

#include <poll.h>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "media/midi/midi_manager.h"

namespace media {

class MidiManagerAlsa : public MidiManager {
 public:
  MidiManagerAlsa();
  virtual ~MidiManagerAlsa();

  // MidiManager implementation.
  virtual MidiResult Initialize() OVERRIDE;
  virtual void DispatchSendMidiData(MidiManagerClient* client,
                                    uint32 port_index,
                                    const std::vector<uint8>& data,
                                    double timestamp) OVERRIDE;

 private:
  void EventReset();
  void EventLoop();

  class MidiDeviceInfo;
  std::vector<scoped_refptr<MidiDeviceInfo> > in_devices_;
  std::vector<scoped_refptr<MidiDeviceInfo> > out_devices_;
  base::Thread send_thread_;
  base::Thread event_thread_;

  // Used for shutting down the |event_thread_| safely.
  int pipe_fd_[2];
  // Used for polling input MIDI ports and |pipe_fd_| in |event_thread_|.
  std::vector<struct pollfd> poll_fds_;

  DISALLOW_COPY_AND_ASSIGN(MidiManagerAlsa);
};

}  // namespace media

#endif  // MEDIA_MIDI_MIDI_MANAGER_ALSA_H_
