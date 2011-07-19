// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/crypto_module_password_dialog.h"

#include <pk11pub.h>

#include "base/logging.h"
#include "content/browser/browser_thread.h"
#include "net/base/crypto_module.h"
#include "net/base/x509_certificate.h"

#if defined(OS_CHROMEOS)
#include "crypto/nss_util.h"
#endif

namespace {

bool ShouldShowDialog(const net::CryptoModule* module) {
  // The wincx arg is unused since we don't call PK11_SetIsLoggedInFunc.
  return (PK11_NeedLogin(module->os_module_handle()) &&
          !PK11_IsLoggedIn(module->os_module_handle(), NULL /* wincx */));
}

// Basically an asynchronous implementation of NSS's PK11_DoPassword.
// Note: This currently handles only the simple case.  See the TODOs in
// GotPassword for what is yet unimplemented.
class SlotUnlocker {
 public:
  SlotUnlocker(const net::CryptoModuleList& modules,
               browser::CryptoModulePasswordReason reason,
               const std::string& host,
               Callback0::Type* callback);

  void Start();

 private:
  void GotPassword(const char* password);
  void Done();

  size_t current_;
  net::CryptoModuleList modules_;
  browser::CryptoModulePasswordReason reason_;
  std::string host_;
  Callback0::Type* callback_;
  PRBool retry_;
};

SlotUnlocker::SlotUnlocker(const net::CryptoModuleList& modules,
                           browser::CryptoModulePasswordReason reason,
                           const std::string& host,
                           Callback0::Type* callback)
    : current_(0),
      modules_(modules),
      reason_(reason),
      host_(host),
      callback_(callback),
      retry_(PR_FALSE) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void SlotUnlocker::Start() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (; current_ < modules_.size(); ++current_) {
    if (ShouldShowDialog(modules_[current_].get())) {
#if defined(OS_CHROMEOS)
      if (crypto::IsTPMTokenReady()) {
        std::string token_name;
        std::string user_pin;
        crypto::GetTPMTokenInfo(&token_name, &user_pin);
        if (modules_[current_]->GetTokenName() == token_name) {
          // The user PIN is a well known secret on this machine, and
          // the user didn't set it, so we need to fetch the value and
          // supply it for them here.
          GotPassword(user_pin.c_str());
          return;
        }
      }
#endif
      ShowCryptoModulePasswordDialog(
          modules_[current_]->GetTokenName(),
          retry_,
          reason_,
          host_,
          NewCallback(this, &SlotUnlocker::GotPassword));
      return;
    }
  }
  Done();
}

void SlotUnlocker::GotPassword(const char* password) {
  // TODO(mattm): PK11_DoPassword has something about PK11_Global.verifyPass.
  // Do we need it?
  // http://mxr.mozilla.org/mozilla/source/security/nss/lib/pk11wrap/pk11auth.c#577

  if (!password) {
    // User cancelled entering password.  Oh well.
    ++current_;
    Start();
    return;
  }

  // TODO(mattm): handle protectedAuthPath
  SECStatus rv = PK11_CheckUserPassword(modules_[current_]->os_module_handle(),
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
  // (SECFailure).  Either way we're done with this slot.
  ++current_;
  Start();
}

void SlotUnlocker::Done() {
  DCHECK_EQ(current_, modules_.size());
  callback_->Run();
  delete this;
}

}  // namespace

namespace browser {

void UnlockSlotsIfNecessary(const net::CryptoModuleList& modules,
                            browser::CryptoModulePasswordReason reason,
                            const std::string& host,
                            Callback0::Type* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  for (size_t i = 0; i < modules.size(); ++i) {
    if (ShouldShowDialog(modules[i].get())) {
      (new SlotUnlocker(modules, reason, host, callback))->Start();
      return;
    }
  }
  callback->Run();
}

void UnlockCertSlotIfNecessary(net::X509Certificate* cert,
                               browser::CryptoModulePasswordReason reason,
                               const std::string& host,
                               Callback0::Type* callback) {
  net::CryptoModuleList modules;
  modules.push_back(net::CryptoModule::CreateFromHandle(
      cert->os_cert_handle()->slot));
  UnlockSlotsIfNecessary(modules, reason, host, callback);
}

}  // namespace browser
