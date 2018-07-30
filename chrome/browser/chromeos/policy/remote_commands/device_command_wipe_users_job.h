// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_WIPE_USERS_JOB_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_WIPE_USERS_JOB_H_

#include "base/macros.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"

namespace policy {

class RemoteCommandsService;

class DeviceCommandWipeUsersJob : public RemoteCommandJob {
 public:
  explicit DeviceCommandWipeUsersJob(RemoteCommandsService* service);
  ~DeviceCommandWipeUsersJob() override;

  // RemoteCommandJob:
  enterprise_management::RemoteCommand_Type GetType() const override;

 protected:
  // RemoteCommandJob:
  bool IsExpired(base::TimeTicks now) override;
  void RunImpl(CallbackWithResult succeeded_callback,
               CallbackWithResult failed_callback) override;

 private:
  RemoteCommandsService* const service_;

  DISALLOW_COPY_AND_ASSIGN(DeviceCommandWipeUsersJob);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_WIPE_USERS_JOB_H_
