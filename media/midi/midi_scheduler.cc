// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_scheduler.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "media/midi/midi_manager.h"

namespace media {

MidiScheduler::MidiScheduler() : weak_factory_(this) {
}

MidiScheduler::~MidiScheduler() {
}

// TODO(crbug.com/467442): Use CancelableTaskTracker once it supports
// DelayedTask.
void MidiScheduler::PostSendDataTask(MidiManagerClient* client,
                                     size_t length,
                                     double timestamp,
                                     const base::Closure& closure) {
  DCHECK(client);

  const base::Closure& weak_closure = base::Bind(
      &MidiScheduler::InvokeClosure, weak_factory_.GetWeakPtr(), closure);

  base::TimeDelta delay;
  if (timestamp != 0.0) {
    base::TimeTicks time_to_send =
        base::TimeTicks() + base::TimeDelta::FromMicroseconds(
                                timestamp * base::Time::kMicrosecondsPerSecond);
    delay = std::max(time_to_send - base::TimeTicks::Now(), base::TimeDelta());
  }
  base::MessageLoop::current()->task_runner()->PostDelayedTask(
      FROM_HERE, weak_closure, delay);

  // TODO(crbug.com/467442): AccumulateMidiBytesSent should be called in
  // InvokeClosure. But for now, we call it here since |client| may be deleted
  // at that time.
  client->AccumulateMidiBytesSent(length);
}

void MidiScheduler::InvokeClosure(const base::Closure& closure) {
  closure.Run();
}

}  // namespace media
