// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/signin_status_metrics_provider.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "components/signin/core/browser/signin_manager.h"

namespace {

// The event of calling function ComputeCurrentSigninStatus and the errors
// occurred during the function execution.
enum ComputeSigninStatus {
  ENTERED_COMPUTE_SIGNIN_STATUS,
  ERROR_COMPUTE_SIGNIN_STATUS,
  COMPUTE_SIGNIN_STATUS_MAX,
};

void RecordComputeSigninStatusHistogram(ComputeSigninStatus status) {
  UMA_HISTOGRAM_ENUMERATION("UMA.ComputeCurrentSigninStatus", status,
                            COMPUTE_SIGNIN_STATUS_MAX);
}

}  // namespace

SigninStatusMetricsProvider::SigninStatusMetricsProvider(bool is_test)
    : signin_status_(UNKNOWN_SIGNIN_STATUS),
      scoped_observer_(this),
      is_test_(is_test),
      weak_ptr_factory_(this) {
  if (is_test_)
    return;

  // Postpone the initialization until all threads are created.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&SigninStatusMetricsProvider::Initialize,
                 weak_ptr_factory_.GetWeakPtr()));
}

SigninStatusMetricsProvider::~SigninStatusMetricsProvider() {
  if (is_test_)
    return;

#if !defined(OS_ANDROID)
  BrowserList::RemoveObserver(this);
#endif

  SigninManagerFactory* factory = SigninManagerFactory::GetInstance();
  if (factory)
    factory->RemoveObserver(this);
}

void SigninStatusMetricsProvider::RecordSigninStatusHistogram() {
  UMA_HISTOGRAM_ENUMERATION(
      "UMA.ProfileSignInStatus", signin_status_, SIGNIN_STATUS_MAX);
  // After a histogram value is recorded, a new UMA session will be started, so
  // we need to re-check the current sign-in status regardless of the previous
  // recorded |signin_status_| value.
  ComputeCurrentSigninStatus();
}

// static
SigninStatusMetricsProvider* SigninStatusMetricsProvider::CreateInstance() {
  return new SigninStatusMetricsProvider(false);
}

void SigninStatusMetricsProvider::OnBrowserAdded(Browser* browser) {
  if (signin_status_ == MIXED_SIGNIN_STATUS)
    return;

  SigninManager* manager = SigninManagerFactory::GetForProfile(
      browser->profile());

  // Nothing will change if the opened browser is in incognito mode.
  if (!manager)
    return;

  const bool signed_in = !manager->GetAuthenticatedUsername().empty();
  UpdateStatusWhenBrowserAdded(signed_in);
}

void SigninStatusMetricsProvider::SigninManagerCreated(
    SigninManagerBase* manager) {
  // Whenever a new profile is created, a new SigninManagerBase will be created
  // for it. This ensures that all sign-in or sign-out actions of all opened
  // profiles are being monitored.
  scoped_observer_.Add(manager);

  // If the status is unknown, it means this is the first created
  // SigninManagerBase and the corresponding profile should be the only opened
  // profile.
  if (signin_status_ == UNKNOWN_SIGNIN_STATUS) {
    size_t signed_in_count =
        manager->GetAuthenticatedUsername().empty() ? 0 : 1;
    UpdateInitialSigninStatus(1, signed_in_count);
  }
}

void SigninStatusMetricsProvider::SigninManagerShutdown(
    SigninManagerBase* manager) {
  if (scoped_observer_.IsObserving(manager))
    scoped_observer_.Remove(manager);
}

void SigninStatusMetricsProvider::GoogleSigninSucceeded(
    const std::string& username,
    const std::string& password) {
  if (signin_status_ == ALL_PROFILES_NOT_SIGNED_IN)
    signin_status_ = MIXED_SIGNIN_STATUS;
}

void SigninStatusMetricsProvider::GoogleSignedOut(const std::string& username) {
  if (signin_status_ == ALL_PROFILES_SIGNED_IN)
    signin_status_ = MIXED_SIGNIN_STATUS;
}

void SigninStatusMetricsProvider::Initialize() {
  // Add observers.
#if !defined(OS_ANDROID)
  // On Android, there is always only one profile in any situation, opening new
  // windows (which is possible with only some Android devices) will not change
  // the opened profiles signin status.
  BrowserList::AddObserver(this);
#endif
  SigninManagerFactory::GetInstance()->AddObserver(this);

  // Start observing all already-created SigninManagers.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  std::vector<Profile*> profiles = profile_manager->GetLoadedProfiles();
  for (size_t i = 0; i < profiles.size(); ++i) {
    SigninManager* manager = SigninManagerFactory::GetForProfileIfExists(
        profiles[i]);
    if (manager) {
      DCHECK(!scoped_observer_.IsObserving(manager));
      scoped_observer_.Add(manager);
    }
  }

  // It is possible that when this object is created, no SigninManager is
  // created yet, for example, when Chrome is opened for the first time after
  // installation on desktop, or when Chrome on Android is loaded into memory.
  if (profiles.empty()) {
    signin_status_ = UNKNOWN_SIGNIN_STATUS;
  } else {
    ComputeCurrentSigninStatus();
  }
}

void SigninStatusMetricsProvider::UpdateInitialSigninStatus(
    size_t total_count,
    size_t signed_in_profiles_count) {
  RecordComputeSigninStatusHistogram(ENTERED_COMPUTE_SIGNIN_STATUS);

  if (total_count == 0) {
    // This should never happen. If it does, record it in histogram.
    RecordComputeSigninStatusHistogram(ERROR_COMPUTE_SIGNIN_STATUS);
    signin_status_ = UNKNOWN_SIGNIN_STATUS;
  } else if (signed_in_profiles_count == 0) {
    signin_status_ = ALL_PROFILES_NOT_SIGNED_IN;
  } else if (total_count == signed_in_profiles_count) {
    signin_status_ = ALL_PROFILES_SIGNED_IN;
  } else {
    signin_status_ = MIXED_SIGNIN_STATUS;
  }
}

void SigninStatusMetricsProvider::UpdateStatusWhenBrowserAdded(bool signed_in) {
#if !defined(OS_ANDROID)
  if ((signin_status_ == ALL_PROFILES_NOT_SIGNED_IN && signed_in) ||
      (signin_status_ == ALL_PROFILES_SIGNED_IN && !signed_in)) {
    signin_status_ = MIXED_SIGNIN_STATUS;
  }
#endif
}

void SigninStatusMetricsProvider::ComputeCurrentSigninStatus() {
  // Get the sign-in status of all currently open profiles. Sign-in status is
  // indicated by its username. When username is not empty, the profile is
  // signed-in.
  std::vector<Profile*> profile_list = ProfileManager::GetLastOpenedProfiles();
  size_t signed_in_profiles_count = 0;
  for (size_t i = 0; i < profile_list.size(); ++i) {
    SigninManager* manager = SigninManagerFactory::GetForProfile(
        profile_list[i]->GetOriginalProfile());
    if (manager && !manager->GetAuthenticatedUsername().empty())
      signed_in_profiles_count++;
  }
  UpdateInitialSigninStatus(profile_list.size(), signed_in_profiles_count);
}

SigninStatusMetricsProvider::ProfilesSigninStatus
SigninStatusMetricsProvider::GetSigninStatusForTesting() {
  return signin_status_;
}
