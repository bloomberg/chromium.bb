// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/extensions/file_manager/device_event_router.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "content/public/browser/browser_thread.h"

namespace file_manager {
namespace {
namespace file_browser_private = extensions::api::file_browser_private;
using content::BrowserThread;
}  // namespace

DeviceEventRouter::DeviceEventRouter()
    : resume_time_delta_(base::TimeDelta::FromSeconds(5)),
      startup_time_delta_(base::TimeDelta::FromSeconds(10)),
      scan_time_delta_(base::TimeDelta::FromSeconds(5)),
      is_starting_up_(false),
      is_resuming_(false),
      weak_factory_(this) {
}

DeviceEventRouter::DeviceEventRouter(base::TimeDelta overriding_time_delta)
    : resume_time_delta_(overriding_time_delta),
      startup_time_delta_(overriding_time_delta),
      scan_time_delta_(overriding_time_delta),
      is_starting_up_(false),
      is_resuming_(false),
      weak_factory_(this) {
}

DeviceEventRouter::~DeviceEventRouter() {
}

void DeviceEventRouter::Startup() {
  is_starting_up_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DeviceEventRouter::StartupDelayed,
                 weak_factory_.GetWeakPtr()),
      startup_time_delta_);
}

void DeviceEventRouter::StartupDelayed() {
  DCHECK(thread_checker_.CalledOnValidThread());
  is_starting_up_ = false;
}

void DeviceEventRouter::OnDeviceAdded(const std::string& device_path) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (is_starting_up_ || is_resuming_) {
    SetDeviceState(device_path, DEVICE_STATE_USUAL);
    return;
  }

  if (IsExternalStorageDisabled()) {
    OnDeviceEvent(file_browser_private::DEVICE_EVENT_TYPE_DISABLED,
                  device_path);
    SetDeviceState(device_path, DEVICE_STATE_USUAL);
    return;
  }

  SetDeviceState(device_path, DEVICE_SCANNED);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DeviceEventRouter::OnDeviceAddedDelayed,
                 weak_factory_.GetWeakPtr(),
                 device_path),
      scan_time_delta_);
}

void DeviceEventRouter::OnDeviceAddedDelayed(const std::string& device_path) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (GetDeviceState(device_path) == DEVICE_SCANNED) {
    // TODO(hirono): Rename DEVICE_EVENT_TYPE_ADDED with
    // DEVICE_EVENT_TYPE_SCAN_STARTED.
    OnDeviceEvent(file_browser_private::DEVICE_EVENT_TYPE_ADDED, device_path);
    SetDeviceState(device_path, DEVICE_SCANNED_AND_REPORTED);
  }
}

void DeviceEventRouter::OnDeviceRemoved(const std::string& device_path) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SetDeviceState(device_path, DEVICE_STATE_USUAL);
  OnDeviceEvent(file_browser_private::DEVICE_EVENT_TYPE_REMOVED, device_path);
}

void DeviceEventRouter::OnDiskAdded(
    const chromeos::disks::DiskMountManager::Disk& disk,
    bool mounting) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!mounting) {
    // If the disk is not being mounted, mark the device scan cancelled.
    const std::string& device_path = disk.system_path_prefix();
    if (GetDeviceState(device_path) == DEVICE_SCANNED_AND_REPORTED) {
      OnDeviceEvent(file_browser_private::DEVICE_EVENT_TYPE_SCAN_CANCELED,
                    device_path);
    }
    SetDeviceState(device_path, DEVICE_STATE_USUAL);
  }
}

void DeviceEventRouter::OnDiskRemoved(
    const chromeos::disks::DiskMountManager::Disk& disk) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (is_resuming_ || is_starting_up_)
    return;

  const std::string& device_path = disk.system_path_prefix();
  if (!disk.mount_path().empty() &&
      GetDeviceState(device_path) != DEVICE_HARD_UNPLUGGED_AND_REPORTED) {
    OnDeviceEvent(file_browser_private::DEVICE_EVENT_TYPE_HARD_UNPLUGGED,
                  device_path);
    SetDeviceState(device_path, DEVICE_HARD_UNPLUGGED_AND_REPORTED);
  }
}

void DeviceEventRouter::OnVolumeMounted(chromeos::MountError error_code,
                                        const VolumeInfo& volume_info,
                                        bool is_remounting) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const std::string& device_path =
      volume_info.system_path_prefix.AsUTF8Unsafe();
  SetDeviceState(device_path, DEVICE_STATE_USUAL);
}

void DeviceEventRouter::OnVolumeUnmounted(chromeos::MountError error_code,
                                          const VolumeInfo& volume_info) {
  // Do nothing.
}

void DeviceEventRouter::OnFormatStarted(const std::string& device_path,
                                        bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (success) {
    OnDeviceEvent(file_browser_private::DEVICE_EVENT_TYPE_FORMAT_START,
                  device_path);
  } else {
    OnDeviceEvent(file_browser_private::DEVICE_EVENT_TYPE_FORMAT_FAIL,
                  device_path);
  }
}

void DeviceEventRouter::OnFormatCompleted(const std::string& device_path,
                                          bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());

  OnDeviceEvent(success ? file_browser_private::DEVICE_EVENT_TYPE_FORMAT_SUCCESS
                        : file_browser_private::DEVICE_EVENT_TYPE_FORMAT_FAIL,
                device_path);
}

void DeviceEventRouter::OnHardUnplugged(const std::string& device_path) {
}

void DeviceEventRouter::SuspendImminent() {
  DCHECK(thread_checker_.CalledOnValidThread());
  is_resuming_ = true;
}

void DeviceEventRouter::SuspendDone(const base::TimeDelta& sleep_duration) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DeviceEventRouter::SuspendDoneDelayed,
                 weak_factory_.GetWeakPtr()),
      resume_time_delta_);
}

void DeviceEventRouter::SuspendDoneDelayed() {
  DCHECK(thread_checker_.CalledOnValidThread());
  is_resuming_ = false;
}

DeviceState DeviceEventRouter::GetDeviceState(
    const std::string& device_path) const {
  const std::map<std::string, DeviceState>::const_iterator it =
      device_states_.find(device_path);
  return it != device_states_.end() ? it->second : DEVICE_STATE_USUAL;
}

void DeviceEventRouter::SetDeviceState(const std::string& device_path,
                                       DeviceState state) {
  if (state != DEVICE_STATE_USUAL) {
    device_states_[device_path] = state;
  } else {
    const std::map<std::string, DeviceState>::iterator it =
        device_states_.find(device_path);
    if (it != device_states_.end())
      device_states_.erase(it);
  }
}

}  // namespace file_manager
