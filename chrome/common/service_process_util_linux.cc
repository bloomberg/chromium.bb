// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/service_process_util_posix.h"

#include <signal.h>
#include <unistd.h>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/threading/platform_thread.h"
#include "chrome/common/auto_start_linux.h"
#include "chrome/common/multi_process_lock.h"

namespace {

// Attempts to take a lock named |name|. If |waiting| is true then this will
// make multiple attempts to acquire the lock.
// Caller is responsible for ownership of the MultiProcessLock.
MultiProcessLock* TakeNamedLock(const std::string& name, bool waiting) {
  scoped_ptr<MultiProcessLock> lock(MultiProcessLock::Create(name));
  if (lock == NULL) return NULL;
  bool got_lock = false;
  for (int i = 0; i < 10; ++i) {
    if (lock->TryLock()) {
      got_lock = true;
      break;
    }
    if (!waiting) break;
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100 * i));
  }
  if (!got_lock) {
    lock.reset();
  }
  return lock.release();
}

MultiProcessLock* TakeServiceInitializingLock(bool waiting) {
  std::string lock_name =
      GetServiceProcessScopedName("_service_initializing");
  return TakeNamedLock(lock_name, waiting);
}

std::string GetBaseDesktopName() {
#if defined(GOOGLE_CHROME_BUILD)
  return "google-chrome-service.desktop";
#else  // CHROMIUM_BUILD
  return "chromium-service.desktop";
#endif
}
}  // namespace

MultiProcessLock* TakeServiceRunningLock(bool waiting) {
  std::string lock_name =
      GetServiceProcessScopedName("_service_running");
  return TakeNamedLock(lock_name, waiting);
}

bool ForceServiceProcessShutdown(const std::string& version,
                                 base::ProcessId process_id) {
  if (kill(process_id, SIGTERM) < 0) {
    DPLOG(ERROR) << "kill";
    return false;
  }
  return true;
}

// Gets the name of the service process IPC channel.
// Returns an absolute path as required.
IPC::ChannelHandle GetServiceProcessChannel() {
  FilePath temp_dir;
  PathService::Get(base::DIR_TEMP, &temp_dir);
  std::string pipe_name = GetServiceProcessScopedVersionedName("_service_ipc");
  std::string pipe_path = temp_dir.Append(pipe_name).value();
  return pipe_path;
}



bool CheckServiceProcessReady() {
  scoped_ptr<MultiProcessLock> running_lock(TakeServiceRunningLock(false));
  return running_lock.get() == NULL;
}

bool ServiceProcessState::TakeSingletonLock() {
  state_->initializing_lock_.reset(TakeServiceInitializingLock(true));
  return state_->initializing_lock_.get();
}

bool ServiceProcessState::AddToAutoRun() {
  DCHECK(autorun_command_line_.get());
#if defined(GOOGLE_CHROME_BUILD)
  std::string app_name = "Google Chrome Service";
#else  // CHROMIUM_BUILD
  std::string app_name = "Chromium Service";
#endif
  return AutoStart::AddApplication(
      GetServiceProcessScopedName(GetBaseDesktopName()),
      app_name,
      autorun_command_line_->GetCommandLineString(),
      false);
}

bool ServiceProcessState::RemoveFromAutoRun() {
  return AutoStart::Remove(
      GetServiceProcessScopedName(GetBaseDesktopName()));
}
