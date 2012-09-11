// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_AUTH_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_AUTH_SERVICE_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/google_apis/operations_base.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace gdata {

class OperationRegistry;

// This class provides authentication for Google services.
// It integrates specific service integration with OAuth2 stack
// (TokenService) and provides OAuth2 token refresh infrastructure.
// All public functions must be called on UI thread.
class AuthService : public content::NotificationObserver {
 public:
  class Observer {
   public:
    // Triggered when a new OAuth2 refresh token is received from TokenService.
    virtual void OnOAuth2RefreshTokenChanged() = 0;

   protected:
    virtual ~Observer() {}
  };

  explicit AuthService(const std::vector<std::string>& scopes);
  virtual ~AuthService();

  // Adds and removes the observer. AddObserver() should be called before
  // Initialize() as it can change the refresh token.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Initializes the auth service. Starts TokenService to retrieve the
  // refresh token.
  void Initialize(Profile* profile);

  // Starts fetching OAuth2 auth token from the refresh token for |scopes_|.
  void StartAuthentication(OperationRegistry* registry,
                           const AuthStatusCallback& callback);

  // True if an OAuth2 access token is retrieved and believed to be fresh.
  // The access token is used to access the Drive server.
  bool HasAccessToken() const { return !access_token_.empty(); }

  // True if an OAuth2 refresh token is present. Its absence means that user
  // is not properly authenticated.
  // The refresh token is used to get the access token.
  bool HasRefreshToken() const { return !refresh_token_.empty(); }

  // Returns OAuth2 access token.
  const std::string& access_token() const { return access_token_; }

  // Clears OAuth2 access token.
  void ClearAccessToken() { access_token_.clear(); }

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Sets the access_token as specified.  This should be used only for testing.
  void set_access_token_for_testing(const std::string& token) {
    access_token_ = token;
  }

 private:
  // Helper function for StartAuthentication() call.
  void StartAuthenticationOnUIThread(OperationRegistry* registry,
                                     const AuthStatusCallback& callback);

  // Callback for AuthOperation (InternalAuthStatusCallback).
  void OnAuthCompleted(const AuthStatusCallback& callback,
                       GDataErrorCode error,
                       const std::string& access_token);

  Profile* profile_;
  std::string refresh_token_;
  std::string access_token_;
  std::vector<std::string> scopes_;
  ObserverList<Observer> observers_;

  content::NotificationRegistrar registrar_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<AuthService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AuthService);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_AUTH_SERVICE_H_
