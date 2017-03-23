// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_CRYPTAUTH_SERVICE_H_
#define COMPONENTS_CRYPTAUTH_CRYPTAUTH_SERVICE_H_

#include "base/macros.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"

namespace cryptauth {

class CryptAuthDeviceManager;
class CryptAuthEnrollmentManager;

// Service which provides access to various CryptAuth singletons.
class CryptAuthService {
 public:
  virtual CryptAuthDeviceManager* GetCryptAuthDeviceManager() = 0;
  virtual CryptAuthEnrollmentManager* GetCryptAuthEnrollmentManager() = 0;
  virtual DeviceClassifier GetDeviceClassifier() = 0;

 protected:
  CryptAuthService() {}
  virtual ~CryptAuthService() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptAuthService);
};

}  // namespace cryptauth

#endif  // COMPONENTS_CRYPTAUTH_CRYPTAUTH_SERVICE_H_
