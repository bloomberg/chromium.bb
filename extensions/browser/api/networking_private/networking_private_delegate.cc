// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/networking_private/networking_private_delegate.h"

#include "extensions/browser/api/networking_private/networking_private_api.h"

namespace extensions {

NetworkingPrivateDelegate::UIDelegate::UIDelegate() {}

NetworkingPrivateDelegate::UIDelegate::~UIDelegate() {}

NetworkingPrivateDelegate::NetworkingPrivateDelegate() {}

NetworkingPrivateDelegate::~NetworkingPrivateDelegate() {
}

void NetworkingPrivateDelegate::AddObserver(
    NetworkingPrivateDelegateObserver* observer) {
  NOTREACHED() << "Class does not use NetworkingPrivateDelegateObserver";
}

void NetworkingPrivateDelegate::RemoveObserver(
    NetworkingPrivateDelegateObserver* observer) {
  NOTREACHED() << "Class does not use NetworkingPrivateDelegateObserver";
}

void NetworkingPrivateDelegate::StartActivate(
    const std::string& guid,
    const std::string& carrier,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  failure_callback.Run(networking_private::kErrorNotSupported);
}

}  // namespace extensions
