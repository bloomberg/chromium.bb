// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_private/networking_private_ui_delegate_factory_impl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/extensions/api/networking_private/networking_private_ui_delegate_chromeos.h"
#endif

namespace extensions {

NetworkingPrivateUIDelegateFactoryImpl::
    NetworkingPrivateUIDelegateFactoryImpl() {}

NetworkingPrivateUIDelegateFactoryImpl::
    ~NetworkingPrivateUIDelegateFactoryImpl() {}

scoped_ptr<NetworkingPrivateDelegate::UIDelegate>
NetworkingPrivateUIDelegateFactoryImpl::CreateDelegate() {
#if defined(OS_CHROMEOS)
  return make_scoped_ptr(
      new chromeos::extensions::NetworkingPrivateUIDelegateChromeOS());
#else
  return nullptr;
#endif
}

}  // namespace extensions
