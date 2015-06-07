// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/chrome_tracing_delegate.h"

#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/tracing/crash_service_uploader.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/common/pref_names.h"
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

scoped_ptr<content::TraceUploader> ChromeTracingDelegate::GetTraceUploader(
    net::URLRequestContextGetter* request_context) {
  return scoped_ptr<content::TraceUploader>(
      new TraceCrashServiceUploader(request_context));
}

bool ChromeTracingDelegate::IsAllowedToBeginBackgroundScenario(
    const content::BackgroundTracingConfig& config,
    bool requires_anonymized_data) {
  Profile* profile = g_browser_process->profile_manager()
                         ->GetLastUsedProfile()
                         ->GetOriginalProfile();
  DCHECK(profile);
  // Safeguard, in case background tracing is responsible for a crash on
  // startup.
  if (profile->GetLastSessionExitType() == Profile::EXIT_CRASHED)
    return false;

  if (requires_anonymized_data && chrome::IsOffTheRecordSessionActive())
    return false;

  if (config.mode ==
      content::BackgroundTracingConfig::PREEMPTIVE_TRACING_MODE) {
    PrefService* local_state = g_browser_process->local_state();
    DCHECK(local_state);
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

bool ChromeTracingDelegate::IsAllowedToEndBackgroundScenario(
    const content::BackgroundTracingConfig& config,
    bool requires_anonymized_data) {
  if (requires_anonymized_data && incognito_launched_)
    return false;

  if (config.mode ==
      content::BackgroundTracingConfig::PREEMPTIVE_TRACING_MODE) {
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
