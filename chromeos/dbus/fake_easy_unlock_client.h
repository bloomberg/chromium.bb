// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_EASY_UNLOCK_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_EASY_UNLOCK_CLIENT_H_

#include <string>

#include "chromeos/dbus/easy_unlock_client.h"

namespace chromeos {

// A fake implemetation of EasyUnlockClient.
class CHROMEOS_EXPORT FakeEasyUnlockClient : public EasyUnlockClient {
 public:
  FakeEasyUnlockClient();
  virtual ~FakeEasyUnlockClient();

  // EasyUnlockClient overrides
  virtual void Init(dbus::Bus* bus) OVERRIDE;
  virtual void GenerateEcP256KeyPair(const KeyPairCallback& callback) OVERRIDE;
  virtual void PerformECDHKeyAgreement(const std::string& private_key,
                                       const std::string& public_key,
                                       const DataCallback& callback) OVERRIDE;
  virtual void CreateSecureMessage(const std::string& payload,
                                   const std::string& secret_key,
                                   const std::string& associated_data,
                                   const std::string& public_metadata,
                                   const std::string& verification_key_id,
                                   const std::string& encryption_type,
                                   const std::string& signature_type,
                                   const DataCallback& callback) OVERRIDE;
  virtual void UnwrapSecureMessage(const std::string& message,
                                   const std::string& secret_key,
                                   const std::string& associated_data,
                                   const std::string& encryption_type,
                                   const std::string& signature_type,
                                   const DataCallback& callback) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeEasyUnlockClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_EASY_UNLOCK_CLIENT_H_
