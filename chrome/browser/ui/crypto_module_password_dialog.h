// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CRYPTO_MODULE_PASSWORD_DIALOG_H_
#define CHROME_BROWSER_UI_CRYPTO_MODULE_PASSWORD_DIALOG_H_
#pragma once

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/ref_counted.h"

namespace base {
class CryptoModuleBlockingPasswordDelegate;
}

namespace net {
class CryptoModule;
typedef std::vector<scoped_refptr<CryptoModule> > CryptoModuleList;
class X509Certificate;
}

namespace browser {

// An enum to describe the reason for the password request.
enum CryptoModulePasswordReason {
  kCryptoModulePasswordKeygen,
  kCryptoModulePasswordCertEnrollment,
  kCryptoModulePasswordClientAuth,
  kCryptoModulePasswordListCerts,
  kCryptoModulePasswordCertImport,
  kCryptoModulePasswordCertExport,
};

typedef Callback1<const char*>::Type CryptoModulePasswordCallback;

// Display a dialog, prompting the user to authenticate to unlock
// |module|. |reason| describes the purpose of the authentication and
// affects the message displayed in the dialog. |server| is the name
// of the server which requested the access.
void ShowCryptoModulePasswordDialog(const std::string& module_name,
                            bool retry,
                            CryptoModulePasswordReason reason,
                            const std::string& server,
                            CryptoModulePasswordCallback* callback);

// Returns a CryptoModuleBlockingPasswordDelegate to open a dialog and block
// until returning. Should only be used on a worker thread.
base::CryptoModuleBlockingPasswordDelegate*
    NewCryptoModuleBlockingDialogDelegate(
        CryptoModulePasswordReason reason,
        const std::string& server);

// Asynchronously unlock |modules|, if necessary.  |callback| is called when
// done (regardless if any modules were successfully unlocked or not).  Should
// only be called on UI thread.
void UnlockSlotsIfNecessary(const net::CryptoModuleList& modules,
                            browser::CryptoModulePasswordReason reason,
                            const std::string& server,
                            Callback0::Type* callback);

// Asynchronously unlock the |cert|'s module, if necessary.  |callback| is
// called when done (regardless if module was successfully unlocked or not).
// Should only be called on UI thread.
void UnlockCertSlotIfNecessary(net::X509Certificate* cert,
                               browser::CryptoModulePasswordReason reason,
                               const std::string& server,
                               Callback0::Type* callback);

}  // namespace browser

#endif  // CHROME_BROWSER_UI_CRYPTO_MODULE_PASSWORD_DIALOG_H_
