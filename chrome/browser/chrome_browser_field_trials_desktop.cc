// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials_desktop.h"

#include <map>
#include <string>

#include "base/command_line.h"
#include "base/debug/activity_tracker.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/browser_watcher/features.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/common/content_switches.h"
#include "media/media_features.h"

#if defined(OS_WIN)
#include "components/browser_watcher/stability_debugging_win.h"
#endif

namespace chrome {

namespace {

void SetupStunProbeTrial() {
#if BUILDFLAG(ENABLE_WEBRTC)
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

#if defined(OS_WIN)
// DO NOT CHANGE VALUES. This is logged persistently in a histogram.
enum StabilityDebuggingInitializationStatus {
  INIT_SUCCESS = 0,
  CREATE_STABILITY_DIR_FAILED = 1,
  GET_STABILITY_FILE_PATH_FAILED = 2,
  INIT_STATUS_MAX = 3
};

void LogStabilityDebuggingInitStatus(
    StabilityDebuggingInitializationStatus status) {
  UMA_HISTOGRAM_ENUMERATION("ActivityTracker.Record.InitStatus", status,
                            INIT_STATUS_MAX);
}

void SetupStabilityDebugging() {
  if (!base::FeatureList::IsEnabled(
          browser_watcher::kStabilityDebuggingFeature)) {
    return;
  }

  // TODO(bcwhite): Adjust these numbers once there is real data to show
  // just how much of an arena is necessary.
  const size_t kMemorySize = 1 << 20;  // 1 MiB
  const int kStackDepth = 4;
  const uint64_t kAllocatorId = 0;

  // Ensure the stability directory exists and determine the stability file's
  // path.
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir) ||
      !base::CreateDirectory(browser_watcher::GetStabilityDir(user_data_dir))) {
    LOG(ERROR) << "Failed to create the stability directory.";
    LogStabilityDebuggingInitStatus(CREATE_STABILITY_DIR_FAILED);
    return;
  }
  base::FilePath stability_file;
  if (!browser_watcher::GetStabilityFileForProcess(
          base::Process::Current(), user_data_dir, &stability_file)) {
    LOG(ERROR) << "Failed to obtain stability file's path.";
    LogStabilityDebuggingInitStatus(GET_STABILITY_FILE_PATH_FAILED);
    return;
  }
  LogStabilityDebuggingInitStatus(INIT_SUCCESS);

  // Track code activities (such as posting task, blocking on locks, and
  // joining threads) that can cause hanging threads and general instability
  base::debug::GlobalActivityTracker::CreateWithFile(
      stability_file, kMemorySize, kAllocatorId,
      browser_watcher::kStabilityDebuggingFeature.name, kStackDepth);
}
#endif  // defined(OS_WIN)

}  // namespace

void SetupDesktopFieldTrials(const base::CommandLine& parsed_command_line) {
  prerender::ConfigurePrerender(parsed_command_line);
  SetupStunProbeTrial();
#if defined(OS_WIN)
  SetupStabilityDebugging();
#endif  // defined(OS_WIN)
}

}  // namespace chrome
