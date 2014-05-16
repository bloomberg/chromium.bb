// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_AUTH_SYNC_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_AUTH_SYNC_OBSERVER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace chromeos {

// This class is responsible for detecting authentication problems reported
// by sync service and
class AuthSyncObserver : public KeyedService,
                         public ProfileSyncServiceObserver {
 public:
  explicit AuthSyncObserver(Profile* user_profile);
  virtual ~AuthSyncObserver();

  void StartObserving();

 private:
  friend class AuthSyncObserverFactory;

  // KeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // ProfileSyncServiceObserver implementation.
  virtual void OnStateChanged() OVERRIDE;

  // Called on attempt to restore supervised user token.
  void OnSupervisedTokenLoaded(const std::string& token);

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(AuthSyncObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_AUTH_SYNC_OBSERVER_H_
