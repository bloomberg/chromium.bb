// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/crypto_module_password_dialog.h"

#include <pk11pub.h>

#include "base/logging.h"
#include "content/browser/browser_thread.h"
#include "net/base/crypto_module.h"
#include "net/base/x509_certificate.h"

namespace {

// Basically an asynchronous implementation of NSS's PK11_DoPassword.
// Note: This currently handles only the simple case.  See the TODOs in
// GotPassword for what is yet unimplemented.
class SlotUnlocker {
 public:
  SlotUnlocker(net::CryptoModule* module,
               browser::CryptoModulePasswordReason reason,
               const std::string& host,
               Callback0::Type* callback);

  void Start();

 private:
  void GotPassword(const char* password);
  void Done();

  scoped_refptr<net::CryptoModule> module_;
  browser::CryptoModulePasswordReason reason_;
  std::string host_;
  Callback0::Type* callback_;
  PRBool retry_;
};

SlotUnlocker::SlotUnlocker(net::CryptoModule* module,
                           browser::CryptoModulePasswordReason reason,
                           const std::string& host,
                           Callback0::Type* callback)
    : module_(module),
      reason_(reason),
      host_(host),
      callback_(callback),
      retry_(PR_FALSE) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void SlotUnlocker::Start() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ShowCryptoModulePasswordDialog(
      module_->GetTokenName(),
      retry_,
      reason_,
      host_,
      NewCallback(this, &SlotUnlocker::GotPassword));
}

void SlotUnlocker::GotPassword(const char* password) {
  // TODO(mattm): PK11_DoPassword has something about PK11_Global.verifyPass.
  // Do we need it?
  // http://mxr.mozilla.org/mozilla/source/security/nss/lib/pk11wrap/pk11auth.c#577

  if (!password) {
    // User cancelled entering password.  Oh well.
    Done();
    return;
  }

  // TODO(mattm): handle protectedAuthPath
  SECStatus rv = PK11_CheckUserPassword(module_->os_module_handle(),
                                        password);
  if (rv == SECWouldBlock) {
    // Incorrect password.  Try again.
    retry_ = PR_TRUE;
    Start();
    return;
  }

  // TODO(mattm): PK11_DoPassword calls nssTrustDomain_UpdateCachedTokenCerts on
  // non-friendly slots.  How important is that?

  // Correct password (SECSuccess) or too many attempts/other failure
  // (SECFailure).  Either way we're done.
  Done();
}

void SlotUnlocker::Done() {
  callback_->Run();
  delete this;
}

}  // namespace

namespace browser {

void UnlockSlotIfNecessary(net::CryptoModule* module,
                           browser::CryptoModulePasswordReason reason,
                           const std::string& host,
                           Callback0::Type* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // The wincx arg is unused since we don't call PK11_SetIsLoggedInFunc.
  if (PK11_NeedLogin(module->os_module_handle()) &&
      !PK11_IsLoggedIn(module->os_module_handle(), NULL /* wincx */)) {
    (new SlotUnlocker(module, reason, host, callback))->Start();
  } else {
    callback->Run();
  }
}

void UnlockCertSlotIfNecessary(net::X509Certificate* cert,
                               browser::CryptoModulePasswordReason reason,
                               const std::string& host,
                               Callback0::Type* callback) {
  scoped_refptr<net::CryptoModule> module(net::CryptoModule::CreateFromHandle(
      cert->os_cert_handle()->slot));
  UnlockSlotIfNecessary(module.get(), reason, host, callback);
}

}  // namespace browser
