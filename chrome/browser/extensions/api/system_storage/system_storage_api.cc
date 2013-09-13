// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_storage/system_storage_api.h"

namespace extensions {

using api::system_storage::StorageUnitInfo;
namespace EjectDevice = api::system_storage::EjectDevice;

SystemStorageGetInfoFunction::SystemStorageGetInfoFunction() {
}

SystemStorageGetInfoFunction::~SystemStorageGetInfoFunction() {
}

bool SystemStorageGetInfoFunction::RunImpl() {
  StorageInfoProvider::Get()->StartQueryInfo(
      base::Bind(&SystemStorageGetInfoFunction::OnGetStorageInfoCompleted,
                 this));
  return true;
}

void SystemStorageGetInfoFunction::OnGetStorageInfoCompleted(bool success) {
  if (success) {
    results_ = api::system_storage::GetInfo::Results::Create(
        StorageInfoProvider::Get()->storage_unit_info_list());
  } else {
    SetError("Error occurred when querying storage information.");
  }

  SendResponse(success);
}

SystemStorageEjectDeviceFunction::~SystemStorageEjectDeviceFunction() {
}

bool SystemStorageEjectDeviceFunction::RunImpl() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  scoped_ptr<EjectDevice::Params> params(EjectDevice::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  StorageMonitor::GetInstance()->EnsureInitialized(base::Bind(
      &SystemStorageEjectDeviceFunction::OnStorageMonitorInit,
      this,
      params->id));
  return true;
}

void SystemStorageEjectDeviceFunction::OnStorageMonitorInit(
    const std::string& transient_device_id) {
  DCHECK(StorageMonitor::GetInstance()->IsInitialized());
  StorageMonitor* monitor = StorageMonitor::GetInstance();
  std::string device_id_str =
      StorageMonitor::GetInstance()->GetDeviceIdForTransientId(
          transient_device_id);

  if (device_id_str == "") {
    HandleResponse(StorageMonitor::EJECT_NO_SUCH_DEVICE);
    return;
  }

  monitor->EjectDevice(
      device_id_str,
      base::Bind(&SystemStorageEjectDeviceFunction::HandleResponse,
                 this));
}

void SystemStorageEjectDeviceFunction::HandleResponse(
    StorageMonitor::EjectStatus status) {
  api::system_storage:: EjectDeviceResultCode result =
      api::system_storage::EJECT_DEVICE_RESULT_CODE_FAILURE;
  switch (status) {
    case StorageMonitor::EJECT_OK:
      result = api::system_storage::EJECT_DEVICE_RESULT_CODE_SUCCESS;
      break;
    case StorageMonitor::EJECT_IN_USE:
      result = api::system_storage::EJECT_DEVICE_RESULT_CODE_IN_USE;
      break;
    case StorageMonitor::EJECT_NO_SUCH_DEVICE:
      result = api::system_storage::
          EJECT_DEVICE_RESULT_CODE_NO_SUCH_DEVICE;
      break;
    case StorageMonitor::EJECT_FAILURE:
      result = api::system_storage::EJECT_DEVICE_RESULT_CODE_FAILURE;
  }

  SetResult(new base::StringValue(
      api::system_storage::ToString(result)));
  SendResponse(true);
}

}  // namespace extensions
