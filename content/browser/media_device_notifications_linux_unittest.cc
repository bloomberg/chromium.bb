// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <mntent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/system_monitor/system_monitor.h"
#include "base/test/mock_devices_changed_observer.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/media_device_notifications_linux.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace {

const char* kValidFS = "vfat";
const char* kInvalidFS = "invalidfs";

const char* kInvalidPath = "invalid path does not exist";

const char* kDevice1 = "d1";
const char* kDevice2 = "d2";
const char* kDevice3 = "d3";

const char* kMountPointA = "mnt_a";
const char* kMountPointB = "mnt_b";

}  // namespace

namespace content {

class MediaDeviceNotificationsLinuxTest : public testing::Test {
 public:
  struct MtabTestData {
    MtabTestData(const char* mount_device,
                 const char* mount_point,
                 const char* mount_type)
        : mount_device(mount_device),
          mount_point(mount_point),
          mount_type(mount_type) {
    }

    const char* mount_device;
    const char* mount_point;
    const char* mount_type;
  };

  MediaDeviceNotificationsLinuxTest()
      : message_loop_(MessageLoop::TYPE_IO),
        file_thread_(BrowserThread::FILE, &message_loop_) {
    system_monitor_.reset(new base::SystemMonitor());
  }
  virtual ~MediaDeviceNotificationsLinuxTest() {}

 protected:
  virtual void SetUp() {
    mock_devices_changed_observer_.reset(new base::MockDevicesChangedObserver);
    system_monitor_->AddDevicesChangedObserver(
        mock_devices_changed_observer_.get());

    // Create and set up a temp dir with files for the test.
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    FilePath test_dir = scoped_temp_dir_.path().AppendASCII("test_etc");
    ASSERT_TRUE(file_util::CreateDirectory(test_dir));
    mtab_file_ = test_dir.AppendASCII("test_mtab");
    struct MtabTestData initial_test_data[] = {
      MtabTestData("dummydevice", "dummydir", kInvalidFS),
    };
    WriteToMtab(initial_test_data, arraysize(initial_test_data), true);

    // Initialize the test subject.
    notifications_ = new MediaDeviceNotificationsLinux(mtab_file_);
    notifications_->Init();
    message_loop_.RunAllPending();
  }

  virtual void TearDown() {
    message_loop_.RunAllPending();
    notifications_ = NULL;
    system_monitor_->RemoveDevicesChangedObserver(
        mock_devices_changed_observer_.get());
  }

  // Used to run tests. When the mtab file gets modified, the message loop
  // needs to run in order to react to the file modification.
  // See WriteToMtab for parameters.
  void WriteToMtabAndRunLoop(struct MtabTestData* data,
                             size_t data_size,
                             bool overwrite) {
    WriteToMtab(data, data_size, overwrite);
    message_loop_.RunAllPending();
  }

  // Create a directory named |dir| relative to the test directory.
  // Set |with_dcim_dir| to true if the created directory will have a "DCIM"
  // subdirectory.
  // Returns the full path to the created directory on success, or an empty
  // path on failure.
  FilePath CreateMountPoint(const char* dir, bool with_dcim_dir) {
    FilePath return_path(scoped_temp_dir_.path());
    return_path = return_path.AppendASCII(dir);
    FilePath path(return_path);
    if (with_dcim_dir)
      path = path.AppendASCII("DCIM");
    if (!file_util::CreateDirectory(path))
      return FilePath();
    return return_path;
  }

  base::MockDevicesChangedObserver& observer() {
    return *mock_devices_changed_observer_.get();
  }

 private:
  // Write the test mtab data to |mtab_file_|.
  // |data| is an array of mtab entries.
  // |data_size| is the array size of |data|.
  // |overwrite| specifies whether to overwrite |mtab_file_|.
  void WriteToMtab(struct MtabTestData* data,
                   size_t data_size,
                   bool overwrite) {
    FILE* file = setmntent(mtab_file_.value().c_str(), overwrite ? "w" : "a");
    ASSERT_TRUE(file);

    struct mntent entry;
    entry.mnt_opts = strdup("rw");
    entry.mnt_freq = 0;
    entry.mnt_passno = 0;
    for (size_t i = 0; i < data_size; ++i) {
      entry.mnt_fsname = strdup(data[i].mount_device);
      entry.mnt_dir = strdup(data[i].mount_point);
      entry.mnt_type = strdup(data[i].mount_type);
      int add_result = addmntent(file, &entry);
      ASSERT_EQ(0, add_result);
      free(entry.mnt_fsname);
      free(entry.mnt_dir);
      free(entry.mnt_type);
    }
    free(entry.mnt_opts);
    int end_result = endmntent(file);
    ASSERT_EQ(1, end_result);

    // Need to ensure data reaches disk so the FilePathWatcher fires in time.
    // Otherwise this will cause MediaDeviceNotificationsLinuxTest to be flaky.
    int fd = open(mtab_file_.value().c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);

    int fsync_result = fsync(fd);
    ASSERT_EQ(0, fsync_result);

    int close_result = close(fd);
    ASSERT_EQ(0, close_result);
  }

