// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"

OneClickSigninSyncStarter::OneClickSigninSyncStarter(
    const std::string& email,
    const std::string& password,
    Profile* profile,
    bool use_default_settings)
    : profile_(profile),
      signin_tracker_(profile, this),
      use_default_settings_(use_default_settings) {
  DCHECK(profile_);

  SigninManager* manager = SigninManagerFactory::GetForProfile(profile_);
  manager->StartSignInWithCredentials(email, password);
}

OneClickSigninSyncStarter::~OneClickSigninSyncStarter() {
}

void OneClickSigninSyncStarter::GaiaCredentialsValid() {
}

void OneClickSigninSyncStarter::SigninFailed() {
  delete this;
}

void OneClickSigninSyncStarter::SigninSuccess() {
  ProfileSyncService* profile_sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);

  if (use_default_settings_) {
    // Just kick off the sync machine, no need to configure it first.
    profile_sync_service->SetSyncSetupCompleted();
    profile_sync_service->UnsuppressAndStart();
  } else {
    // Give the user a chance to configure things.
    profile_sync_service->ShowConfigure(false);
  }

  delete this;
}
