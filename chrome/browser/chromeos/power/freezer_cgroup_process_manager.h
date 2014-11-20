// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_FREEZER_CGROUP_PROCESS_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_FREEZER_CGROUP_PROCESS_MANAGER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#include "chrome/browser/chromeos/power/renderer_freezer.h"

namespace chromeos {

// Manages all the processes in the freezer cgroup on Chrome OS.
class FreezerCgroupProcessManager : public RendererFreezer::Delegate {
 public:
  FreezerCgroupProcessManager();
  ~FreezerCgroupProcessManager() override;

  // RendererFreezer::Delegate overrides.
  void SetShouldFreezeRenderer(base::ProcessHandle handle,
                               bool frozen) override;
  bool FreezeRenderers() override;
  bool ThawRenderers() override;
  bool CanFreezeRenderers() override;

 private:
  bool WriteCommandToFile(const std::string& command,
                          const base::FilePath& file);

  // Control path for the cgroup that is not frozen.
  base::FilePath default_control_path_;

  // Control and state paths for the cgroup whose processes will be frozen.
  base::FilePath to_be_frozen_control_path_;
  base::FilePath to_be_frozen_state_path_;

  bool enabled_;

  DISALLOW_COPY_AND_ASSIGN(FreezerCgroupProcessManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_FREEZER_CGROUP_PROCESS_MANAGER_H_
