// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pepper_flash.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"

#if defined(OS_WIN)
#include "base/win/metro.h"
#endif

namespace {

const char* const kFieldTrialName = "ApplyPepperFlash";
const char* const kDisableGroupName = "DisableByDefault";
const char* const kEnableGroupName = "EnableByDefault";
int g_disabled_group_number = -1;

void ActivateFieldTrial() {
  // The field trial will expire on Jan 1st, 2014.
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          kFieldTrialName, 1000, kDisableGroupName, 2014, 1, 1,
          &g_disabled_group_number));

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kPpapiFlashFieldTrial)) {
    std::string switch_value =
        command_line->GetSwitchValueASCII(switches::kPpapiFlashFieldTrial);
    if (switch_value == switches::kPpapiFlashFieldTrialEnableByDefault) {
      trial->AppendGroup(kEnableGroupName, 1000);
      return;
    } else if (switch_value ==
        switches::kPpapiFlashFieldTrialDisableByDefault) {
      return;
    }
  }

  // Disable by default if one time randomization is not available.
  if (!base::FieldTrialList::IsOneTimeRandomizationEnabled())
    return;

  trial->UseOneTimeRandomization();
  // 50% goes into the enable-by-default group.
  trial->AppendGroup(kEnableGroupName, 500);
}

bool IsInFieldTrialGroup() {
  static bool activated = false;
  if (!activated) {
    ActivateFieldTrial();
    activated = true;
  }

  int group = base::FieldTrialList::FindValue(kFieldTrialName);
  return group != base::FieldTrial::kNotFinalized &&
         group != g_disabled_group_number;
}

}  // namespace

bool ConductingPepperFlashFieldTrial() {
#if defined(OS_WIN)
  return true;
#else
  return false;
#endif
}

bool IsPepperFlashEnabledByDefault() {
#if defined(USE_AURA)
  // Pepper Flash is required for Aura (on any OS).
  return true;
#elif defined(OS_WIN)
  // Pepper Flash is required for Windows 8 Metro mode.
  if (base::win::GetMetroModule())
    return true;

  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_CANARY)
    return true;

  // For other Windows users, enable only for Dev users in a field trial.
  return channel == chrome::VersionInfo::CHANNEL_DEV && IsInFieldTrialGroup();
#elif defined(OS_LINUX)
  // For Linux, always try to use it (availability is checked elsewhere).
  return true;
#else
  return false;
#endif
}
