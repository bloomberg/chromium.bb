// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_ANDROID_H_
#define COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_ANDROID_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "components/gcm_driver/instance_id/instance_id.h"

namespace instance_id {

// InstanceID implementation for Android.
class InstanceIDAndroid : public InstanceID {
 public:
  InstanceIDAndroid(const std::string& app_id);
  ~InstanceIDAndroid() override;

  // InstanceID:
  void GetID(const GetIDCallback& callback) override;
  void GetCreationTime(const GetCreationTimeCallback& callback) override;
  void GetToken(const std::string& audience,
                const std::string& scope,
                const std::map<std::string, std::string>& options,
                const GetTokenCallback& callback) override;
  void DeleteToken(const std::string& audience,
                   const std::string& scope,
                   const DeleteTokenCallback& callback) override;
  void DeleteID(const DeleteIDCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InstanceIDAndroid);
};

}  // namespace instance_id

#endif  // COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_ANDROID_H_
