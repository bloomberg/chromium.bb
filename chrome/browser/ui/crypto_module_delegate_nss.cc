// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/crypto_module_delegate_nss.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "chrome/browser/net/nss_context.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

ChromeNSSCryptoModuleDelegate::ChromeNSSCryptoModuleDelegate(
    chrome::CryptoModulePasswordReason reason,
    const net::HostPortPair& server)
    : reason_(reason),
      server_(server),
      event_(false, false),
      cancelled_(false) {}

ChromeNSSCryptoModuleDelegate::~ChromeNSSCryptoModuleDelegate() {}

bool ChromeNSSCryptoModuleDelegate::InitializeSlot(
    content::ResourceContext* context,
    const base::Closure& initialization_complete_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!slot_);
  base::Callback<void(crypto::ScopedPK11Slot)> get_slot_callback;
  if (!initialization_complete_callback.is_null())
    get_slot_callback = base::Bind(&ChromeNSSCryptoModuleDelegate::DidGetSlot,
                                   // Caller is responsible for keeping |this|
                                   // alive until the callback is run.
                                   base::Unretained(this),
                                   initialization_complete_callback);

  slot_ = GetPrivateNSSKeySlotForResourceContext(context, get_slot_callback);
  return slot_.get() != NULL;
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

void ChromeNSSCryptoModuleDelegate::DidGetSlot(const base::Closure& callback,
                                               crypto::ScopedPK11Slot slot) {
  DCHECK(!slot_);
  slot_ = slot.Pass();
  callback.Run();
}

crypto::CryptoModuleBlockingPasswordDelegate*
CreateCryptoModuleBlockingPasswordDelegate(
    chrome::CryptoModulePasswordReason reason,
    const net::HostPortPair& server) {
  // Returns a ChromeNSSCryptoModuleDelegate without calling InitializeSlot.
  // Since it is only being used as a CreateCryptoModuleBlockingDialogDelegate,
  // initializing the slot handle is unnecessary.
  return new ChromeNSSCryptoModuleDelegate(reason, server);
}
