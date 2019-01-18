// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_SERVICE_H_
#define COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/version_info/channel.h"

namespace syncer {
class ModelTypeControllerDelegate;
class LocalDeviceInfoProvider;
}  // namespace syncer

namespace send_tab_to_self {
class SendTabToSelfBridge;
class SendTabToSelfModel;

// KeyedService responsible for send tab to self sync.
class SendTabToSelfService : public KeyedService {
 public:
  SendTabToSelfService(
      version_info::Channel channel,
      syncer::LocalDeviceInfoProvider* local_device_info_provider);
  ~SendTabToSelfService() override;

  SendTabToSelfModel* GetSendTabToSelfModel();

  // For ProfileSyncService to initialize the controller.
  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetControllerDelegate();

 private:
  std::unique_ptr<SendTabToSelfBridge> bridge_;

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfService);
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_SERVICE_H_
