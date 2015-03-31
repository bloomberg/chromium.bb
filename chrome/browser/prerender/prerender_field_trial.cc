// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_field_trial.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_switches.h"

using std::string;

namespace prerender {

namespace {

const char kDefaultPrerenderServiceURLPrefix[] =
    "https://clients4.google.com/prerenderservice/?q=";
const int kDefaultPrerenderServiceTimeoutMs = 1000;
const int kDefaultPrefetchListTimeoutSeconds = 300;

}  // end namespace

void ConfigurePrerender(const base::CommandLine& command_line) {
  PrerenderManager::PrerenderManagerMode mode =
      PrerenderManager::PRERENDER_MODE_ENABLED;
  if (command_line.HasSwitch(switches::kPrerenderMode)) {
    const string switch_value =
        command_line.GetSwitchValueASCII(switches::kPrerenderMode);

    if (switch_value == switches::kPrerenderModeSwitchValueDisabled) {
      mode = PrerenderManager::PRERENDER_MODE_DISABLED;
    } else if (switch_value.empty() ||
               switch_value == switches::kPrerenderModeSwitchValueEnabled) {
      // The empty string means the option was provided with no value, and that
      // means enable.
      mode = PrerenderManager::PRERENDER_MODE_ENABLED;
    } else {
      mode = PrerenderManager::PRERENDER_MODE_DISABLED;
      LOG(ERROR) << "Invalid --prerender option received on command line: "
                 << switch_value;
      LOG(ERROR) << "Disabling prerendering!";
    }
  }

  PrerenderManager::SetMode(mode);
}

bool IsOmniboxEnabled(Profile* profile) {
  if (!profile)
    return false;

  if (!PrerenderManager::IsPrerenderingPossible())
    return false;

  // Override any field trial groups if the user has set a command line flag.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kPrerenderFromOmnibox)) {
    const string switch_value =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kPrerenderFromOmnibox);

    if (switch_value == switches::kPrerenderFromOmniboxSwitchValueEnabled)
      return true;

    if (switch_value == switches::kPrerenderFromOmniboxSwitchValueDisabled)
      return false;

    DCHECK_EQ(switches::kPrerenderFromOmniboxSwitchValueAuto, switch_value);
  }

  return (base::FieldTrialList::FindFullName("PrerenderFromOmnibox") !=
          "OmniboxPrerenderDisabled");
}

bool IsUnencryptedSyncEnabled(Profile* profile) {
  ProfileSyncService* service = ProfileSyncServiceFactory::GetInstance()->
      GetForProfile(profile);
  return service && service->GetOpenTabsUIDelegate() &&
      !service->EncryptEverythingEnabled();
}

// Indicates whether the Local Predictor is enabled based on field trial
// selection.
bool IsLocalPredictorEnabled() {
  return false;
}

bool ShouldDisableLocalPredictorBasedOnSyncAndConfiguration(Profile* profile) {
  return false;
}

bool ShouldDisableLocalPredictorDueToPreferencesAndNetwork(Profile* profile) {
  return false;
}

bool IsLoggedInPredictorEnabled() {
  return IsLocalPredictorEnabled();
}

bool IsSideEffectFreeWhitelistEnabled() {
  return IsLocalPredictorEnabled();
}

bool IsLocalPredictorPrerenderLaunchEnabled() {
  return true;
}

bool IsLocalPredictorPrerenderAlwaysControlEnabled() {
  // If we prefetch rather than prerender, we automatically also prerender
  // as a control group only.
  return IsLocalPredictorPrerenderPrefetchEnabled();
}

bool IsLocalPredictorPrerenderPrefetchEnabled() {
  return false;
}

bool ShouldQueryPrerenderService(Profile* profile) {
  return false;
}

bool ShouldQueryPrerenderServiceForCurrentURL() {
  return true;
}

bool ShouldQueryPrerenderServiceForCandidateURLs() {
  return true;
}

string GetPrerenderServiceURLPrefix() {
  return kDefaultPrerenderServiceURLPrefix;
}

int GetPrerenderServiceBehaviorID() {
  return 0;
}

int GetPrerenderServiceFetchTimeoutMs() {
  return kDefaultPrerenderServiceTimeoutMs;
}

int GetPrerenderPrefetchListTimeoutSeconds() {
  return kDefaultPrefetchListTimeoutSeconds;
}

int GetLocalPredictorTTLSeconds() {
  // Use a default value of 180s.
  return 180;
}

int GetLocalPredictorPrerenderPriorityHalfLifeTimeSeconds() {
  return 0;
}

int GetLocalPredictorMaxConcurrentPrerenders() {
  return 1;
}

int GetLocalPredictorMaxLaunchPrerenders() {
  return 1;
}

bool SkipLocalPredictorFragment() {
  return false;
}

bool SkipLocalPredictorHTTPS() {
  return false;
}

bool SkipLocalPredictorWhitelist() {
  return false;
}

bool SkipLocalPredictorServiceWhitelist() {
  return false;
}

bool SkipLocalPredictorLoggedIn() {
  return false;
}

bool SkipLocalPredictorDefaultNoPrerender() {
  return false;
}

bool SkipLocalPredictorLocalCandidates() {
  return false;
}

bool SkipLocalPredictorServiceCandidates() {
  return false;
}

}  // namespace prerender
