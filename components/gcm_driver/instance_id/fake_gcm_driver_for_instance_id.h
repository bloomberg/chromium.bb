// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_INSTANCE_ID_FAKE_GCM_DRIVER__FOR_INSTANCE_ID_H_
#define COMPONENTS_GCM_DRIVER_INSTANCE_ID_FAKE_GCM_DRIVER__FOR_INSTANCE_ID_H_

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/gcm_driver/fake_gcm_driver.h"

namespace instance_id {

class FakeGCMDriverForInstanceID : public gcm::FakeGCMDriver,
                                   public gcm::InstanceIDStore {
 public:
  FakeGCMDriverForInstanceID();
  ~FakeGCMDriverForInstanceID() override;

  // FakeGCMDriver overrides:
  gcm::InstanceIDStore* GetInstanceIDStore() override;

  // InstanceIDStore overrides:
  void AddInstanceIDData(const std::string& app_id,
                         const std::string& instance_id_data) override;
  void RemoveInstanceIDData(const std::string& app_id) override;
  void GetInstanceIDData(
      const std::string& app_id,
      const gcm::InstanceIDStore::GetInstanceIDDataCallback& callback) override;

 private:
  std::map<std::string, std::string> instance_id_data_;

  DISALLOW_COPY_AND_ASSIGN(FakeGCMDriverForInstanceID);
};

}  // namespace instance_id

#endif  // COMPONENTS_GCM_DRIVER_INSTANCE_ID_FAKE_GCM_DRIVER__FOR_INSTANCE_ID_H_
