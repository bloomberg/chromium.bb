// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/freezer_cgroup_process_manager.h"

#include <string>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"

namespace chromeos {

namespace {
const char kFreezerPath[] = "/sys/fs/cgroup/freezer/chrome_renderers";
const char kToBeFrozen[] = "to_be_frozen";
const char kFreezerState[] = "freezer.state";
const char kCgroupProcs[] = "cgroup.procs";

const char kFreezeCommand[] = "FROZEN";
const char kThawCommand[] = "THAWED";

}  // namespace

FreezerCgroupProcessManager::FreezerCgroupProcessManager()
    : default_control_path_(base::FilePath(kFreezerPath).Append(kCgroupProcs)),
      to_be_frozen_control_path_(base::FilePath(kFreezerPath)
                                     .AppendASCII(kToBeFrozen)
                                     .AppendASCII(kCgroupProcs)),
      to_be_frozen_state_path_(base::FilePath(kFreezerPath)
                                   .AppendASCII(kToBeFrozen)
                                   .AppendASCII(kFreezerState)),
      enabled_(base::PathIsWritable(default_control_path_) &&
               base::PathIsWritable(to_be_frozen_control_path_) &&
               base::PathIsWritable(to_be_frozen_state_path_)) {
  if (!enabled_) {
    LOG(WARNING) << "Cgroup freezer does not exist or is not writable. "
                 << "Unable to freeze renderer processes.";
  }
}

FreezerCgroupProcessManager::~FreezerCgroupProcessManager() {
}

void FreezerCgroupProcessManager::SetShouldFreezeRenderer(
    base::ProcessHandle handle,
    bool frozen) {
  std::string pid = base::StringPrintf("%d", handle);

  WriteCommandToFile(
      pid, frozen ? to_be_frozen_control_path_ : default_control_path_);
}

bool FreezerCgroupProcessManager::FreezeRenderers() {
  if (!enabled_) {
    LOG(ERROR) << "Attempting to freeze renderers when the freezer cgroup is "
               << "not available.";
    return false;
  }

  return WriteCommandToFile(kFreezeCommand, to_be_frozen_state_path_);
}

bool FreezerCgroupProcessManager::ThawRenderers() {
  if (!enabled_) {
    LOG(ERROR) << "Attempting to thaw renderers when the freezer cgroup is not "
               << "available.";
    return false;
  }

  return WriteCommandToFile(kThawCommand, to_be_frozen_state_path_);
}

bool FreezerCgroupProcessManager::CanFreezeRenderers() {
  return enabled_;
}

bool FreezerCgroupProcessManager::WriteCommandToFile(
    const std::string& command,
    const base::FilePath& file) {
  int bytes = base::WriteFile(file, command.c_str(), command.size());
  if (bytes == -1) {
    PLOG(ERROR) << "Writing " << command << " to " << file.value() << " failed";
    return false;
  } else if (bytes != static_cast<int>(command.size())) {
    LOG(ERROR) << "Only wrote " << bytes << " byte(s) when writing " << command
               << " to " << file.value();
    return false;
  }
  return true;
}

}  // namespace chromeos
