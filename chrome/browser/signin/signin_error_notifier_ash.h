// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_ERROR_NOTIFIER_ASH_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_ERROR_NOTIFIER_ASH_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "chrome/browser/signin/signin_error_controller.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"

class Profile;

// Shows signin-related errors as notifications in Ash.
class SigninErrorNotifier : public SigninErrorController::Observer,
                            public BrowserContextKeyedService {
 public:
  SigninErrorNotifier(SigninErrorController* controller, Profile* profile);
  virtual ~SigninErrorNotifier();

  // BrowserContextKeyedService:
  virtual void Shutdown() OVERRIDE;

  // SigninErrorController::Observer:
  virtual void OnErrorChanged() OVERRIDE;

 private:
  base::string16 GetMessageTitle() const;
  base::string16 GetMessageBody() const;

  // The error controller to query for error details.
  SigninErrorController* error_controller_;

  // The Profile this service belongs to.
  Profile* profile_;

  // Used to keep track of the message center notification.
  std::string notification_id_;

  DISALLOW_COPY_AND_ASSIGN(SigninErrorNotifier);
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_ERROR_NOTIFIER_ASH_H_
