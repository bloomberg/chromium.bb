// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_CAST_PRIVATE_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_CAST_PRIVATE_DELEGATE_H_

#include <string>

#include "base/callback.h"
#include "extensions/common/api/networking_private.h"

namespace extensions {

// Delegate interface that provides crypto methods needed to verify cast
// certificates and encrypt data using public key derived from the verified
// certificate.
// TODO(tbarzic): This is to be used during migration of
//     networkingPrivate.verify* methods to networking.castPrivate API to share
//     verification logic shared between networkingPrivate and
//     networking.castPrivate API. When the deprecated networkingPrivate methods
//     are removed, this interface should be removed, too.
class NetworkingCastPrivateDelegate {
 public:
  virtual ~NetworkingCastPrivateDelegate() {}

  using FailureCallback = base::Callback<void(const std::string& error)>;
  using VerifiedCallback = base::Callback<void(bool is_valid)>;
  using DataCallback = base::Callback<void(const std::string& encrypted_data)>;

  // Verifies that data provided in |properties| authenticates a cast device.
  virtual void VerifyDestination(
      const api::networking_private::VerificationProperties& properties,
      const VerifiedCallback& success_callback,
      const FailureCallback& failure_callback) = 0;

  // Verifies that data provided in |properties| authenticates a cast device.
  // If the device is verified as a cast device, it fetches credentials of the
  // network identified with |network_guid| and returns the network credentials
  // encrypted with a public key derived from |properties|.
  virtual void VerifyAndEncryptCredentials(
      const std::string& network_guid,
      const api::networking_private::VerificationProperties& properties,
      const DataCallback& encrypted_credetials_callback,
      const FailureCallback& failure_callback) = 0;

  // Verifies that data provided in |properties| authenticates a cast device.
  // If the device is verified as a cast device, it returns |data| encrypted
  // with a public key derived from |properties|.
  virtual void VerifyAndEncryptData(
      const std::string& data,
      const api::networking_private::VerificationProperties& properties,
      const DataCallback& enrypted_data_callback,
      const FailureCallback& failure_callback) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_CAST_PRIVATE_DELEGATE_H_
