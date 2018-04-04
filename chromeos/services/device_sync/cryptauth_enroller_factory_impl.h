// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLER_FACTORY_IMPL_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLER_FACTORY_IMPL_H_

#include "base/memory/ref_counted.h"
#include "components/cryptauth/cryptauth_enroller.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "net/url_request/url_request_context_getter.h"

namespace identity {
class IdentityManager;
}  // namespace identity

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace chromeos {

namespace device_sync {

// CryptAuthEnrollerFactory implementation which utilizes IdentityManager.
class CryptAuthEnrollerFactoryImpl
    : public cryptauth::CryptAuthEnrollerFactory {
 public:
  CryptAuthEnrollerFactoryImpl(
      identity::IdentityManager* identity_manager,
      scoped_refptr<net::URLRequestContextGetter> url_request_context,
      const cryptauth::DeviceClassifier& device_classifier);
  ~CryptAuthEnrollerFactoryImpl() override;

  // cryptauth::CryptAuthEnrollerFactory:
  std::unique_ptr<cryptauth::CryptAuthEnroller> CreateInstance() override;

 private:
  identity::IdentityManager* identity_manager_;
  const scoped_refptr<net::URLRequestContextGetter> url_request_context_;
  const cryptauth::DeviceClassifier device_classifier_;
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLER_FACTORY_IMPL_H_
