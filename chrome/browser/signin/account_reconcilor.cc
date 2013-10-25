// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_reconcilor.h"
#include "chrome/browser/signin/google_auto_login_helper.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

AccountReconcilor::AccountReconcilor(Profile* profile) : profile_(profile) {
  RegisterWithSigninManager();

  // If this profile is not connected, the reconcilor should do nothing but
  // wait for the connection.
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile_);
  if (!signin_manager->GetAuthenticatedUsername().empty()) {
    RegisterWithTokenService();
    StartPeriodicReconciliation();
  }
}

void AccountReconcilor::RegisterWithSigninManager() {
  content::Source<Profile> source(profile_);
  registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL, source);
  registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_SIGNED_OUT, source);
}

void AccountReconcilor::UnregisterWithSigninManager() {
  registrar_.RemoveAll();
}

void AccountReconcilor::RegisterWithTokenService() {
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  token_service->AddObserver(this);
}

void AccountReconcilor::UnregisterWithTokenService() {
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  token_service->RemoveObserver(this);
}

void AccountReconcilor::StartPeriodicReconciliation() {
  // TODO(rogerta): pick appropriate thread and timeout value.
  reconciliation_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMinutes(5),
      this,
      &AccountReconcilor::PeriodicReconciliation);
}

void AccountReconcilor::StopPeriodicReconciliation() {
  reconciliation_timer_.Stop();
}

void AccountReconcilor::PeriodicReconciliation() {
  PerformReconcileAction();
}

void AccountReconcilor::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL:
      RegisterWithTokenService();
      StartPeriodicReconciliation();
      break;
    case chrome::NOTIFICATION_GOOGLE_SIGNED_OUT:
      UnregisterWithTokenService();
      StopPeriodicReconciliation();
      break;
    default:
      NOTREACHED();
      break;
  }
}

void AccountReconcilor::OnRefreshTokenAvailable(const std::string& account_id) {
  PerformMergeAction(account_id);
}

void AccountReconcilor::OnRefreshTokenRevoked(const std::string& account_id) {
  PerformRemoveAction(account_id);
}

void AccountReconcilor::OnRefreshTokensLoaded() {}

void AccountReconcilor::PerformMergeAction(const std::string& account_id) {
  // GoogleAutoLoginHelper deletes itself upon success / failure.
  GoogleAutoLoginHelper* helper = new GoogleAutoLoginHelper(profile_);
  helper->LogIn(account_id);
}

void AccountReconcilor::PerformRemoveAction(const std::string& account_id) {
  // TODO(acleung): Implement this:
}

void AccountReconcilor::PerformReconcileAction() {
  // TODO(acleung): Implement this:
}

AccountReconcilor::~AccountReconcilor() {
  // Make sure shutdown was called first.
  DCHECK(registrar_.IsEmpty());
}

void AccountReconcilor::Shutdown() {
  UnregisterWithSigninManager();
  UnregisterWithTokenService();
  StopPeriodicReconciliation();
}
