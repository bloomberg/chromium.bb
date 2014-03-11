// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/storage_monitor/test_storage_monitor.h"

#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "components/storage_monitor/storage_info.h"

#if defined(OS_LINUX)
#include "components/storage_monitor/test_media_transfer_protocol_manager_linux.h"
#include "device/media_transfer_protocol/media_transfer_protocol_manager.h"
#endif

namespace storage_monitor {

TestStorageMonitor::TestStorageMonitor()
    : StorageMonitor(),
      init_called_(false) {
#if defined(OS_LINUX)
  media_transfer_protocol_manager_.reset(
      new TestMediaTransferProtocolManagerLinux());
#endif
}

TestStorageMonitor::~TestStorageMonitor() {}

// static
TestStorageMonitor* TestStorageMonitor::CreateAndInstall() {
  TestStorageMonitor* monitor = new TestStorageMonitor();
  scoped_ptr<StorageMonitor> pass_monitor(monitor);
  monitor->Init();
  monitor->MarkInitialized();

  if (StorageMonitor::GetInstance() == NULL) {
    StorageMonitor::SetStorageMonitorForTesting(pass_monitor.Pass());
    return monitor;
  }

  return NULL;
}

// static
TestStorageMonitor* TestStorageMonitor::CreateForBrowserTests() {
  TestStorageMonitor* monitor = new TestStorageMonitor();
  monitor->Init();
  monitor->MarkInitialized();

  scoped_ptr<StorageMonitor> pass_monitor(monitor);
  StorageMonitor::SetStorageMonitorForTesting(pass_monitor.Pass());

  return monitor;
}

// static
void TestStorageMonitor::SyncInitialize() {
  StorageMonitor* monitor = StorageMonitor::GetInstance();
  if (monitor->IsInitialized())
    return;

  base::WaitableEvent event(true, false);
  monitor->EnsureInitialized(base::Bind(&base::WaitableEvent::Signal,
                             base::Unretained(&event)));
  while (!event.IsSignaled()) {
    base::RunLoop().RunUntilIdle();
  }
  DCHECK(monitor->IsInitialized());
}

void TestStorageMonitor::Init() {
  init_called_ = true;
}

void TestStorageMonitor::MarkInitialized() {
  StorageMonitor::MarkInitialized();
}

bool TestStorageMonitor::GetStorageInfoForPath(
    const base::FilePath& path,
    StorageInfo* device_info) const {
  DCHECK(device_info);

  if (!path.IsAbsolute())
    return false;

  std::string device_id = StorageInfo::MakeDeviceId(
      StorageInfo::FIXED_MASS_STORAGE, path.AsUTF8Unsafe());
  *device_info =
      StorageInfo(device_id, path.value(), base::string16(), base::string16(),
                  base::string16(), 0);
  return true;
}

#if defined(OS_WIN)
bool TestStorageMonitor::GetMTPStorageInfoFromDeviceId(
    const std::string& storage_device_id,
    base::string16* device_location,
    base::string16* storage_object_id) const {
  return false;
}
#endif

#if defined(OS_LINUX)
device::MediaTransferProtocolManager*
TestStorageMonitor::media_transfer_protocol_manager() {
  return media_transfer_protocol_manager_.get();
}
#endif

StorageMonitor::Receiver* TestStorageMonitor::receiver() const {
  return StorageMonitor::receiver();
}

void TestStorageMonitor::EjectDevice(
    const std::string& device_id,
    base::Callback<void(EjectStatus)> callback) {
  ejected_device_ = device_id;
  callback.Run(EJECT_OK);
}

}  // namespace storage_monitor
