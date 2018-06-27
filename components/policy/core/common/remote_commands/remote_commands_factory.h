// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_FACTORY_H_
#define COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/policy/policy_export.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

class RemoteCommandJob;
class RemoteCommandsService;

// An interface class for creating remote commands based on command type.
class POLICY_EXPORT RemoteCommandsFactory {
 public:
  virtual ~RemoteCommandsFactory();

  virtual std::unique_ptr<RemoteCommandJob> BuildJobForType(
      enterprise_management::RemoteCommand_Type type,
      RemoteCommandsService* service) = 0;

 private:
  DISALLOW_ASSIGN(RemoteCommandsFactory);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_FACTORY_H_
