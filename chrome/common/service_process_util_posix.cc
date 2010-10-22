// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/service_process_util.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_version_info.h"

namespace {

// Gets the name of the lock file for service process.
FilePath GetServiceProcessLockFilePath() {
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  chrome::VersionInfo version_info;
  std::string lock_file_name = version_info.Version() + "Service Process Lock";
  return user_data_dir.Append(lock_file_name);
}

}  // namespace

bool ForceServiceProcessShutdown(const std::string& version) {
  NOTIMPLEMENTED();
  return false;
}

bool CheckServiceProcessReady() {
  const FilePath path = GetServiceProcessLockFilePath();
  return file_util::PathExists(path);
}

struct ServiceProcessState::StateData {
  // No state yet for Posix.
};

bool ServiceProcessState::TakeSingletonLock() {
  // TODO(sanjeevr): Implement singleton mechanism for POSIX.
  NOTIMPLEMENTED();
  return true;
}

void ServiceProcessState::SignalReady(Task* shutdown_task) {
  // TODO(hclam): Implement better mechanism for these platform.
  // Also we need to save shutdown task. For now we just delete the shutdown
  // task because we have not way to listen for shutdown requests.
  delete shutdown_task;
  const FilePath path = GetServiceProcessLockFilePath();
  FILE* file = file_util::OpenFile(path, "wb+");
  if (!file)
    return;
  VLOG(1) << "Created Service Process lock file: " << path.value();
  file_util::TruncateFile(file) && file_util::CloseFile(file);
}

void ServiceProcessState::SignalStopped() {
  const FilePath path = GetServiceProcessLockFilePath();
  file_util::Delete(path, false);
  shared_mem_service_data_.reset();
}

bool ServiceProcessState::AddToAutoRun() {
  NOTIMPLEMENTED();
  return false;
}

bool ServiceProcessState::RemoveFromAutoRun() {
  NOTIMPLEMENTED();
  return false;
}

void ServiceProcessState::TearDownState() {
}

bool ServiceProcessState::ShouldHandleOtherVersion() {
  // On POSIX, the shared memory is a file in disk. We may have a stale file
  // lying around from a previous run. So the check is not reliable.
  return false;
}

