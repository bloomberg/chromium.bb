// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/remote_commands/device_commands_factory_chromeos.h"

#include "base/memory/scoped_ptr.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/policy/remote_commands/device_command_reboot_job.h"
#include "chrome/browser/chromeos/policy/remote_commands/device_command_screenshot_job.h"
#include "chrome/browser/chromeos/policy/remote_commands/screenshot_delegate.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "content/public/browser/browser_thread.h"
#include "policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

DeviceCommandsFactoryChromeOS::DeviceCommandsFactoryChromeOS() {
}

DeviceCommandsFactoryChromeOS::~DeviceCommandsFactoryChromeOS() {
}

scoped_ptr<RemoteCommandJob> DeviceCommandsFactoryChromeOS::BuildJobForType(
    em::RemoteCommand_Type type) {
  switch (type) {
    case em::RemoteCommand_Type_DEVICE_REBOOT:
      return make_scoped_ptr<RemoteCommandJob>(new DeviceCommandRebootJob(
          chromeos::DBusThreadManager::Get()->GetPowerManagerClient()));
    case em::RemoteCommand_Type_DEVICE_SCREENSHOT:
      return make_scoped_ptr<RemoteCommandJob>(
          new DeviceCommandScreenshotJob(make_scoped_ptr(new ScreenshotDelegate(
              content::BrowserThread::GetBlockingPool()->GetSequencedTaskRunner(
                  content::BrowserThread::GetBlockingPool()
                      ->GetSequenceToken())))));
    default:
      return nullptr;
  }
}

}  // namespace policy
