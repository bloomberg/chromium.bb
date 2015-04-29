// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_IMPL_H_
#define COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_IMPL_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "components/gcm_driver/instance_id/instance_id.h"

namespace instance_id {

// InstanceID implementation for desktop and iOS.
class InstanceIDImpl : public InstanceID {
 public:
  explicit InstanceIDImpl(const std::string& app_id);
  ~InstanceIDImpl() override;

  // InstanceID:
  std::string GetID() override;
  base::Time GetCreationTime() override;
  void GetToken(const std::string& audience,
                const std::string& scope,
                const std::map<std::string, std::string>& options,
                const GetTokenCallback& callback) override;
  void DeleteToken(const std::string& audience,
                   const std::string& scope,
                   const DeleteTokenCallback& callback) override;
  void DeleteID(const DeleteIDCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InstanceIDImpl);
};

}  // namespace instance_id

#endif  // COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_IMPL_H_
