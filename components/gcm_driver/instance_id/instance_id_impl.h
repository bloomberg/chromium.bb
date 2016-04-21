// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_IMPL_H_
#define COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_IMPL_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/gcm_driver/gcm_delayed_task_controller.h"
#include "components/gcm_driver/instance_id/instance_id.h"

namespace gcm {
class InstanceIDHandler;
}  // namespace gcm

namespace instance_id {

// InstanceID implementation for desktop and iOS.
class InstanceIDImpl : public InstanceID {
 public:
  InstanceIDImpl(const std::string& app_id, gcm::InstanceIDHandler* handler);
  ~InstanceIDImpl() override;

  // InstanceID:
  void GetID(const GetIDCallback& callback) override;
  void GetCreationTime(const GetCreationTimeCallback& callback) override;
  void GetToken(const std::string& authorized_entity,
                const std::string& scope,
                const std::map<std::string, std::string>& options,
                const GetTokenCallback& callback) override;
  void DeleteToken(const std::string& authorized_entity,
                   const std::string& scope,
                   const DeleteTokenCallback& callback) override;
  void DeleteID(const DeleteIDCallback& callback) override;

 private:
  void EnsureIDGenerated();

  void OnGetTokenCompleted(const GetTokenCallback& callback,
                           const std::string& token,
                           gcm::GCMClient::Result result);
  void OnDeleteTokenCompleted(const DeleteTokenCallback& callback,
                              gcm::GCMClient::Result result);
  void OnDeleteIDCompleted(const DeleteIDCallback& callback,
                           gcm::GCMClient::Result result);
  void GetInstanceIDDataCompleted(const std::string& instance_id,
                                  const std::string& extra_data);

  void DoGetID(const GetIDCallback& callback);
  void DoGetCreationTime(const GetCreationTimeCallback& callback);
  void DoGetToken(
      const std::string& authorized_entity,
      const std::string& scope,
      const std::map<std::string, std::string>& options,
      const GetTokenCallback& callback);
  void DoDeleteToken(const std::string& authorized_entity,
                     const std::string& scope,
                     const DeleteTokenCallback& callback);
  void DoDeleteID(const DeleteIDCallback& callback);

  // Owned by GCMProfileServiceFactory, which is a dependency of
  // InstanceIDProfileServiceFactory, which owns this.
  gcm::InstanceIDHandler* handler_;

  gcm::GCMDelayedTaskController delayed_task_controller_;

  // The generated Instance ID.
  std::string id_;

  // The time when the Instance ID has been generated.
  base::Time creation_time_;

  base::WeakPtrFactory<InstanceIDImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(InstanceIDImpl);
};

}  // namespace instance_id

#endif  // COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_IMPL_H_
