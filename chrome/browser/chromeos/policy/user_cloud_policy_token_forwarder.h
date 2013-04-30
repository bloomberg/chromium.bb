// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_TOKEN_FORWARDER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_TOKEN_FORWARDER_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class TokenService;

namespace policy {

class UserCloudPolicyManagerChromeOS;

// A PKS that observes a TokenService and passes its refresh token to a
// UserCloudPolicyManagerChromeOS, when it becomes available. This service
// decouples the UserCloudPolicyManagerChromeOS from depending directly on the
// TokenService, since it is initialized much earlier.
class UserCloudPolicyTokenForwarder : public ProfileKeyedService,
                                      public content::NotificationObserver {
 public:
  // The factory of this PKS depends on the factories of these two arguments,
  // so this object will be Shutdown() first and these pointers can be used
  // until that point.
  UserCloudPolicyTokenForwarder(UserCloudPolicyManagerChromeOS* manager,
                                TokenService* token_service);
  virtual ~UserCloudPolicyTokenForwarder();

  // ProfileKeyedService:
  virtual void Shutdown() OVERRIDE;

  // NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  UserCloudPolicyManagerChromeOS* manager_;
  TokenService* token_service_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyTokenForwarder);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_TOKEN_FORWARDER_H_
