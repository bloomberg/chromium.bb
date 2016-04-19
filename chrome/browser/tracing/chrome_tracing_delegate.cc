// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/chrome_tracing_delegate.h"

#include <utility>

#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/tracing/crash_service_uploader.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/trace_event_args_whitelist.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/active_field_trials.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/browser_thread.h"

namespace {

const int kMinDaysUntilNextUpload = 7;

}  // namespace

void ChromeTracingDelegate::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kBackgroundTracingLastUpload, 0);
}

ChromeTracingDelegate::ChromeTracingDelegate() : incognito_launched_(false) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  BrowserList::AddObserver(this);
}

ChromeTracingDelegate::~ChromeTracingDelegate() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  BrowserList::RemoveObserver(this);
}

void ChromeTracingDelegate::OnBrowserAdded(Browser* browser) {
  if (browser->profile()->IsOffTheRecord())
    incognito_launched_ = true;
}

std::unique_ptr<content::TraceUploader> ChromeTracingDelegate::GetTraceUploader(
    net::URLRequestContextGetter* request_context) {
  return std::unique_ptr<content::TraceUploader>(
      new TraceCrashServiceUploader(request_context));
}

namespace {

enum PermitMissingProfile { PROFILE_REQUIRED, PROFILE_NOT_REQUIRED };

bool ProfileAllowsScenario(const content::BackgroundTracingConfig& config,
                           PermitMissingProfile profile_permission) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return false;

  Profile* profile = profile_manager->GetProfileByPath(
      profile_manager->GetLastUsedProfileDir(profile_manager->user_data_dir()));

  // If the profile hasn't loaded or been created yet, we allow the scenario
  // to start up, but not be finalized.
  if (!profile) {
    if (profile_permission == PROFILE_REQUIRED)
      return false;
    else
      return true;
  }

  // Safeguard, in case background tracing is responsible for a crash on
  // startup.
  if (profile->GetLastSessionExitType() == Profile::EXIT_CRASHED)
    return false;

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

#if !defined(OS_CHROMEOS)
  if (!local_state->GetBoolean(metrics::prefs::kMetricsReportingEnabled))
    return false;
#endif // OS_CHROMEOS

  if (config.tracing_mode() == content::BackgroundTracingConfig::PREEMPTIVE) {
    const base::Time last_upload_time = base::Time::FromInternalValue(
        local_state->GetInt64(prefs::kBackgroundTracingLastUpload));
    if (!last_upload_time.is_null()) {
      base::Time computed_next_allowed_time =
          last_upload_time + base::TimeDelta::FromDays(kMinDaysUntilNextUpload);
      if (computed_next_allowed_time > base::Time::Now()) {
        return false;
      }
    }
  }

  return true;
}

}  // namespace

bool ChromeTracingDelegate::IsAllowedToBeginBackgroundScenario(
    const content::BackgroundTracingConfig& config,
    bool requires_anonymized_data) {
  if (!ProfileAllowsScenario(config, PROFILE_NOT_REQUIRED))
    return false;

  if (requires_anonymized_data && chrome::IsOffTheRecordSessionActive())
    return false;

  return true;
}

bool ChromeTracingDelegate::IsAllowedToEndBackgroundScenario(
    const content::BackgroundTracingConfig& config,
    bool requires_anonymized_data) {
  if (requires_anonymized_data && incognito_launched_)
    return false;

  if (!ProfileAllowsScenario(config, PROFILE_REQUIRED))
    return false;

  if (config.tracing_mode() == content::BackgroundTracingConfig::PREEMPTIVE) {
    PrefService* local_state = g_browser_process->local_state();
    DCHECK(local_state);
    local_state->SetInt64(prefs::kBackgroundTracingLastUpload,
                          base::Time::Now().ToInternalValue());

    // We make sure we've persisted the value in case finalizing the tracing
    // causes a crash.
    local_state->CommitPendingWrite();
  }

  return true;
}

void ChromeTracingDelegate::GenerateMetadataDict(
    base::DictionaryValue* metadata_dict) {
  DCHECK(metadata_dict);
  std::vector<std::string> variations;
  variations::GetFieldTrialActiveGroupIdsAsStrings(&variations);

  std::unique_ptr<base::ListValue> variations_list(new base::ListValue());
  for (const auto& it : variations)
    variations_list->Append(new base::StringValue(it));

  metadata_dict->Set("field-trials", std::move(variations_list));
}

content::MetadataFilterPredicate
ChromeTracingDelegate::GetMetadataFilterPredicate() {
  return base::Bind(&IsMetadataWhitelisted);
}
