// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CRYPTOHOME_MOCK_HOMEDIR_METHODS_H_
#define CHROMEOS_CRYPTOHOME_MOCK_HOMEDIR_METHODS_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chromeos/cryptohome/homedir_methods.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cryptohome {

class CHROMEOS_EXPORT MockHomedirMethods : public HomedirMethods {
 public:
  MockHomedirMethods();
  ~MockHomedirMethods() override;

  void SetUp(bool success, MountError return_code);

  MOCK_METHOD3(GetKeyDataEx,
               void(const Identification& id,
                    const std::string& label,
                    const GetKeyDataCallback& callback));
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
  MOCK_METHOD3(RenameCryptohome,
               void(const Identification& id_from,
                    const Identification& id_to,
                    const Callback& callback));
  MOCK_METHOD2(GetAccountDiskUsage,
               void(const Identification& id,
                    const GetAccountDiskUsageCallback& callback));

  void set_mount_callback(const base::Closure& callback) {
    on_mount_called_ = callback;
  }
  void set_add_key_callback(const base::Closure& callback) {
    on_add_key_called_ = callback;
  }

 private:
  void DoCallback(const Callback& callback);
  void DoGetDataCallback(const GetKeyDataCallback& callback);
  void DoMountCallback(const MountCallback& callback);
  void DoAddKeyCallback(const Callback& callback);

  bool success_ = false;
  MountError return_code_ = MOUNT_ERROR_NONE;

  base::Closure on_mount_called_;
  base::Closure on_add_key_called_;

  DISALLOW_COPY_AND_ASSIGN(MockHomedirMethods);
};

}  // namespace cryptohome

#endif  // CHROMEOS_CRYPTOHOME_MOCK_HOMEDIR_METHODS_H_
