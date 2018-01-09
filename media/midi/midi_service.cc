// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_service.h"

#include "base/feature_list.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "media/midi/midi_manager.h"
#include "media/midi/midi_switches.h"
#include "media/midi/task_service.h"

namespace midi {

std::unique_ptr<MidiManager> MidiService::ManagerFactory::Create(
    MidiService* service) {
  return std::unique_ptr<MidiManager>(MidiManager::Create(service));
}

// static
base::TimeDelta MidiService::TimestampToTimeDeltaDelay(double timestamp) {
  base::TimeDelta delay;
  if (timestamp != 0.0) {
    base::TimeTicks time_to_send =
        base::TimeTicks() + base::TimeDelta::FromMicroseconds(
                                timestamp * base::Time::kMicrosecondsPerSecond);
    delay = std::max(time_to_send - base::TimeTicks::Now(), base::TimeDelta());
  }
  return delay;
}

MidiService::MidiService() : MidiService(std::make_unique<ManagerFactory>()) {}

MidiService::MidiService(std::unique_ptr<ManagerFactory> factory)
    : manager_factory_(std::move(factory)),
      task_service_(std::make_unique<TaskService>()) {}

MidiService::~MidiService() {
  base::AutoLock lock(lock_);

  manager_.reset();

  base::AutoLock threads_lock(threads_lock_);
  threads_.clear();
}

void MidiService::Shutdown() {
  base::AutoLock lock(lock_);
  if (manager_) {
    DCHECK(manager_destructor_runner_);
    manager_destructor_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MidiManager::Shutdown,
                                  base::Unretained(manager_.get())));
    manager_destructor_runner_->DeleteSoon(FROM_HERE, std::move(manager_));
    manager_destructor_runner_ = nullptr;
  }
}

void MidiService::StartSession(MidiManagerClient* client) {
  base::AutoLock lock(lock_);
  if (!manager_) {
    manager_ = manager_factory_->Create(this);
    DCHECK(!manager_destructor_runner_);
    manager_destructor_runner_ = base::ThreadTaskRunnerHandle::Get();
  }
  manager_->StartSession(client);
}

bool MidiService::EndSession(MidiManagerClient* client) {
  base::AutoLock lock(lock_);

  // |client| does not seem to be valid.
  if (!manager_ || !manager_->EndSession(client))
    return false;

// Do not destruct MidiManager on macOS to avoid a Core MIDI issue that
// MIDIClientCreate starts failing with the OSStatus -50 after repeated calls
// of MIDIClientDispose. It rarely happens, but once it starts, it will never
// get back to be sane. See https://crbug.com/718140.
#if !defined(OS_MACOSX)
  if (!manager_->HasOpenSession()) {
    // MidiManager for each platform should be able to shutdown correctly even
    // if following destruction happens in the middle of StartInitialization().
    manager_->Shutdown();
    manager_.reset();
    DCHECK(manager_destructor_runner_);
    DCHECK(manager_destructor_runner_->BelongsToCurrentThread());
    manager_destructor_runner_ = nullptr;
  }
#endif
  return true;
}

void MidiService::DispatchSendMidiData(MidiManagerClient* client,
                                       uint32_t port_index,
                                       const std::vector<uint8_t>& data,
                                       double timestamp) {
  base::AutoLock lock(lock_);

  // MidiService needs to consider invalid DispatchSendMidiData calls without
  // an open session that could be sent from a broken renderer.
  if (manager_)
    manager_->DispatchSendMidiData(client, port_index, data, timestamp);
}

scoped_refptr<base::SingleThreadTaskRunner> MidiService::GetTaskRunner(
    size_t runner_id) {
  base::AutoLock lock(threads_lock_);
  if (threads_.size() <= runner_id)
    threads_.resize(runner_id + 1);
  if (!threads_[runner_id]) {
    threads_[runner_id] = std::make_unique<base::Thread>(
        base::StringPrintf("MidiServiceThread(%zu)", runner_id));
#if defined(OS_WIN)
    threads_[runner_id]->init_com_with_mta(true);
#endif
    threads_[runner_id]->Start();
  }
  return threads_[runner_id]->task_runner();
}

}  // namespace midi
