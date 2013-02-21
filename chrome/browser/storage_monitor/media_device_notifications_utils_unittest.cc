// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_monitor/media_device_notifications_utils.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "chrome/browser/storage_monitor/removable_device_constants.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

using content::BrowserThread;

class MediaDeviceNotificationUtilsTest : public testing::Test {
 public:
  MediaDeviceNotificationUtilsTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE) { }
  virtual ~MediaDeviceNotificationUtilsTest() { }

  // Verify mounted device type.
  void checkDeviceType(const base::FilePath::StringType& mount_point,
                       bool expected_val) {
    if (expected_val)
      EXPECT_TRUE(IsMediaDevice(mount_point));
    else
      EXPECT_FALSE(IsMediaDevice(mount_point));
  }

 protected:
  // Create mount point for the test device.
  base::FilePath CreateMountPoint(bool create_dcim_dir) {
    base::FilePath path(scoped_temp_dir_.path());
    if (create_dcim_dir)
      path = path.Append(kDCIMDirectoryName);
    if (!file_util::CreateDirectory(path))
      return base::FilePath();
    return scoped_temp_dir_.path();
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    file_thread_.Start();
  }

  virtual void TearDown() {
    WaitForFileThread();
  }

  static void PostQuitToUIThread() {
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            MessageLoop::QuitClosure());
  }

  static void WaitForFileThread() {
    BrowserThread::PostTask(BrowserThread::FILE,
                            FROM_HERE,
                            base::Bind(&PostQuitToUIThread));
    MessageLoop::current()->Run();
  }

  MessageLoop message_loop_;

 private:
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  base::ScopedTempDir scoped_temp_dir_;
};

// Test to verify that IsMediaDevice() function returns true for the given
// media device mount point.
TEST_F(MediaDeviceNotificationUtilsTest, MediaDeviceAttached) {
  // Create a dummy mount point with DCIM Directory.
  base::FilePath mount_point(CreateMountPoint(true));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&MediaDeviceNotificationUtilsTest::checkDeviceType,
                 base::Unretained(this), mount_point.value(), true));
  message_loop_.RunUntilIdle();
}

// Test to verify that IsMediaDevice() function returns false for a given
// non-media device mount point.
TEST_F(MediaDeviceNotificationUtilsTest, NonMediaDeviceAttached) {
  // Create a dummy mount point without DCIM Directory.
  base::FilePath mount_point(CreateMountPoint(false));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&MediaDeviceNotificationUtilsTest::checkDeviceType,
                 base::Unretained(this), mount_point.value(), false));
  message_loop_.RunUntilIdle();
}

}  // namespace chrome
