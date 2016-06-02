// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_PROCESS_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_PROCESS_H_

#include <string>
#include <vector>

#include "base/process/process_handle.h"
#include "components/arc/common/process.mojom.h"

namespace arc {

class ArcProcess {
 public:
  ArcProcess(base::ProcessId nspid,
             base::ProcessId pid,
             const std::string& process_name,
             mojom::ProcessState process_state);
  ~ArcProcess();

  ArcProcess(ArcProcess&& other);
  ArcProcess& operator=(ArcProcess&& other);

  base::ProcessId nspid() const { return nspid_; }
  base::ProcessId pid() const { return pid_; }
  const std::string& process_name() const { return process_name_; }
  mojom::ProcessState process_state() const { return process_state_; }
  std::vector<std::string>& packages() { return packages_; }
  const std::vector<std::string>& packages() const { return packages_; }

 private:
  base::ProcessId nspid_;
  base::ProcessId pid_;
  std::string process_name_;
  mojom::ProcessState process_state_;
  std::vector<std::string> packages_;

  DISALLOW_COPY_AND_ASSIGN(ArcProcess);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_PROCESS_H_
