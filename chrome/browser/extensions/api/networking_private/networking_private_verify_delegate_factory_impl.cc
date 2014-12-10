// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_private/networking_private_verify_delegate_factory_impl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/extensions/api/networking_private/networking_private_verify_delegate_chromeos.h"
#elif defined(OS_WIN) || defined(OSMACOSX)
#include "chrome/browser/extensions/api/networking_private/crypto_verify_impl.h"
#endif

namespace extensions {

NetworkingPrivateVerifyDelegateFactoryImpl::
    NetworkingPrivateVerifyDelegateFactoryImpl() {
}

NetworkingPrivateVerifyDelegateFactoryImpl::
    ~NetworkingPrivateVerifyDelegateFactoryImpl() {
}

scoped_ptr<NetworkingPrivateDelegate::VerifyDelegate>
NetworkingPrivateVerifyDelegateFactoryImpl::CreateDelegate() {
#if defined(OS_CHROMEOS)
  return make_scoped_ptr(new NetworkingPrivateVerifyDelegateChromeOS());
#elif defined(OS_WIN) || defined(OSMACOSX)
  return make_scoped_ptr(new CryptoVerifyImpl());
#endif
  return nullptr;
}

}  // namespace extensions
