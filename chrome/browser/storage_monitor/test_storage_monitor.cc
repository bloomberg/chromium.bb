// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_monitor/test_storage_monitor.h"

#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/storage_monitor/storage_info.h"
#include "chrome/test/base/testing_browser_process.h"

#if defined(OS_LINUX)
#include "chrome/browser/storage_monitor/test_media_transfer_protocol_manager_linux.h"
#include "device/media_transfer_protocol/media_transfer_protocol_manager.h"
#endif

namespace chrome {
namespace test {

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
  TestingBrowserProcess* browser_process = TestingBrowserProcess::GetGlobal();
  if (browser_process) {
    DCHECK(browser_process->storage_monitor() == NULL);
    browser_process->SetStorageMonitor(pass_monitor.Pass());
    return monitor;
  }
  return NULL;
}

// static
TestStorageMonitor* TestStorageMonitor::CreateForBrowserTests() {
  TestStorageMonitor* return_monitor = new TestStorageMonitor();
  return_monitor->Init();
  return_monitor->MarkInitialized();

  scoped_ptr<StorageMonitor> monitor(return_monitor);
  BrowserProcessImpl* browser_process =
      static_cast<BrowserProcessImpl*>(g_browser_process);
  DCHECK(browser_process);
  browser_process->set_storage_monitor_for_test(monitor.Pass());
  return return_monitor;
}

// static
void TestStorageMonitor::RemoveSingleton() {
  TestingBrowserProcess* browser_process = TestingBrowserProcess::GetGlobal();
  if (browser_process)
    browser_process->SetStorageMonitor(scoped_ptr<StorageMonitor>());
}

// static
void TestStorageMonitor::SyncInitialize() {
  StorageMonitor* monitor = g_browser_process->storage_monitor();
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
      StorageInfo(device_id, path.BaseName().LossyDisplayName(), path.value(),
                  string16(), string16(), string16(), 0);
  return true;
}

#if defined(OS_WIN)
bool TestStorageMonitor::GetMTPStorageInfoFromDeviceId(
    const std::string& storage_device_id,
    string16* device_location,
    string16* storage_object_id) const {
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

}  // namespace test
}  // namespace chrome
