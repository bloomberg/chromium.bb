// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace ios {
class ChromeBrowserState;
}

class AuthenticationService;

// Singleton that owns all |AuthenticationServices| and associates them with
// browser states. Listens for the |BrowserState|'s destruction notification and
// cleans up the associated |AuthenticationService|.
class AuthenticationServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static AuthenticationService* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);
  static AuthenticationService* GetForBrowserStateIfExists(
      ios::ChromeBrowserState* browser_state);
  static AuthenticationServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<AuthenticationServiceFactory>;

  AuthenticationServiceFactory();
  ~AuthenticationServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  DISALLOW_COPY_AND_ASSIGN(AuthenticationServiceFactory);
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_FACTORY_H_
