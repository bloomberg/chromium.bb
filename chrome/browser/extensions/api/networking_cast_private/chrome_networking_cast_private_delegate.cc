// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_cast_private/chrome_networking_cast_private_delegate.h"

#include "base/callback.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_verify_delegate_factory_impl.h"
#include "extensions/common/api/networking_private.h"

namespace extensions {

namespace {

ChromeNetworkingCastPrivateDelegate::FactoryCallback* g_factory_callback =
    nullptr;

}  // namespace

std::unique_ptr<ChromeNetworkingCastPrivateDelegate>
ChromeNetworkingCastPrivateDelegate::Create() {
  if (g_factory_callback)
    return g_factory_callback->Run();
  return std::unique_ptr<ChromeNetworkingCastPrivateDelegate>(
      new ChromeNetworkingCastPrivateDelegate(
          NetworkingPrivateVerifyDelegateFactoryImpl().CreateDelegate()));
}

void ChromeNetworkingCastPrivateDelegate::SetFactoryCallbackForTest(
    FactoryCallback* factory_callback) {
  g_factory_callback = factory_callback;
}

ChromeNetworkingCastPrivateDelegate::ChromeNetworkingCastPrivateDelegate(
    std::unique_ptr<NetworkingPrivateDelegate::VerifyDelegate> crypto_verify)
    : crypto_verify_(std::move(crypto_verify)) {}

ChromeNetworkingCastPrivateDelegate::~ChromeNetworkingCastPrivateDelegate() {}

void ChromeNetworkingCastPrivateDelegate::VerifyDestination(
    const api::networking_private::VerificationProperties& properties,
    const VerifiedCallback& success_callback,
    const FailureCallback& failure_callback) {
  crypto_verify_->VerifyDestination(properties, success_callback,
                                    failure_callback);
}

void ChromeNetworkingCastPrivateDelegate::VerifyAndEncryptCredentials(
    const std::string& guid,
    const api::networking_private::VerificationProperties& properties,
    const DataCallback& success_callback,
    const FailureCallback& failure_callback) {
  crypto_verify_->VerifyAndEncryptCredentials(
      guid, properties, success_callback, failure_callback);
}

void ChromeNetworkingCastPrivateDelegate::VerifyAndEncryptData(
    const std::string& data,
    const api::networking_private::VerificationProperties& properties,
    const DataCallback& success_callback,
    const FailureCallback& failure_callback) {
  crypto_verify_->VerifyAndEncryptData(properties, data, success_callback,
                                       failure_callback);
}

}  // namespace extensions
