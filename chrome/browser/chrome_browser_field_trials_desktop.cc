// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials_desktop.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/auto_launch_trial.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/tracing/background_tracing_field_trial.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/variations/variations_util.h"
#include "components/variations/variations_associated_data.h"

namespace chrome {

namespace {

void AutoLaunchChromeFieldTrial() {
  std::string brand;
  google_brand::GetBrand(&brand);

  // Create a 100% field trial based on the brand code.
  if (auto_launch_trial::IsInExperimentGroup(brand)) {
    base::FieldTrialList::CreateFieldTrial(kAutoLaunchTrialName,
                                           kAutoLaunchTrialAutoLaunchGroup);
  } else if (auto_launch_trial::IsInControlGroup(brand)) {
    base::FieldTrialList::CreateFieldTrial(kAutoLaunchTrialName,
                                           kAutoLaunchTrialControlGroup);
  }
}

void SetupLightSpeedTrials() {
  if (!variations::GetVariationParamValue("LightSpeed", "NoGpu").empty()) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableGpu);
  }
}

void SetupStunProbeTrial() {
  std::map<std::string, std::string> params;
  if (!variations::GetVariationParams("StunProbeTrial", &params))
    return;

  // The parameter, used by StartStunFieldTrial, should have the following
  // format: "request_per_ip/interval/sharedsocket/server1:port/server2:port/
  // server3:port/"
  std::string cmd_param = params["request_per_ip"] + "/" + params["interval"] +
                          "/" + params["sharedsocket"] + "/" +
                          params["server1"] + "/" + params["server2"] + "/" +
                          params["server3"] + "/";

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kWebRtcStunProbeTrialParameter, cmd_param);
}

}  // namespace

void SetupDesktopFieldTrials(const base::CommandLine& parsed_command_line) {
  prerender::ConfigurePrerender(parsed_command_line);
  AutoLaunchChromeFieldTrial();
  SetupLightSpeedTrials();
  tracing::SetupBackgroundTracingFieldTrial();
  SetupStunProbeTrial();
}

}  // namespace chrome
