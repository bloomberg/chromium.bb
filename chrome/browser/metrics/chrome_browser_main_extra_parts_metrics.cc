// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_browser_main_extra_parts_metrics.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/metrics/histogram.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/shell_integration.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/touch/touch_device.h"
#include "ui/base/ui_base_switches.h"

namespace {

enum UMATouchEventsState {
  UMA_TOUCH_EVENTS_ENABLED,
  UMA_TOUCH_EVENTS_AUTO_ENABLED,
  UMA_TOUCH_EVENTS_AUTO_DISABLED,
  UMA_TOUCH_EVENTS_DISABLED,
  // NOTE: Add states only immediately above this line. Make sure to
  // update the enum list in tools/histograms/histograms.xml accordingly.
  UMA_TOUCH_EVENTS_STATE_COUNT
};

void LogIntelMicroArchitecture() {
#if defined(ARCH_CPU_X86_FAMILY)
  base::CPU cpu;
  base::CPU::IntelMicroArchitecture arch = cpu.GetIntelMicroArchitecture();
  UMA_HISTOGRAM_ENUMERATION("Platform.IntelMaxMicroArchitecture", arch,
                            base::CPU::MAX_INTEL_MICRO_ARCHITECTURE);
#endif  // defined(ARCH_CPU_X86_FAMILY)
}

void RecordDefaultBrowserUMAStat() {
  // Record whether Chrome is the default browser or not.
  ShellIntegration::DefaultWebClientState default_state =
      ShellIntegration::GetDefaultBrowser();
  UMA_HISTOGRAM_ENUMERATION("DefaultBrowser.State", default_state,
                            ShellIntegration::NUM_DEFAULT_STATES);
}

void RecordTouchEventState() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  const std::string touch_enabled_switch =
      command_line.HasSwitch(switches::kTouchEvents) ?
      command_line.GetSwitchValueASCII(switches::kTouchEvents) :
      switches::kTouchEventsAuto;

  UMATouchEventsState state;
  if (touch_enabled_switch.empty() ||
      touch_enabled_switch == switches::kTouchEventsEnabled) {
    state = UMA_TOUCH_EVENTS_ENABLED;
  } else if (touch_enabled_switch == switches::kTouchEventsAuto) {
    state = ui::IsTouchDevicePresent() ?
        UMA_TOUCH_EVENTS_AUTO_ENABLED : UMA_TOUCH_EVENTS_AUTO_DISABLED;
  } else if (touch_enabled_switch == switches::kTouchEventsDisabled) {
    state = UMA_TOUCH_EVENTS_DISABLED;
  } else {
    NOTREACHED();
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("Touchscreen.TouchEventsEnabled", state,
                            UMA_TOUCH_EVENTS_STATE_COUNT);
}

}  // namespace

ChromeBrowserMainExtraPartsMetrics::ChromeBrowserMainExtraPartsMetrics() {
}

ChromeBrowserMainExtraPartsMetrics::~ChromeBrowserMainExtraPartsMetrics() {
}

void ChromeBrowserMainExtraPartsMetrics::PreProfileInit() {
  LogIntelMicroArchitecture();
}

void ChromeBrowserMainExtraPartsMetrics::PreBrowserStart() {
  about_flags::RecordUMAStatistics(g_browser_process->local_state());

  // Querying the default browser state can be slow, do it in the background.
  content::BrowserThread::GetBlockingPool()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&RecordDefaultBrowserUMAStat),
        base::TimeDelta::FromSeconds(45));
}

void ChromeBrowserMainExtraPartsMetrics::PostBrowserStart() {
  RecordTouchEventState();
}

namespace chrome {

void AddMetricsExtraParts(ChromeBrowserMainParts* main_parts) {
  main_parts->AddParts(new ChromeBrowserMainExtraPartsMetrics());
}

}  // namespace chrome
