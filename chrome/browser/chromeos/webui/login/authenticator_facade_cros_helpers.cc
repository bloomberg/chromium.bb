// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/browser_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/webui/login/authenticator_facade_cros_helpers.h"

namespace chromeos {

AuthenticatorFacadeCrosHelpers::AuthenticatorFacadeCrosHelpers() {
}

Authenticator* AuthenticatorFacadeCrosHelpers::CreateAuthenticator(
    LoginStatusConsumer* consumer) {

  return CrosLibrary::Get()->EnsureLoaded() ?
      LoginUtils::Get()->CreateAuthenticator(consumer) :
      NULL;
}

void AuthenticatorFacadeCrosHelpers::PostAuthenticateToLogin(
    Authenticator* authenticator,
    Profile* profile,
    const std::string& username,
    const std::string& password) {

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(authenticator,
                        &Authenticator::AuthenticateToLogin,
                        profile,
                        username,
                        password,
                        std::string(),
                        std::string()));
}

}  // namespace chromeos
