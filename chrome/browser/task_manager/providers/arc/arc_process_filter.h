// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_ARC_ARC_PROCESS_FILTER_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_ARC_ARC_PROCESS_FILTER_H_

#include <set>
#include <string>

#include "chrome/browser/chromeos/arc/process/arc_process.h"

namespace task_manager {

// The many ARC system apps can make the task manager noisy. This filter
// determines whether to show an ARC process in the task manager or not.
// Apps can be whitelisted explicitly, or allowed based on their ProcessState.
// See crbug.com/654564
class ArcProcessFilter {
 public:
  // Zero argument constructor will use a default whitelist.
  ArcProcessFilter();
  explicit ArcProcessFilter(const std::set<std::string>& whitelist);
  ~ArcProcessFilter();

  bool ShouldDisplayProcess(const arc::ArcProcess& process) const;

 private:
  const std::set<std::string> whitelist_;

  DISALLOW_COPY_AND_ASSIGN(ArcProcessFilter);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_ARC_ARC_PROCESS_FILTER_H_
