// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_AUTH_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_AUTH_SERVICE_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/gdata/gdata_errorcode.h"
#include "chrome/browser/chromeos/gdata/gdata_params.h"
#include "chrome/common/net/gaia/oauth2_access_token_fetcher.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace gdata {

class GDataOperationRegistry;

// This class provides authentication for GData based services.
// It integrates specific service integration with OAuth2 stack
// (TokenService) and provides OAuth2 token refresh infrastructure.
// All public functions must be called on UI thread.
class GDataAuthService : public content::NotificationObserver {
 public:
  class Observer {
   public:
    // Triggered when a new OAuth2 refresh token is received from TokenService.
    virtual void OnOAuth2RefreshTokenChanged() = 0;

   protected:
    virtual ~Observer() {}
  };

  GDataAuthService();
  virtual ~GDataAuthService();

  // Adds and removes the observer. AddObserver() should be called before
  // Initialize() as it can change the refresh token.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Initializes the auth service. Starts TokenService to retrieve the
  // refresh token.
  void Initialize(Profile* profile);

  // Starts fetching OAuth2 auth token from the refresh token.
  void StartAuthentication(GDataOperationRegistry* registry,
                           const AuthStatusCallback& callback);

  // True if OAuth2 auth token is retrieved and believed to be fresh.
  bool IsFullyAuthenticated() const { return !auth_token_.empty(); }

  // True if OAuth2 refresh token is present. It's absence means that user
  // is not properly authenticated.
  bool IsPartiallyAuthenticated() const { return !refresh_token_.empty(); }

  // Gets OAuth2 auth token.
  const std::string& oauth2_auth_token() const { return auth_token_; }

  // Clears OAuth2 token.
  void ClearOAuth2Token() { auth_token_.clear(); }

  // Gets OAuth2 refresh token.
  const std::string& GetOAuth2RefreshToken() { return refresh_token_; }

  // Callback for AuthOperation (InternalAuthStatusCallback).
  void OnAuthCompleted(scoped_refptr<base::MessageLoopProxy> relay_proxy,
                       const AuthStatusCallback& callback,
                       GDataErrorCode error,
                       const std::string& auth_token);

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Sets the auth_token as specified.  This should be used only for testing.
  void set_oauth2_auth_token_for_testing(const std::string& token) {
    auth_token_ = token;
  }

 private:
  // Helper function for StartAuthentication() call.
  void StartAuthenticationOnUIThread(
      GDataOperationRegistry* registry,
      scoped_refptr<base::MessageLoopProxy> relay_proxy,
      const AuthStatusCallback& callback);

  Profile* profile_;
  std::string refresh_token_;
  std::string auth_token_;
  ObserverList<Observer> observers_;

  content::NotificationRegistrar registrar_;
  base::WeakPtrFactory<GDataAuthService> weak_ptr_factory_;
  base::WeakPtr<GDataAuthService> weak_ptr_bound_to_ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(GDataAuthService);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_AUTH_SERVICE_H_
