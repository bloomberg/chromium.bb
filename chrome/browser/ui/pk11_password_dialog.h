// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PK11_PASSWORD_DIALOG_H_
#define CHROME_BROWSER_UI_PK11_PASSWORD_DIALOG_H_
#pragma once

#include <string>

#include "base/callback.h"

namespace base {
class PK11BlockingPasswordDelegate;
}

namespace net {
class CryptoModule;
class X509Certificate;
}

namespace browser {

// An enum to describe the reason for the password request.
enum PK11PasswordReason {
  kPK11PasswordKeygen,
  kPK11PasswordCertEnrollment,
  kPK11PasswordClientAuth,
  kPK11PasswordCertImport,
  kPK11PasswordCertExport,
};

typedef Callback1<const char*>::Type PK11PasswordCallback;

// Display a dialog, prompting the user to authenticate to unlock
// |module|. |reason| describes the purpose of the authentication and
// affects the message displayed in the dialog. |server| is the name
// of the server which requested the access.
void ShowPK11PasswordDialog(const std::string& module_name,
                            bool retry,
                            PK11PasswordReason reason,
                            const std::string& server,
                            PK11PasswordCallback* callback);

// Returns a PK11BlockingPasswordDelegate to open a dialog and block
// until returning. Should only be used on a worker thread.
base::PK11BlockingPasswordDelegate* NewPK11BlockingDialogDelegate(
    PK11PasswordReason reason,
    const std::string& server);

// Asynchronously unlock |module|, if necessary.  |callback| is called when done
// (regardless if module was successfully unlocked or not).  Should only be
// called on UI thread.
void UnlockSlotIfNecessary(net::CryptoModule* module,
                           browser::PK11PasswordReason reason,
                           const std::string& server,
                           Callback0::Type* callback);

// Asynchronously unlock the |cert|'s module, if necessary.  |callback| is
// called when done (regardless if module was successfully unlocked or not).
// Should only be called on UI thread.
void UnlockCertSlotIfNecessary(net::X509Certificate* cert,
                               browser::PK11PasswordReason reason,
                               const std::string& server,
                               Callback0::Type* callback);

}  // namespace browser

#endif  // CHROME_BROWSER_UI_PK11_PASSWORD_DIALOG_H_
