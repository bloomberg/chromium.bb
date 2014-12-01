// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/freezer_cgroup_process_manager.h"

#include <string>

#include "base/files/file_util.h"
#include "base/logging.h"

namespace chromeos {

namespace {
const char kFreezerStatePath[] =
    "/sys/fs/cgroup/freezer/chrome_renderers/freezer.state";
const char kFreezeCommand[] = "FROZEN";
const char kThawCommand[] = "THAWED";

}  // namespace

FreezerCgroupProcessManager::FreezerCgroupProcessManager()
    : state_path_(base::FilePath(kFreezerStatePath)),
      enabled_(base::PathIsWritable(state_path_)) {
  if (!enabled_) {
    LOG(WARNING) << "Cgroup freezer does not exist or is not writable. "
                 << "Unable to freeze renderer processes.";
  }
}

FreezerCgroupProcessManager::~FreezerCgroupProcessManager() {
}

bool FreezerCgroupProcessManager::FreezeRenderers() {
  if (!enabled_) {
    LOG(ERROR) << "Attempting to freeze renderers when the freezer cgroup is "
               << "not available.";
    return false;
  }

  return WriteCommandToStateFile(kFreezeCommand);
}

bool FreezerCgroupProcessManager::ThawRenderers() {
  if (!enabled_) {
    LOG(ERROR) << "Attempting to thaw renderers when the freezer cgroup is not "
               << "available.";
    return false;
  }

  return WriteCommandToStateFile(kThawCommand);
}

bool FreezerCgroupProcessManager::CanFreezeRenderers() {
  return enabled_;
}

bool FreezerCgroupProcessManager::WriteCommandToStateFile(
    const std::string& command) {
  int bytes = base::WriteFile(state_path_, command.c_str(), command.size());
  if (bytes == -1) {
    PLOG(ERROR) << "Writing " << command << " to " << state_path_.value()
                << " failed";
    return false;
  } else if (bytes != static_cast<int>(command.size())) {
    LOG(ERROR) << "Only wrote " << bytes << " byte(s) when writing "
               << command << " to " << state_path_.value();
    return false;
  }
  return true;
}

}  // namespace chromeos
