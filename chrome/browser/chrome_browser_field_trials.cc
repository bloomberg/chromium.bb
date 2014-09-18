// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/variations/uniformity_field_trials.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/omnibox/omnibox_field_trial.h"

#if defined(OS_ANDROID) || defined(OS_IOS)
#include "chrome/browser/chrome_browser_field_trials_mobile.h"
#else
#include "chrome/browser/chrome_browser_field_trials_desktop.h"
#endif

ChromeBrowserFieldTrials::ChromeBrowserFieldTrials(
    const CommandLine& parsed_command_line)
    : parsed_command_line_(parsed_command_line) {
}

ChromeBrowserFieldTrials::~ChromeBrowserFieldTrials() {
}

void ChromeBrowserFieldTrials::SetupFieldTrials(const base::Time& install_time,
                                                PrefService* local_state) {
  DCHECK(!install_time.is_null());

  // Field trials that are shared by all platforms.
  chrome_variations::SetupUniformityFieldTrials(install_time);
  InstantiateDynamicTrials();

#if defined(OS_ANDROID) || defined(OS_IOS)
  chrome::SetupMobileFieldTrials(parsed_command_line_);
#else
  chrome::SetupDesktopFieldTrials(
      parsed_command_line_, local_state);
#endif
}

void ChromeBrowserFieldTrials::InstantiateDynamicTrials() {
  // The following trials are used from renderer process.
  // Mark here so they will be sync-ed.
  base::FieldTrialList::FindValue("CLD1VsCLD2");
  base::FieldTrialList::FindValue("MouseEventPreconnect");
  base::FieldTrialList::FindValue("UnauthorizedPluginInfoBar");
  base::FieldTrialList::FindValue("DisplayList2dCanvas");
  base::FieldTrialList::FindValue("V8ScriptStreaming");
  // Activate the autocomplete dynamic field trials.
  OmniboxFieldTrial::ActivateDynamicTrials();
}
