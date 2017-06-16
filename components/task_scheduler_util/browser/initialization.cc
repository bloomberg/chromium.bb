// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/task_scheduler_util/browser/initialization.h"

#include <map>
#include <string>

#include "components/task_scheduler_util/common/variations_util.h"
#include "components/variations/variations_associated_data.h"

namespace task_scheduler_util {

namespace {

constexpr char kFieldTrialName[] = "BrowserScheduler";

}  // namespace

std::unique_ptr<base::TaskScheduler::InitParams>
GetBrowserTaskSchedulerInitParamsFromVariations() {
  std::map<std::string, std::string> variation_params;
  if (!::variations::GetVariationParams(kFieldTrialName, &variation_params))
    return nullptr;

  return GetTaskSchedulerInitParams(
      "", variation_params, base::SchedulerBackwardCompatibility::INIT_COM_STA);
}

}  // namespace task_scheduler_util
