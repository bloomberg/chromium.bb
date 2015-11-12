// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials_desktop.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/tracing/background_tracing_field_trial.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/variations/variations_util.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/common/content_switches.h"

namespace chrome {

namespace {

void SetupLightSpeedTrials() {
  if (!variations::GetVariationParamValue("LightSpeed", "NoGpu").empty()) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableGpu);
  }
}

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

}  // namespace

void SetupDesktopFieldTrials(const base::CommandLine& parsed_command_line) {
  prerender::ConfigurePrerender(parsed_command_line);
  SetupLightSpeedTrials();
  tracing::SetupBackgroundTracingFieldTrial();
  SetupStunProbeTrial();
}

}  // namespace chrome
