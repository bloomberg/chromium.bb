// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials_desktop.h"

#include <string>

#include "base/command_line.h"
#include "base/debug/activity_tracker.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/variations/variations_util.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/common/content_switches.h"

namespace chrome {

namespace {

const base::Feature kStabilityDebuggingFeature{
    "StabilityDebugging", base::FEATURE_DISABLED_BY_DEFAULT
};

void SetupStunProbeTrial() {
#if defined(ENABLE_WEBRTC)
  std::map<std::string, std::string> params;
  if (!variations::GetVariationParams("StunProbeTrial2", &params))
    return;

  // The parameter, used by StartStunFieldTrial, should have the following
  // format: "request_per_ip/interval/sharedsocket/batch_size/total_batches/
  // server1:port/server2:port/server3:port/"
  std::string cmd_param = params["request_per_ip"] + "/" + params["interval"] +
                          "/" + params["sharedsocket"] + "/" +
                          params["batch_size"] + "/" + params["total_batches"] +
                          "/" + params["server1"] + "/" + params["server2"] +
                          "/" + params["server3"] + "/" + params["server4"] +
                          "/" + params["server5"] + "/" + params["server6"];

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kWebRtcStunProbeTrialParameter, cmd_param);
#endif
}

void SetupStabilityDebugging() {
  if (!base::FeatureList::IsEnabled(kStabilityDebuggingFeature))
    return;

  // TODO(bcwhite): Adjust these numbers once there is real data to show
  // just how much of an arena is necessary.
  const size_t kMemorySize = 1 << 20;  // 1 MiB
  const int kStackDepth = 4;
  const uint64_t kAllocatorId = 0;

  // Track code activities (such as posting task, blocking on locks, and
  // joining threads) that can cause hanging threads and general instability.
  base::FilePath user_data_dir;
  bool success = base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  DCHECK(success);
  base::debug::GlobalActivityTracker::CreateWithFile(
      user_data_dir
          .AppendASCII("StabilityDebugInfo")
          .AddExtension(base::PersistentMemoryAllocator::kFileExtension),
      kMemorySize, kAllocatorId, kStabilityDebuggingFeature.name, kStackDepth);
}

}  // namespace

void SetupDesktopFieldTrials(const base::CommandLine& parsed_command_line) {
  prerender::ConfigurePrerender(parsed_command_line);
  SetupStunProbeTrial();
  SetupStabilityDebugging();
}

}  // namespace chrome
