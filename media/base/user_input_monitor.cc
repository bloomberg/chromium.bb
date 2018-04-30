// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/user_input_monitor.h"

#include "base/atomicops.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"

namespace media {

uint32_t ReadKeyPressMonitorCount(
    const base::ReadOnlySharedMemoryMapping& shmem_mapping) {
  if (!shmem_mapping.IsValid())
    return 0;

  // No ordering constraints between Load/Store operations, a temporary
  // inconsistent value is fine.
  return base::subtle::NoBarrier_Load(
      reinterpret_cast<const base::subtle::Atomic32*>(shmem_mapping.memory()));
}

void WriteKeyPressMonitorCount(
    const base::WritableSharedMemoryMapping& shmem_mapping,
    uint32_t count) {
  if (!shmem_mapping.IsValid())
    return;

  // No ordering constraints between Load/Store operations, a temporary
  // inconsistent value is fine.
  base::subtle::NoBarrier_Store(
      reinterpret_cast<base::subtle::Atomic32*>(shmem_mapping.memory()), count);
}

#ifdef DISABLE_USER_INPUT_MONITOR
// static
std::unique_ptr<UserInputMonitor> UserInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  return nullptr;
}
#endif  // DISABLE_USER_INPUT_MONITOR
UserInputMonitor::UserInputMonitor() = default;

UserInputMonitor::~UserInputMonitor() = default;

UserInputMonitorBase::UserInputMonitorBase() = default;

UserInputMonitorBase::~UserInputMonitorBase() {
  DCHECK_EQ(0u, references_);
}

void UserInputMonitorBase::EnableKeyPressMonitoring() {
  base::AutoLock auto_lock(lock_);
  ++references_;
  if (references_ == 1) {
    StartKeyboardMonitoring();
    DVLOG(2) << "Started keyboard monitoring.";
  }
}

void UserInputMonitorBase::DisableKeyPressMonitoring() {
  base::AutoLock auto_lock(lock_);
  DCHECK_NE(references_, 0u);
  --references_;
  if (references_ == 0) {
    StopKeyboardMonitoring();
    DVLOG(2) << "Stopped keyboard monitoring.";
  }
}

}  // namespace media
