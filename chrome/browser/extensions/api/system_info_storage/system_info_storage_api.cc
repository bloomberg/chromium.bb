// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/extensions/api/system_info_storage/system_info_storage_api.h"

namespace extensions {

using api::experimental_system_info_storage::StorageUnitInfo;

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

void SystemInfoStorageGetFunction::OnGetStorageInfoCompleted(
    const StorageInfo& info, bool success) {

  if (success) {
    results_ =
      api::experimental_system_info_storage::Get::Results::Create(info);
  } else {
    SetError("Error occurred when querying storage information.");
  }

  SendResponse(success);
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
