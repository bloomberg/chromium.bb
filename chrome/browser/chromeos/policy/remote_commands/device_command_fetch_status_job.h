// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_FETCH_STATUS_JOB_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_FETCH_STATUS_JOB_H_

#include <string>

#include "base/macros.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"

namespace policy {

class DeviceCommandFetchStatusJob : public RemoteCommandJob {
 public:
  DeviceCommandFetchStatusJob();
  ~DeviceCommandFetchStatusJob() override;

  // RemoteCommandJob:
  enterprise_management::RemoteCommand_Type GetType() const override;

 protected:
  // RemoteCommandJob:
  void RunImpl(CallbackWithResult succeeded_callback,
               CallbackWithResult failed_callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceCommandFetchStatusJob);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_FETCH_STATUS_JOB_H_
