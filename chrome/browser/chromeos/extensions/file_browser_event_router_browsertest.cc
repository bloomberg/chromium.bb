// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_browser_event_router.h"

#include "chrome/browser/chromeos/extensions/file_browser_private_api.h"
#include "chrome/browser/chromeos/extensions/file_browser_private_api_factory.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "chromeos/disks/mock_disk_mount_manager.h"

using testing::_;

namespace chromeos {
namespace disks {
namespace {

class FileBrowserEventRouterBrowserTest : public InProcessBrowserTest {
 public:
  // ExtensionApiTest override
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
   InProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    disk_mount_manager_mock_ = new MockDiskMountManager;
    chromeos::disks::DiskMountManager::InitializeForTesting(
        disk_mount_manager_mock_);
    disk_mount_manager_mock_->SetupDefaultReplies();
  }

  MockDiskMountManager* disk_mount_manager_mock_;
};

IN_PROC_BROWSER_TEST_F(FileBrowserEventRouterBrowserTest,
                       ExternalStoragePolicyTest) {
  FileBrowserPrivateAPI* file_browser =
      FileBrowserPrivateAPIFactory::GetForProfile(browser()->profile());
  scoped_refptr<FileBrowserEventRouter> event_router =
      file_browser->event_router();

  DiskMountManager::DiskEvent event = DiskMountManager::DISK_ADDED;
  // Prepare a fake disk. All that matters here is that the mount point is empty
  // but the device path is not so that it will exercise the path we care about.
  DiskMountManager::Disk disk(
      "fake_path", "", "X", "X", "X", "X", "X", "X", "X", "X", "X", "X",
      chromeos::DEVICE_TYPE_USB, 1, false, false, true, false, false);

  // First we set the policy to prevent storage mounting and check that the
  // callback is not called.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kExternalStorageDisabled,
                                               true);

  EXPECT_CALL(*disk_mount_manager_mock_, MountPath(_, _, _, _)).Times(0);

  event_router->OnDiskEvent(event, &disk);

  // Next we repeat but with the policy not active this time.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kExternalStorageDisabled,
                                               false);

  EXPECT_CALL(*disk_mount_manager_mock_,
              MountPath("fake_path", "", "", chromeos::MOUNT_TYPE_DEVICE))
      .Times(1);

  event_router->OnDiskEvent(event, &disk);
}

}  // namespace
}  // namespace disks
}  // namespace chromeos
