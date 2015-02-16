// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/remote_commands/device_command_reboot_job.h"

#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "chromeos/dbus/power_manager_client.h"
#include "policy/proto/device_management_backend.pb.h"

namespace policy {

namespace {

// Determines the time, measured from the time of issue, after which the command
// queue will consider this command expired if the command has not been started.
const int kCommandExpirationTimeInMinutes = 10;

// Determines the minimum uptime after which a reboot might be scheduled. Note:
// |kCommandExpirationTimeInMinutes| >= |kMinimumUptimeInMinutes| as
// otherwise, a valid command issued right after boot may time out.
const int kMinimumUptimeInMinutes = 10;

}  // namespace

DeviceCommandRebootJob::DeviceCommandRebootJob(
    chromeos::PowerManagerClient* power_manager_client)
    : power_manager_client_(power_manager_client), weak_ptr_factory_(this) {
  CHECK(power_manager_client_);
}

DeviceCommandRebootJob::~DeviceCommandRebootJob() {
}

enterprise_management::RemoteCommand_Type DeviceCommandRebootJob::GetType()
    const {
  return enterprise_management::RemoteCommand_Type_DEVICE_REBOOT;
}

bool DeviceCommandRebootJob::IsExpired(base::Time now) {
  return now > issued_time() + base::TimeDelta::FromMinutes(
                                   kCommandExpirationTimeInMinutes);
}

void DeviceCommandRebootJob::RunImpl(
    const SucceededCallback& succeeded_callback,
    const FailedCallback& failed_callback) {
  // Determines the time delta between the command having been issued and the
  // boot time of the system.
  const base::TimeDelta uptime =
      base::TimeDelta::FromMilliseconds(base::SysInfo::Uptime());
  const base::Time boot_time = base::Time::Now() - uptime;
  const base::TimeDelta delta = boot_time - issued_time();
  // If the reboot command was issued before the system booted, we inform the
  // server that the reboot succeeded. Otherwise, the reboot must still be
  // performed and we invoke it. |kMinimumUptimeInMinutes| defines a lower limit
  // on the uptime to avoid uninterruptable reboot loops.
  if (delta > base::TimeDelta()) {
    succeeded_callback.Run(nullptr);
    return;
  }

  const base::TimeDelta kZeroTimeDelta;
  reboot_timer_.Start(
      FROM_HERE,
      std::max(base::TimeDelta::FromMinutes(kMinimumUptimeInMinutes) - uptime,
               kZeroTimeDelta),
      base::Bind(&DeviceCommandRebootJob::Reboot,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DeviceCommandRebootJob::TerminateImpl() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

base::TimeDelta DeviceCommandRebootJob::GetCommmandTimeout() const {
  return base::TimeDelta::FromMinutes(kMinimumUptimeInMinutes);
}

void DeviceCommandRebootJob::Reboot() const {
  power_manager_client_->RequestRestart();
}

}  // namespace policy
