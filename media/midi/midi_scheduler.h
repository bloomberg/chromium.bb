// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_SCHEDULER_H_
#define MEDIA_MIDI_MIDI_SCHEDULER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"

namespace media {

class MidiManagerClient;

// TODO(crbug.com/467442): Make tasks cancelable per client.
class MidiScheduler final {
 public:
  MidiScheduler();
  ~MidiScheduler();

  // Post |closure| to the current message loop safely. The |closure| will not
  // be invoked after MidiScheduler is deleted. AccumulateMidiBytesSent() of
  // |client| is called internally.
  void PostSendDataTask(MidiManagerClient* client,
                        size_t length,
                        double timestamp,
                        const base::Closure& closure);

 private:
  void InvokeClosure(const base::Closure& closure);

  base::WeakPtrFactory<MidiScheduler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MidiScheduler);
};

}  // namespace media

#endif  // MEDIA_MIDI_MIDI_SCHEDULER_H_
