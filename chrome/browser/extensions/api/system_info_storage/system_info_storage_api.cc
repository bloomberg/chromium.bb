// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/extensions/api/system_info_storage/system_info_storage_api.h"

#include "base/logging.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/api/system_info_storage/storage_info_provider.h"

namespace extensions {

using api::experimental_system_info_storage::StorageInfo;
using api::experimental_system_info_storage::StorageUnitInfo;
using content::BrowserThread;

SystemInfoStorageGetFunction::SystemInfoStorageGetFunction() {
}

SystemInfoStorageGetFunction::~SystemInfoStorageGetFunction() {
}

bool SystemInfoStorageGetFunction::RunImpl() {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&SystemInfoStorageGetFunction::WorkOnFileThread, this));
  return true;
}

void SystemInfoStorageGetFunction::WorkOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  bool success = false;
  scoped_ptr<StorageInfoProvider> provider(StorageInfoProvider::Create());
  StorageInfo info;
  if (provider.get() && provider->GetStorageInfo(&info)) {
    SetResult(info.ToValue().release());
    success = true;
  } else {
    SetError("Error in querying storage information");
  }

  // Return the result on UI thread.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SystemInfoStorageGetFunction::RespondOnUIThread,
      this, success));
}

void SystemInfoStorageGetFunction::RespondOnUIThread(bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SendResponse(success);
}

#if !defined(OS_WIN)
// A Mock implementation for temporary usage. Will be moved out for testing
// once the platform specifc implementations are ready.
const char kStorageTypeUnknownStr[] = "unknown";
const char kStorageTypeHardDiskStr[] = "harddisk";
const char kStorageTypeUSBStr[] = "usb";
const char kStorageTypeMMCStr[] = "mmc";

class MockStorageInfoProvider : public StorageInfoProvider {
 public:
  MockStorageInfoProvider() {}
  virtual ~MockStorageInfoProvider() {}
  virtual bool GetStorageInfo(StorageInfo* info) OVERRIDE;
};

bool MockStorageInfoProvider::GetStorageInfo(StorageInfo* info) {
  if (info == NULL) return false;
  linked_ptr<StorageUnitInfo> unit(new StorageUnitInfo());
  unit->id = "0xbeaf";
  unit->type = kStorageTypeUnknownStr;
  unit->capacity = 4098;
  unit->available_capacity = 1000;

  info->units.push_back(unit);
  return true;
}

// static
StorageInfoProvider* StorageInfoProvider::Create() {
  return new MockStorageInfoProvider();
}
#endif  // !OS_WIN

}  // namespace extensions
