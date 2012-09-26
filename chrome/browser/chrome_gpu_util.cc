// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_gpu_util.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/version.h"
#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"

using content::GpuDataManager;

namespace gpu_util {

bool ShouldRunStage3DFieldTrial() {
#if !defined(OS_WIN)
  return false;
#else
  if (base::win::GetVersion() >= base::win::VERSION_VISTA)
    return false;
  return true;
#endif
}

void InitializeStage3DFieldTrial() {
  if (!ShouldRunStage3DFieldTrial()) {
    base::FieldTrial* trial =
        base::FieldTrialList::Find(content::kStage3DFieldTrialName);
    if (trial)
      trial->Disable();
    return;
  }

  const base::FieldTrial::Probability kDivisor = 1000;
  scoped_refptr<base::FieldTrial> trial(
    base::FieldTrialList::FactoryGetFieldTrial(
        content::kStage3DFieldTrialName, kDivisor,
        content::kStage3DFieldTrialEnabledName, 2013, 3, 1, NULL));

  // Produce the same result on every run of this client.
  trial->UseOneTimeRandomization();

  // Kill-switch, so disabled unless we get info from server.
  int blacklisted_group = trial->AppendGroup(
      content::kStage3DFieldTrialBlacklistedName, kDivisor);

  bool enabled = (trial->group() != blacklisted_group);

  UMA_HISTOGRAM_BOOLEAN("GPU.Stage3DFieldTrial", enabled);
}

void DisableCompositingFieldTrial() {
  base::FieldTrial* trial =
      base::FieldTrialList::Find(content::kGpuCompositingFieldTrialName);
  if (trial)
    trial->Disable();
}

bool ShouldRunCompositingFieldTrial() {
// Enable the field trial only on desktop OS's.
#if !(defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX))
  return false;
#endif

// Necessary for linux_chromeos build since it defines both OS_LINUX
// and OS_CHROMEOS .
#if defined(OS_CHROMEOS)
  return false;
#endif

#if defined(OS_WIN)
  // Don't run the trial on Windows XP.
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return false;
#endif

  // The performance of accelerated compositing is too low with software
  // rendering.
  if (content::GpuDataManager::GetInstance()->ShouldUseSoftwareRendering())
    return false;

  // Don't activate the field trial if force-compositing-mode has been
  // explicitly disabled from the command line.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableForceCompositingMode))
    return false;

  return true;
}

// Note: The compositing field trial may be created at startup time via the
// Finch framework. In that case, all the Groups and probability values are
// set before this function is called and any Field Trial setup calls
// made here are simply ignored.
// Early outs from this function intended to bypass activation of the field
// trial must call DisableCompositingFieldTrial() before returning.
void InitializeCompositingFieldTrial() {
  // Early out in configurations that should not run the compositing
  // field trial.
  if (!ShouldRunCompositingFieldTrial()) {
    DisableCompositingFieldTrial();
    return;
  }

  const base::FieldTrial::Probability kDivisor = 3;
  // Note: This field trial should be removed once we're comfortable with
  // turning force compositing mode and threaded compositing on all relevant
  // platforms (see crbug.com/149991).
  scoped_refptr<base::FieldTrial> trial(
    base::FieldTrialList::FactoryGetFieldTrial(
        content::kGpuCompositingFieldTrialName, kDivisor,
        "disable", 2013, 12, 31, NULL));

  // Produce the same result on every run of this client.
  trial->UseOneTimeRandomization();

  // Note: The static field trial probabilities set here can be overwritten
  // at runtime by Finch. Changing these static values won't have an effect
  // if a Finch study is active.
  base::FieldTrial::Probability force_compositing_mode_probability = 0;
  base::FieldTrial::Probability threaded_compositing_probability = 0;

  // Threaded compositing mode isn't feature complete on mac or linux yet:
  // http://crbug.com/133602 for mac
  // http://crbug.com/140866 for linux

#if defined(OS_WIN)
  // threaded-compositing turned on by default on Windows.
  // (Windows XP is excluded explicitly in ShouldRunCompositingFieldTrial)
  threaded_compositing_probability = 3;
#elif defined(OS_MACOSX)
  // force-compositing-mode turned on by default on mac.
  force_compositing_mode_probability = 3;
#elif defined(OS_LINUX)
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel != chrome::VersionInfo::CHANNEL_STABLE &&
      channel != chrome::VersionInfo::CHANNEL_BETA) {
    // On channels < beta, force-compositing-mode and
    // threaded-compositing on with 1/3 probability each.
    force_compositing_mode_probability = 1;
    threaded_compositing_probability = 1;
  }
#endif

  int force_compositing_group = trial->AppendGroup(
      content::kGpuCompositingFieldTrialForceCompositingEnabledName,
      force_compositing_mode_probability);
  int thread_group = trial->AppendGroup(
      content::kGpuCompositingFieldTrialThreadEnabledName,
      threaded_compositing_probability);

  bool force_compositing = (trial->group() == force_compositing_group);
  bool thread = (trial->group() == thread_group);
  UMA_HISTOGRAM_BOOLEAN("GPU.InForceCompositingModeFieldTrial",
                        force_compositing);
  UMA_HISTOGRAM_BOOLEAN("GPU.InCompositorThreadFieldTrial", thread);
}

}  // namespace gpu_util;

