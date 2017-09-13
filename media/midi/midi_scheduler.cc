// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_scheduler.h"

#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "media/midi/midi_manager.h"

namespace midi {

MidiScheduler::MidiScheduler(MidiManager* manager)
    : manager_(manager),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_factory_(this) {}

MidiScheduler::~MidiScheduler() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

// TODO(crbug.com/467442): Use CancelableTaskTracker once it supports
// DelayedTask.
// TODO(shaochuan): Unit tests to ensure thread safety.
void MidiScheduler::PostSendDataTask(MidiManagerClient* client,
                                     size_t length,
                                     double timestamp,
                                     base::OnceClosure closure) {
  DCHECK(client);

  base::OnceClosure weak_closure =
      base::BindOnce(&MidiScheduler::InvokeClosure, weak_factory_.GetWeakPtr(),
                     client, length, std::move(closure));

  base::TimeDelta delay;
  if (timestamp != 0.0) {
    base::TimeTicks time_to_send =
        base::TimeTicks() + base::TimeDelta::FromMicroseconds(
                                timestamp * base::Time::kMicrosecondsPerSecond);
    delay = std::max(time_to_send - base::TimeTicks::Now(), base::TimeDelta());
  }
  task_runner_->PostDelayedTask(FROM_HERE, std::move(weak_closure), delay);
}

void MidiScheduler::InvokeClosure(MidiManagerClient* client,
                                  size_t length,
                                  base::OnceClosure closure) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::move(closure).Run();
  manager_->AccumulateMidiBytesSent(client, length);
}

}  // namespace midi
