// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_SIGNIN_ACCOUNT_RECONCILOR_H_
#define CHROME_BROWSER_SIGNIN_ACCOUNT_RECONCILOR_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "google_apis/gaia/oauth2_token_service.h"

class Profile;
struct ChromeCookieDetails;

class AccountReconcilor : public BrowserContextKeyedService,
                                 content::NotificationObserver,
                                 OAuth2TokenService::Observer {
 public:
  explicit AccountReconcilor(Profile* profile);
  virtual ~AccountReconcilor();

  // BrowserContextKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  Profile* profile() { return profile_; }

  bool IsPeriodicReconciliationRunning() {
    return reconciliation_timer_.IsRunning();
  }

 private:
  // Register and unregister with dependent services.
  void RegisterWithCookieMonster();
  void UnregisterWithCookieMonster();
  void RegisterWithSigninManager();
  void UnregisterWithSigninManager();
  void RegisterWithTokenService();
  void UnregisterWithTokenService();

  // Start and stop the periodic reconciliation.
  void StartPeriodicReconciliation();
  void StopPeriodicReconciliation();
  void PeriodicReconciliation();

  // The profile that this reconcilor belongs to.
  Profile* profile_;
  content::NotificationRegistrar registrar_;
  base::RepeatingTimer<AccountReconcilor> reconciliation_timer_;

  void PerformMergeAction(const std::string& account_id);
  void PerformRemoveAction(const std::string& account_id);
  void PerformReconcileAction();

  // Overriden from content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void OnCookieChanged(ChromeCookieDetails* details);

  // Overriden from OAuth2TokenService::Observer
  virtual void OnRefreshTokenAvailable(const std::string& account_id) OVERRIDE;
  virtual void OnRefreshTokenRevoked(const std::string& account_id) OVERRIDE;
  virtual void OnRefreshTokensLoaded() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AccountReconcilor);
};

#endif  // CHROME_BROWSER_SIGNIN_ACCOUNT_RECONCILOR_H_
