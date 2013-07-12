// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/extensions/api/system_info_storage/system_info_storage_api.h"

namespace extensions {

using api::experimental_system_info_storage::StorageUnitInfo;
namespace EjectDevice = api::experimental_system_info_storage::EjectDevice;

SystemInfoStorageGetFunction::SystemInfoStorageGetFunction() {
}

SystemInfoStorageGetFunction::~SystemInfoStorageGetFunction() {
}

bool SystemInfoStorageGetFunction::RunImpl() {
  StorageInfoProvider::Get()->StartQueryInfo(
      base::Bind(&SystemInfoStorageGetFunction::OnGetStorageInfoCompleted,
                 this));
  return true;
}

void SystemInfoStorageGetFunction::OnGetStorageInfoCompleted(bool success) {
  if (success) {
    results_ =
      api::experimental_system_info_storage::Get::Results::Create(
          StorageInfoProvider::Get()->storage_unit_info_list());
  } else {
    SetError("Error occurred when querying storage information.");
  }

  SendResponse(success);
}

SystemInfoStorageEjectDeviceFunction::~SystemInfoStorageEjectDeviceFunction() {
}

bool SystemInfoStorageEjectDeviceFunction::RunImpl() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  scoped_ptr<EjectDevice::Params> params(EjectDevice::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  chrome::StorageMonitor::GetInstance()->EnsureInitialized(base::Bind(
      &SystemInfoStorageEjectDeviceFunction::OnStorageMonitorInit,
      this,
      params->id));
  return true;
}

void SystemInfoStorageEjectDeviceFunction::OnStorageMonitorInit(
    const std::string& transient_device_id) {
  DCHECK(chrome::StorageMonitor::GetInstance()->IsInitialized());
  chrome::StorageMonitor* monitor = chrome::StorageMonitor::GetInstance();
  std::string device_id_str =
      StorageInfoProvider::Get()->GetDeviceIdForTransientId(
          transient_device_id);

  if (device_id_str == "") {
    HandleResponse(chrome::StorageMonitor::EJECT_NO_SUCH_DEVICE);
    return;
  }

  monitor->EjectDevice(
      device_id_str,
      base::Bind(&SystemInfoStorageEjectDeviceFunction::HandleResponse,
                 this));
}

void SystemInfoStorageEjectDeviceFunction::HandleResponse(
    chrome::StorageMonitor::EjectStatus status) {
  api::experimental_system_info_storage:: EjectDeviceResultCode result =
      api::experimental_system_info_storage::EJECT_DEVICE_RESULT_CODE_FAILURE;
  switch (status) {
    case chrome::StorageMonitor::EJECT_OK:
      result = api::experimental_system_info_storage::
          EJECT_DEVICE_RESULT_CODE_SUCCESS;
      break;
    case chrome::StorageMonitor::EJECT_IN_USE:
      result = api::experimental_system_info_storage::
          EJECT_DEVICE_RESULT_CODE_IN_USE;
      break;
    case chrome::StorageMonitor::EJECT_NO_SUCH_DEVICE:
      result = api::experimental_system_info_storage::
          EJECT_DEVICE_RESULT_CODE_NO_SUCH_DEVICE;
      break;
    case chrome::StorageMonitor::EJECT_FAILURE:
      result = api::experimental_system_info_storage::
          EJECT_DEVICE_RESULT_CODE_FAILURE;
  }

  SetResult(base::StringValue::CreateStringValue(
      api::experimental_system_info_storage::ToString(result)));
  SendResponse(true);
}

SystemInfoStorageAddWatchFunction::SystemInfoStorageAddWatchFunction() {
}

SystemInfoStorageAddWatchFunction::~SystemInfoStorageAddWatchFunction() {
}

bool SystemInfoStorageAddWatchFunction::RunImpl() {
  // TODO(Haojian): Implement the addWatch api.
  return false;
}

SystemInfoStorageRemoveWatchFunction::SystemInfoStorageRemoveWatchFunction() {
}

SystemInfoStorageRemoveWatchFunction::~SystemInfoStorageRemoveWatchFunction() {
}

bool SystemInfoStorageRemoveWatchFunction::RunImpl() {
  // TODO(Haojian): Implement the removeWatch api.
  return false;
}

SystemInfoStorageGetAllWatchFunction::SystemInfoStorageGetAllWatchFunction() {
}

SystemInfoStorageGetAllWatchFunction::~SystemInfoStorageGetAllWatchFunction() {
}

bool SystemInfoStorageGetAllWatchFunction::RunImpl() {
  // TODO(Haojian): Implement the getAllWatch api.
  return false;
}

SystemInfoStorageRemoveAllWatchFunction::
SystemInfoStorageRemoveAllWatchFunction() {
}

SystemInfoStorageRemoveAllWatchFunction::
~SystemInfoStorageRemoveAllWatchFunction() {
}

bool SystemInfoStorageRemoveAllWatchFunction::RunImpl() {
  // TODO(Haojian): Implement the removeAllWatch api.
  return false;
}

}  // namespace extensions
