// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/sync/one_click_signin_histogram.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"

OneClickSigninSyncStarter::OneClickSigninSyncStarter(
    Profile* profile,
    const std::string& session_index,
    const std::string& email,
    const std::string& password,
    bool use_default_settings)
    : profile_(profile),
      signin_tracker_(profile, this),
      use_default_settings_(use_default_settings) {
  DCHECK(profile_);

  int action = use_default_settings ? one_click_signin::HISTOGRAM_WITH_DEFAULTS
                                    : one_click_signin::HISTOGRAM_WITH_ADVANCED;
  UMA_HISTOGRAM_ENUMERATION("AutoLogin.Reverse", action,
                            one_click_signin::HISTOGRAM_MAX);

  ProfileSyncService* profile_sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  // Let the sync service know that setup is in progress so it doesn't start
  // syncing until the user has finished any configuration.
  profile_sync_service->set_setup_in_progress(true);
  SigninManager* manager = SigninManagerFactory::GetForProfile(profile_);
  manager->StartSignInWithCredentials(session_index, email, password);
}

OneClickSigninSyncStarter::~OneClickSigninSyncStarter() {
}

void OneClickSigninSyncStarter::GaiaCredentialsValid() {
}

void OneClickSigninSyncStarter::SigninFailed(
    const GoogleServiceAuthError& error) {
  ProfileSyncService* profile_sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  profile_sync_service->set_setup_in_progress(false);
  delete this;
}

void OneClickSigninSyncStarter::SigninSuccess() {
  ProfileSyncService* profile_sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);

  if (use_default_settings_) {
    // Just kick off the sync machine, no need to configure it first.
    profile_sync_service->SetSyncSetupCompleted();
    profile_sync_service->set_setup_in_progress(false);
    profile_sync_service->UnsuppressAndStart();
  } else {
    // Give the user a chance to configure things. We don't clear the
    // ProfileSyncService::setup_in_progress flag because we don't want sync
    // to start up until after the configure UI is displayed (the configure UI
    // will clear the flag when the user is done setting up sync).
    LoginUIServiceFactory::GetForProfile(profile_)->ShowLoginUI(false);
  }

  delete this;
}
