// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CRYPTOHOME_MOCK_HOMEDIR_METHODS_H_
#define CHROMEOS_CRYPTOHOME_MOCK_HOMEDIR_METHODS_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chromeos/cryptohome/homedir_methods.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cryptohome {

class CHROMEOS_EXPORT MockHomedirMethods : public HomedirMethods {
 public:
  MockHomedirMethods();
  virtual ~MockHomedirMethods();

  void SetUp(bool success, MountError return_code);

  MOCK_METHOD3(CheckKeyEx,
               void(const Identification& id,
                    const Authorization& key,
                    const Callback& callback));
  MOCK_METHOD4(MountEx,
               void(const Identification& id,
                    const Authorization& key,
                    const MountParameters& request,
                    const MountCallback& callback));
  MOCK_METHOD5(AddKeyEx,
               void(const Identification& id,
                    const Authorization& auth,
                    const KeyDefinition& key,
                    bool clobber_if_exist,
                    const Callback& callback));
  MOCK_METHOD4(RemoveKeyEx,
               void(const Identification& id,
                    const Authorization& auth,
                    const std::string& label,
                    const Callback& callback));
  MOCK_METHOD5(UpdateKeyEx,
               void(const Identification& id,
                    const Authorization& auth,
                    const KeyDefinition& key,
                    const std::string& signature,
                    const Callback& callback));

 private:
  bool success_;
  MountError return_code_;

  void DoCallback(const Callback& callback);
  void DoMountCallback(const MountCallback& callback);

  DISALLOW_COPY_AND_ASSIGN(MockHomedirMethods);
};

}  // namespace cryptohome

#endif  // CHROMEOS_CRYPTOHOME_MOCK_HOMEDIR_METHODS_H_
