// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/crypto_module_password_dialog.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/crypto_module_blocking_password_delegate.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace chrome {

namespace {

class CryptoModuleBlockingDialogDelegate
    : public crypto::CryptoModuleBlockingPasswordDelegate {
 public:
  CryptoModuleBlockingDialogDelegate(CryptoModulePasswordReason reason,
                                     const std::string& server)
      : event_(false, false),
        reason_(reason),
        server_(server),
        cancelled_(false) {
  }

  virtual ~CryptoModuleBlockingDialogDelegate() {
    // Make sure we clear the password in memory.
    password_.replace(0, password_.size(), password_.size(), 0);
  }

  // crypto::CryptoModuleBlockingDialogDelegate implementation.
  virtual std::string RequestPassword(const std::string& slot_name,
                                      bool retry,
                                      bool* cancelled) OVERRIDE {
    DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(!event_.IsSignaled());
    event_.Reset();

    if (BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::Bind(&CryptoModuleBlockingDialogDelegate::ShowDialog,
                       // We block on event_ until the task completes, so
                       // there's no need to ref-count.
                       base::Unretained(this),
                       slot_name,
                       retry))) {
      event_.Wait();
    }
    *cancelled = cancelled_;
    return password_;
  }

 private:
  void ShowDialog(const std::string& slot_name,
                  bool retry) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    ShowCryptoModulePasswordDialog(
        slot_name,
        retry,
        reason_,
        server_,
        NULL,  // TODO(mattm): Supply parent window.
        base::Bind(&CryptoModuleBlockingDialogDelegate::GotPassword,
                   // We block on event_ until the task completes, so
                   // there's no need to ref-count.
                   base::Unretained(this)));
  }

  void GotPassword(const char* password) {
    if (password)
      password_ = password;
    else
      cancelled_ = true;
    event_.Signal();
  }

  base::WaitableEvent event_;
  CryptoModulePasswordReason reason_;
  std::string server_;
  std::string password_;
  bool cancelled_;

  DISALLOW_COPY_AND_ASSIGN(CryptoModuleBlockingDialogDelegate);
};

}  // namespace

crypto::CryptoModuleBlockingPasswordDelegate*
    NewCryptoModuleBlockingDialogDelegate(CryptoModulePasswordReason reason,
                                          const std::string& server) {
  return new CryptoModuleBlockingDialogDelegate(reason, server);
}

}  // namespace chrome
