// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/crypto_module_delegate_nss.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "chrome/browser/net/nss_context.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

void CreateWithSlot(chrome::CryptoModulePasswordReason reason,
                    const net::HostPortPair& server,
                    const base::Callback<void(
                        scoped_ptr<ChromeNSSCryptoModuleDelegate>)>& callback,
                    crypto::ScopedPK11Slot slot) {
  if (!slot) {
    callback.Run(scoped_ptr<ChromeNSSCryptoModuleDelegate>());
    return;
  }
  callback.Run(scoped_ptr<ChromeNSSCryptoModuleDelegate>(
      new ChromeNSSCryptoModuleDelegate(reason, server, slot.Pass())));
}

}  // namespace

ChromeNSSCryptoModuleDelegate::ChromeNSSCryptoModuleDelegate(
    chrome::CryptoModulePasswordReason reason,
    const net::HostPortPair& server,
    crypto::ScopedPK11Slot slot)
    : reason_(reason),
      server_(server),
      event_(false, false),
      cancelled_(false),
      slot_(slot.Pass()) {
}

ChromeNSSCryptoModuleDelegate::~ChromeNSSCryptoModuleDelegate() {}

// static
void ChromeNSSCryptoModuleDelegate::CreateForResourceContext(
    chrome::CryptoModulePasswordReason reason,
    const net::HostPortPair& server,
    content::ResourceContext* context,
    const base::Callback<void(scoped_ptr<ChromeNSSCryptoModuleDelegate>)>&
        callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  base::Callback<void(crypto::ScopedPK11Slot)> get_slot_callback =
      base::Bind(&CreateWithSlot, reason, server, callback);

  crypto::ScopedPK11Slot slot =
      GetPrivateNSSKeySlotForResourceContext(context, get_slot_callback);
  if (slot)
    get_slot_callback.Run(slot.Pass());
}

// TODO(mattm): allow choosing which slot to generate and store the key.
crypto::ScopedPK11Slot ChromeNSSCryptoModuleDelegate::RequestSlot() {
  return slot_.Pass();
}

std::string ChromeNSSCryptoModuleDelegate::RequestPassword(
    const std::string& slot_name,
    bool retry,
    bool* cancelled) {
  DCHECK(!event_.IsSignaled());
  event_.Reset();

  if (BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&ChromeNSSCryptoModuleDelegate::ShowDialog,
                     // This method blocks on |event_| until the task completes,
                     // so there's no need to ref-count.
                     base::Unretained(this),
                     slot_name,
                     retry))) {
    event_.Wait();
  }
  *cancelled = cancelled_;
  return password_;
}

void ChromeNSSCryptoModuleDelegate::ShowDialog(const std::string& slot_name,
                                               bool retry) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ShowCryptoModulePasswordDialog(
      slot_name,
      retry,
      reason_,
      server_.host(),
      NULL,  // TODO(mattm): Supply parent window.
      base::Bind(&ChromeNSSCryptoModuleDelegate::GotPassword,
                 // RequestPassword is blocked on |event_| until GotPassword is
                 // called, so there's no need to ref-count.
                 base::Unretained(this)));
}

void ChromeNSSCryptoModuleDelegate::GotPassword(const std::string& password) {
  if (!password.empty())
    password_ = password;
  else
    cancelled_ = true;
  event_.Signal();
}

crypto::CryptoModuleBlockingPasswordDelegate*
CreateCryptoModuleBlockingPasswordDelegate(
    chrome::CryptoModulePasswordReason reason,
    const net::HostPortPair& server) {
  // Returns a ChromeNSSCryptoModuleDelegate without Pk11Slot. Since it is only
  // being used as a CryptoModuleBlockingDialogDelegate, using a slot handle is
  // unnecessary.
  return new ChromeNSSCryptoModuleDelegate(
      reason, server, crypto::ScopedPK11Slot());
}
