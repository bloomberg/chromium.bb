// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INVALIDATION_INVALIDATION_AUTH_PROVIDER_H_
#define CHROME_BROWSER_INVALIDATION_INVALIDATION_AUTH_PROVIDER_H_

#include <string>

#include "base/macros.h"
#include "base/observer_list.h"

class OAuth2TokenService;

namespace invalidation {

// Encapsulates authentication-related dependencies of InvalidationService
// implementations so different implementations can be used for regular Profiles
// and Chrome OS Kiosk Apps.
class InvalidationAuthProvider {
 public:
  class Observer {
   public:
    virtual ~Observer();

    // Called when the user logs out.
    virtual void OnInvalidationAuthLogout() = 0;
  };

  virtual ~InvalidationAuthProvider();

  // Gets the token service vending tokens for authentication to the cloud.
  virtual OAuth2TokenService* GetTokenService() = 0;

  // Gets the account ID to use for authentication.
  virtual std::string GetAccountId() = 0;

  // Shows the login popup, returns true if successful.
  virtual bool ShowLoginUI() = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  InvalidationAuthProvider();

  // Fires an OnInvalidationAuthLogout notification.
  void FireInvalidationAuthLogout();

 private:
  ObserverList<Observer, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(InvalidationAuthProvider);
};

}  // namespace invalidation

#endif  // CHROME_BROWSER_INVALIDATION_INVALIDATION_AUTH_PROVIDER_H_
