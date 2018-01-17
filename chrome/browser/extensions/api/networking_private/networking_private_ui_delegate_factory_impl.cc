// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_private/networking_private_ui_delegate_factory_impl.h"

#include <memory>

#include "build/build_config.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/extensions/api/networking_private/networking_private_ui_delegate_chromeos.h"
#endif

namespace extensions {

NetworkingPrivateUIDelegateFactoryImpl::
    NetworkingPrivateUIDelegateFactoryImpl() {}

NetworkingPrivateUIDelegateFactoryImpl::
    ~NetworkingPrivateUIDelegateFactoryImpl() {}

std::unique_ptr<NetworkingPrivateDelegate::UIDelegate>
NetworkingPrivateUIDelegateFactoryImpl::CreateDelegate() {
#if defined(OS_CHROMEOS)
  return std::make_unique<
      chromeos::extensions::NetworkingPrivateUIDelegateChromeOS>();
#else
  return nullptr;
#endif
}

}  // namespace extensions
