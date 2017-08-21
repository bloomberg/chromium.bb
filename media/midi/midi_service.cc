// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_service.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "media/midi/midi_manager.h"
#include "media/midi/midi_switches.h"
#include "media/midi/task_service.h"

namespace midi {

namespace {

bool IsDynamicInstantiationEnabled() {
// TODO(toyoshim): Support on all platforms. See https://crbug.com/672793.
#if defined(OS_LINUX) || defined(OS_WIN)
  return true;
#else
  return base::FeatureList::IsEnabled(
      features::kMidiManagerDynamicInstantiation);
#endif
}

}  // namespace

MidiService::MidiService(void)
    : task_service_(base::MakeUnique<TaskService>()),
      is_dynamic_instantiation_enabled_(IsDynamicInstantiationEnabled()) {
  base::AutoLock lock(lock_);

  if (!is_dynamic_instantiation_enabled_)
    manager_ = base::WrapUnique(MidiManager::Create(this));
}

MidiService::MidiService(std::unique_ptr<MidiManager> manager)
    : task_service_(base::MakeUnique<TaskService>()),
      is_dynamic_instantiation_enabled_(false) {
  base::AutoLock lock(lock_);

  manager_ = std::move(manager);
}

MidiService::~MidiService() {
  base::AutoLock lock(lock_);

  manager_.reset();

  base::AutoLock threads_lock(threads_lock_);
  threads_.clear();
}

void MidiService::Shutdown() {
  base::AutoLock lock(lock_);
  if (manager_) {
    manager_->Shutdown();
    if (is_dynamic_instantiation_enabled_)
      manager_destructor_runner_->DeleteSoon(FROM_HERE, std::move(manager_));
    manager_destructor_runner_ = nullptr;
  }
}

void MidiService::StartSession(MidiManagerClient* client) {
  base::AutoLock lock(lock_);
  if (!manager_) {
    DCHECK(is_dynamic_instantiation_enabled_);
    manager_.reset(MidiManager::Create(this));
    if (!manager_destructor_runner_)
      manager_destructor_runner_ = base::ThreadTaskRunnerHandle::Get();
  }
  manager_->StartSession(client);
}

void MidiService::EndSession(MidiManagerClient* client) {
  base::AutoLock lock(lock_);

  // |client| does not seem to be valid.
  if (!manager_ || !manager_->EndSession(client))
    return;

  if (is_dynamic_instantiation_enabled_ && !manager_->HasOpenSession()) {
    // MidiManager for each platform should be able to shutdown correctly even
    // if following Shutdown() call happens in the middle of
    // StartInitialization() to support the dynamic instantiation feature.
    manager_->Shutdown();
    manager_.reset();
    manager_destructor_runner_ = nullptr;
  }
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
    threads_[runner_id] = base::MakeUnique<base::Thread>(
        base::StringPrintf("MidiServiceThread(%zu)", runner_id));
#if defined(OS_WIN)
    threads_[runner_id]->init_com_with_mta(true);
#endif
    threads_[runner_id]->Start();
  }
  return threads_[runner_id]->task_runner();
}

}  // namespace midi
