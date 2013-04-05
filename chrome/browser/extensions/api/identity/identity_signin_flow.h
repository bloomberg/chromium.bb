// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_SIGNIN_FLOW_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_SIGNIN_FLOW_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace extensions {

// IdentitySigninFlow is a controller class to do a sign-in flow for an
// interactive Identity API call. The UI is launched through the LoginUIService.
// When the flow completes, the delegate is notified, and on success will
// be given an OAuth2 login refresh token.
class IdentitySigninFlow : public content::NotificationObserver {
 public:
  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}
    // Called when the flow has completed successfully.
    virtual void SigninSuccess(const std::string& token) = 0;
    // Called when the flow has failed.
    virtual void SigninFailed() = 0;

    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  IdentitySigninFlow(Delegate* delegate,
                     Profile* profile);
  virtual ~IdentitySigninFlow();

  // Starts the flow. Should only be called once.
  void Start();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  Delegate* delegate_;
  Profile* profile_;
  // Used to listen to notifications from the TokenService.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(IdentitySigninFlow);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_SIGNIN_FLOW_H_
