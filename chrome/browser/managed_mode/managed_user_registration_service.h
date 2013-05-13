// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_REGISTRATION_SERVICE_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_REGISTRATION_SERVICE_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_manager.h"

class GoogleServiceAuthError;

// Holds the state necessary for registering a new managed user with the
// management server and associating it with its custodian. It is owned by the
// custodian's profile.
class ManagedUserRegistrationService : public ProfileKeyedService {
 public:
  // Callback for Register() below. If registration is successful, |token| will
  // contain an authentication token for the newly registered managed user,
  // otherwise |token| will be empty and |error| will contain the authentication
  // error for the custodian.
  typedef base::Callback<void(const GoogleServiceAuthError& /* error */,
                              const std::string& /* token */)>
      RegistrationCallback;

  ManagedUserRegistrationService();
  virtual ~ManagedUserRegistrationService();

  // Registers a new managed user with the server. |name| is the display name of
  // the user. |callback| is called with the result of the registration.
  void Register(const string16& name,
                const RegistrationCallback& callback);

  // Convenience method that registers a new managed user with the server and
  // initializes it locally. The callback allows it to be run after a new
  // profile has been created:
  //   ProfileManager::CreateMultiProfileAsync(
  //       name, icon,
  //       managed_user_registration_service->GetRegistrationAndInitCallback(),
  //       desktop_type, managed_user);
  ProfileManager::CreateCallback GetRegistrationAndInitCallback();

 private:
  void DispatchCallback();

  void OnProfileCreated(Profile* profile,
                        Profile::CreateStatus status);

  base::WeakPtrFactory<ManagedUserRegistrationService> weak_ptr_factory_;

  string16 display_name_;
  RegistrationCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ManagedUserRegistrationService);
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_REGISTRATION_SERVICE_H_