  // The message loop and file thread to run tests on.
  MessageLoop message_loop_;
  BrowserThreadImpl file_thread_;

  // SystemMonitor and DevicesChangedObserver to hook together to test.
  scoped_ptr<base::SystemMonitor> system_monitor_;
  scoped_ptr<base::MockDevicesChangedObserver> mock_devices_changed_observer_;

  // Temporary directory for created test data.
  ScopedTempDir scoped_temp_dir_;
  // Path to the test mtab file.
  FilePath mtab_file_;

  scoped_refptr<MediaDeviceNotificationsLinux> notifications_;

  DISALLOW_COPY_AND_ASSIGN(MediaDeviceNotificationsLinuxTest);
};

TEST_F(MediaDeviceNotificationsLinuxTest, BasicAttachDetach) {
  testing::Sequence mock_sequence;
  FilePath test_path = CreateMountPoint(kMountPointA, true);
  ASSERT_FALSE(test_path.empty());
  struct MtabTestData test_data[] = {
    MtabTestData(kDevice1, kInvalidPath, kValidFS),
    MtabTestData(kDevice2, test_path.value().c_str(), kValidFS),
  };
  EXPECT_CALL(observer(), OnMediaDeviceAttached(0, kDevice2, test_path))
      .InSequence(mock_sequence);
  WriteToMtabAndRunLoop(test_data, arraysize(test_data), false);

  EXPECT_CALL(observer(), OnMediaDeviceDetached(0)).InSequence(mock_sequence);
  WriteToMtabAndRunLoop(NULL, 0, true);
}

// Only mount points with DCIM directories are recognized.
TEST_F(MediaDeviceNotificationsLinuxTest, DCIM) {
  testing::Sequence mock_sequence;
  FilePath test_pathA = CreateMountPoint(kMountPointA, true);
  ASSERT_FALSE(test_pathA.empty());
  struct MtabTestData test_data1[] = {
    MtabTestData(kDevice1, test_pathA.value().c_str(), kValidFS),
  };
  EXPECT_CALL(observer(), OnMediaDeviceAttached(0, kDevice1, test_pathA))
      .InSequence(mock_sequence);
  WriteToMtabAndRunLoop(test_data1, arraysize(test_data1), false);

  FilePath test_pathB = CreateMountPoint(kMountPointB, false);
  ASSERT_FALSE(test_pathB.empty());
  struct MtabTestData test_data2[] = {
    MtabTestData(kDevice2, test_pathB.value().c_str(), kValidFS),
  };
  WriteToMtabAndRunLoop(test_data2, arraysize(test_data2), false);

  EXPECT_CALL(observer(), OnMediaDeviceDetached(0)).InSequence(mock_sequence);
  WriteToMtabAndRunLoop(NULL, 0, true);
}

TEST_F(MediaDeviceNotificationsLinuxTest, MultiDevicesMultiMountPoints) {
  FilePath test_pathA = CreateMountPoint(kMountPointA, true);
  FilePath test_pathB = CreateMountPoint(kMountPointB, true);
  ASSERT_FALSE(test_pathA.empty());
  ASSERT_FALSE(test_pathB.empty());

  // Attach two devices.
  // kDevice1 -> kMountPointA
  // kDevice2 -> kMountPointB
  struct MtabTestData test_data1[] = {
    MtabTestData(kDevice1, test_pathA.value().c_str(), kValidFS),
    MtabTestData(kDevice2, test_pathB.value().c_str(), kValidFS),
  };
  EXPECT_CALL(observer(), OnMediaDeviceAttached(_, _, _)).Times(2);
  EXPECT_CALL(observer(), OnMediaDeviceDetached(_)).Times(0);
  WriteToMtabAndRunLoop(test_data1, arraysize(test_data1), false);

  // Attach |kDevice1| to |kMountPointB|.
  // |kDevice2| is inaccessible, so it is detached. |kDevice1| has been
  // re-attached at |kMountPointB|, so it is 'detached' from kMountPointA.
  // kDevice1 -> kMountPointA
  // kDevice2 -> kMountPointB
  // kDevice1 -> kMountPointB
  struct MtabTestData test_data2[] = {
    MtabTestData(kDevice1, test_pathB.value().c_str(), kValidFS),
  };
  EXPECT_CALL(observer(), OnMediaDeviceAttached(_, _, _)).Times(1);
  EXPECT_CALL(observer(), OnMediaDeviceDetached(_)).Times(2);
  WriteToMtabAndRunLoop(test_data2, arraysize(test_data2), false);

  // Attach |kDevice2| to |kMountPointA|.
  // kDevice1 -> kMountPointA
  // kDevice2 -> kMountPointB
  // kDevice1 -> kMountPointB
  // kDevice2 -> kMountPointA
  struct MtabTestData test_data3[] = {
    MtabTestData(kDevice2, test_pathA.value().c_str(), kValidFS),
  };
  EXPECT_CALL(observer(), OnMediaDeviceAttached(_, _, _)).Times(1);
  EXPECT_CALL(observer(), OnMediaDeviceDetached(_)).Times(0);
  WriteToMtabAndRunLoop(test_data3, arraysize(test_data3), false);

  // Detach |kDevice2| from |kMountPointA|.
  // kDevice1 -> kMountPointA
  // kDevice2 -> kMountPointB
  // kDevice1 -> kMountPointB
  struct MtabTestData test_data4[] = {
    MtabTestData(kDevice1, test_pathA.value().c_str(), kValidFS),
    MtabTestData(kDevice2, test_pathB.value().c_str(), kValidFS),
    MtabTestData(kDevice1, test_pathB.value().c_str(), kValidFS),
  };
  EXPECT_CALL(observer(), OnMediaDeviceAttached(_, _, _)).Times(0);
  EXPECT_CALL(observer(), OnMediaDeviceDetached(_)).Times(1);
  WriteToMtabAndRunLoop(test_data4, arraysize(test_data4), true);

  // Detach |kDevice1| from |kMountPointB|.
  // kDevice1 -> kMountPointA
  // kDevice2 -> kMountPointB
  EXPECT_CALL(observer(), OnMediaDeviceAttached(_, _, _)).Times(2);
  EXPECT_CALL(observer(), OnMediaDeviceDetached(_)).Times(1);
  WriteToMtabAndRunLoop(test_data1, arraysize(test_data1), true);

  // Detach all devices.
  EXPECT_CALL(observer(), OnMediaDeviceAttached(_, _, _)).Times(0);
  EXPECT_CALL(observer(), OnMediaDeviceDetached(_)).Times(2);
  WriteToMtabAndRunLoop(NULL, 0, true);
}

TEST_F(MediaDeviceNotificationsLinuxTest, MultiDevicesOneMountPoint) {
  testing::Sequence mock_sequence;
  FilePath test_pathA = CreateMountPoint(kMountPointA, true);
  FilePath test_pathB = CreateMountPoint(kMountPointB, true);
  ASSERT_FALSE(test_pathA.empty());
  ASSERT_FALSE(test_pathB.empty());

  // |kDevice1| is most recently mounted at |kMountPointB|.
  // kDevice1 -> kMountPointA
  // kDevice2 -> kMountPointB
  // kDevice1 -> kMountPointB
  struct MtabTestData test_data1[] = {
    MtabTestData(kDevice1, test_pathA.value().c_str(), kValidFS),
    MtabTestData(kDevice2, test_pathB.value().c_str(), kValidFS),
    MtabTestData(kDevice1, test_pathB.value().c_str(), kValidFS),
  };
  EXPECT_CALL(observer(), OnMediaDeviceAttached(0, kDevice1, test_pathB))
      .Times(1);
  EXPECT_CALL(observer(), OnMediaDeviceDetached(_)).Times(0);
  WriteToMtabAndRunLoop(test_data1, arraysize(test_data1), true);

  // Attach |kDevice3| to |kMountPointB|.
  // |kDevice1| is inaccessible at its most recent mount point, so it is
  // detached and unavailable, even though it is still accessible via
  // |kMountPointA|.
  // kDevice1 -> kMountPointA
  // kDevice2 -> kMountPointB
  // kDevice1 -> kMountPointB
  // kDevice3 -> kMountPointB
  struct MtabTestData test_data2[] = {
    MtabTestData(kDevice3, test_pathB.value().c_str(), kValidFS),
  };
  EXPECT_CALL(observer(), OnMediaDeviceDetached(0)).Times(1);
  EXPECT_CALL(observer(), OnMediaDeviceAttached(1, kDevice3, test_pathB))
      .Times(1);
  WriteToMtabAndRunLoop(test_data2, arraysize(test_data2), false);

  // Detach all devices.
  EXPECT_CALL(observer(), OnMediaDeviceAttached(_, _, _)).Times(0);
  EXPECT_CALL(observer(), OnMediaDeviceDetached(1)).Times(1);
  WriteToMtabAndRunLoop(NULL, 0, true);
}

}  // namespace content
