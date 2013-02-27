// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// RemovableDeviceNotificationsLinux unit tests.

#include "chrome/browser/storage_monitor/removable_device_notifications_linux.h"

#include <mntent.h>
#include <stdio.h>

#include <string>

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/storage_monitor/media_device_notifications_utils.h"
#include "chrome/browser/storage_monitor/media_storage_util.h"
#include "chrome/browser/storage_monitor/mock_removable_storage_observer.h"
#include "chrome/browser/storage_monitor/removable_device_constants.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

namespace {

const char kValidFS[] = "vfat";
const char kInvalidFS[] = "invalidfs";

const char kInvalidPath[] = "invalid path does not exist";

const char kDeviceDCIM1[] = "d1";
const char kDeviceDCIM2[] = "d2";
const char kDeviceDCIM3[] = "d3";
const char kDeviceNoDCIM[] = "d4";
const char kDeviceFixed[] = "d5";

const char kInvalidDevice[] = "invalid_device";

const char kMountPointA[] = "mnt_a";
const char kMountPointB[] = "mnt_b";
const char kMountPointC[] = "mnt_c";

struct TestDeviceData {
  const char* device_path;
  const char* unique_id;
  const char* device_name;
  MediaStorageUtil::Type type;
  uint64 partition_size_in_bytes;
};

const TestDeviceData kTestDeviceData[] = {
  { kDeviceDCIM1, "UUID:FFF0-000F", "TEST_USB_MODEL_1",
    MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM, 88788 },
  { kDeviceDCIM2, "VendorModelSerial:ComName:Model2010:8989",
    "TEST_USB_MODEL_2", MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM,
    8773 },
  { kDeviceDCIM3, "VendorModelSerial:::WEM319X792", "TEST_USB_MODEL_3",
    MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM, 22837 },
  { kDeviceNoDCIM, "UUID:ABCD-1234", "TEST_USB_MODEL_4",
    MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM, 512 },
  { kDeviceFixed, "UUID:743A-2349", "743A-2349",
    MediaStorageUtil::FIXED_MASS_STORAGE, 17282 },
};

void GetDeviceInfo(const base::FilePath& device_path,
                   std::string* id,
                   string16* name,
                   bool* removable,
                   uint64* partition_size_in_bytes) {
  for (size_t i = 0; i < arraysize(kTestDeviceData); i++) {
    if (device_path.value() == kTestDeviceData[i].device_path) {
      if (id)
        *id = kTestDeviceData[i].unique_id;
      if (name)
        *name = ASCIIToUTF16(kTestDeviceData[i].device_name);
      if (removable) {
        MediaStorageUtil::Type type = kTestDeviceData[i].type;
        *removable =
          (type == MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM) ||
          (type == MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM);
      }
      if (partition_size_in_bytes)
        *partition_size_in_bytes = kTestDeviceData[i].partition_size_in_bytes;
      return;
    }
  }
  NOTREACHED();
}

uint64 GetDevicePartitionSize(const std::string& device) {
  for (size_t i = 0; i < arraysize(kTestDeviceData); ++i) {
    if (device == kTestDeviceData[i].device_path)
      return kTestDeviceData[i].partition_size_in_bytes;
  }
  return 0;
}

std::string GetDeviceId(const std::string& device) {
  for (size_t i = 0; i < arraysize(kTestDeviceData); ++i) {
    if (device == kTestDeviceData[i].device_path) {
      return MediaStorageUtil::MakeDeviceId(kTestDeviceData[i].type,
                                            kTestDeviceData[i].unique_id);
    }
  }
  if (device == kInvalidDevice) {
    return MediaStorageUtil::MakeDeviceId(MediaStorageUtil::FIXED_MASS_STORAGE,
                                          kInvalidDevice);
  }
  return std::string();
}

string16 GetDeviceNameWithSizeDetails(const std::string& device) {
  for (size_t i = 0; i < arraysize(kTestDeviceData); ++i) {
    if (device == kTestDeviceData[i].device_path) {
      return GetDisplayNameForDevice(
          kTestDeviceData[i].partition_size_in_bytes,
          ASCIIToUTF16(kTestDeviceData[i].device_name));
    }
  }
  return string16();
}

string16 GetDeviceName(const std::string& device) {
  for (size_t i = 0; i < arraysize(kTestDeviceData); i++) {
    if (device == kTestDeviceData[i].device_path)
      return ASCIIToUTF16(kTestDeviceData[i].device_name);
  }
  return string16();
}

class RemovableDeviceNotificationsLinuxTestWrapper
    : public RemovableDeviceNotificationsLinux {
 public:
  RemovableDeviceNotificationsLinuxTestWrapper(const base::FilePath& path,
                                               MessageLoop* message_loop)
      : RemovableDeviceNotificationsLinux(path, &GetDeviceInfo),
        message_loop_(message_loop) {
  }

 private:
  // Avoids code deleting the object while there are references to it.
  // Aside from the base::RefCountedThreadSafe friend class, any attempts to
  // call this dtor will result in a compile-time error.
  virtual ~RemovableDeviceNotificationsLinuxTestWrapper() {}

  virtual void OnFilePathChanged(const base::FilePath& path,
                                 bool error) OVERRIDE {
    RemovableDeviceNotificationsLinux::OnFilePathChanged(path, error);
    message_loop_->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }

  MessageLoop* message_loop_;

  DISALLOW_COPY_AND_ASSIGN(RemovableDeviceNotificationsLinuxTestWrapper);
};

class RemovableDeviceNotificationLinuxTest : public testing::Test {
 public:
  struct MtabTestData {
    MtabTestData(const std::string& mount_device,
                 const std::string& mount_point,
                 const std::string& mount_type)
        : mount_device(mount_device),
          mount_point(mount_point),
          mount_type(mount_type) {
    }

    const std::string mount_device;
    const std::string mount_point;
    const std::string mount_type;
  };

  RemovableDeviceNotificationLinuxTest()
      : message_loop_(MessageLoop::TYPE_IO),
        file_thread_(content::BrowserThread::FILE, &message_loop_) {
  }
  virtual ~RemovableDeviceNotificationLinuxTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    // Create and set up a temp dir with files for the test.
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    base::FilePath test_dir = scoped_temp_dir_.path().AppendASCII("test_etc");
    ASSERT_TRUE(file_util::CreateDirectory(test_dir));
    mtab_file_ = test_dir.AppendASCII("test_mtab");
    MtabTestData initial_test_data[] = {
      MtabTestData("dummydevice", "dummydir", kInvalidFS),
    };
    WriteToMtab(initial_test_data,
                arraysize(initial_test_data),
                true  /* overwrite */);

    // Initialize the test subject.
    notifications_ = new RemovableDeviceNotificationsLinuxTestWrapper(
        mtab_file_, &message_loop_);
    mock_storage_observer_.reset(new MockRemovableStorageObserver);
    notifications_->AddObserver(mock_storage_observer_.get());

    notifications_->Init();
    message_loop_.RunUntilIdle();
  }

  virtual void TearDown() OVERRIDE {
    message_loop_.RunUntilIdle();
    notifications_->RemoveObserver(mock_storage_observer_.get());
    notifications_ = NULL;
  }

  // Append mtab entries from the |data| array of size |data_size| to the mtab
  // file, and run the message loop.
  void AppendToMtabAndRunLoop(const MtabTestData* data, size_t data_size) {
    WriteToMtab(data, data_size, false  /* do not overwrite */);
    message_loop_.Run();
  }

  // Overwrite the mtab file with mtab entries from the |data| array of size
  // |data_size|, and run the message loop.
  void OverwriteMtabAndRunLoop(const MtabTestData* data, size_t data_size) {
    WriteToMtab(data, data_size, true  /* overwrite */);
    message_loop_.Run();
  }

  // Simplied version of OverwriteMtabAndRunLoop() that just deletes all the
  // entries in the mtab file.
  void WriteEmptyMtabAndRunLoop() {
    OverwriteMtabAndRunLoop(NULL,  // No data.
                            0);    // No data length.
  }

  // Create a directory named |dir| relative to the test directory.
  // It has a DCIM directory, so RemovableDeviceNotificationsLinux recognizes it
  // as a media directory.
  base::FilePath CreateMountPointWithDCIMDir(const std::string& dir) {
    return CreateMountPoint(dir, true  /* create DCIM dir */);
  }

  // Create a directory named |dir| relative to the test directory.
  // It does not have a DCIM directory, so RemovableDeviceNotificationsLinux
  // does not recognizes it as a media directory.
  base::FilePath CreateMountPointWithoutDCIMDir(const std::string& dir) {
    return CreateMountPoint(dir, false  /* do not create DCIM dir */);
  }

  void RemoveDCIMDirFromMountPoint(const std::string& dir) {
    base::FilePath dcim =
        scoped_temp_dir_.path().AppendASCII(dir).Append(kDCIMDirectoryName);
    file_util::Delete(dcim, false);
  }

  MockRemovableStorageObserver& observer() {
    return *mock_storage_observer_;
  }

  RemovableDeviceNotificationsLinux* notifier() {
    return notifications_.get();
  }

 private:
  // Create a directory named |dir| relative to the test directory.
  // Set |with_dcim_dir| to true if the created directory will have a "DCIM"
  // subdirectory.
  // Returns the full path to the created directory on success, or an empty
  // path on failure.
  base::FilePath CreateMountPoint(const std::string& dir, bool with_dcim_dir) {
    base::FilePath return_path(scoped_temp_dir_.path());
    return_path = return_path.AppendASCII(dir);
    base::FilePath path(return_path);
    if (with_dcim_dir)
      path = path.Append(kDCIMDirectoryName);
    if (!file_util::CreateDirectory(path))
      return base::FilePath();
    return return_path;
  }

  // Write the test mtab data to |mtab_file_|.
  // |data| is an array of mtab entries.
  // |data_size| is the array size of |data|.
  // |overwrite| specifies whether to overwrite |mtab_file_|.
  void WriteToMtab(const MtabTestData* data,
                   size_t data_size,
                   bool overwrite) {
    FILE* file = setmntent(mtab_file_.value().c_str(), overwrite ? "w" : "a");
    ASSERT_TRUE(file);

    // Due to the glibc *mntent() interface design, which is out of our
    // control, the mtnent struct has several char* fields, even though
    // addmntent() does not write to them in the calls below. To make the
    // compiler happy while avoiding making additional copies of strings,
    // we just const_cast() the strings' c_str()s.
    // Assuming addmntent() does not write to the char* fields, this is safe.
    // It is unlikely the platforms this test suite runs on will have an
    // addmntent() implementation that does change the char* fields. If that
    // was ever the case, the test suite will start crashing or failing.
    mntent entry;
    static const char kMountOpts[] = "rw";
    entry.mnt_opts = const_cast<char*>(kMountOpts);
    entry.mnt_freq = 0;
    entry.mnt_passno = 0;
    for (size_t i = 0; i < data_size; ++i) {
      entry.mnt_fsname = const_cast<char*>(data[i].mount_device.c_str());
      entry.mnt_dir = const_cast<char*>(data[i].mount_point.c_str());
      entry.mnt_type = const_cast<char*>(data[i].mount_type.c_str());
      ASSERT_EQ(0, addmntent(file, &entry));
    }
    ASSERT_EQ(1, endmntent(file));
  }

  // The message loop and file thread to run tests on.
  MessageLoop message_loop_;
  content::TestBrowserThread file_thread_;

  scoped_ptr<MockRemovableStorageObserver> mock_storage_observer_;

  // Temporary directory for created test data.
  base::ScopedTempDir scoped_temp_dir_;
  // Path to the test mtab file.
  base::FilePath mtab_file_;

  scoped_refptr<RemovableDeviceNotificationsLinuxTestWrapper> notifications_;

  DISALLOW_COPY_AND_ASSIGN(RemovableDeviceNotificationLinuxTest);
};

// Simple test case where we attach and detach a media device.
TEST_F(RemovableDeviceNotificationLinuxTest, BasicAttachDetach) {
  base::FilePath test_path = CreateMountPointWithDCIMDir(kMountPointA);
  ASSERT_FALSE(test_path.empty());
  MtabTestData test_data[] = {
    MtabTestData(kDeviceDCIM2, test_path.value(), kValidFS),
    MtabTestData(kDeviceFixed, kInvalidPath, kValidFS),
  };
  // Only |kDeviceDCIM2| should be attached, since |kDeviceFixed| has a bad
  // path.
  AppendToMtabAndRunLoop(test_data, arraysize(test_data));

  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());
  EXPECT_EQ(GetDeviceId(kDeviceDCIM2), observer().last_attached().device_id);
  EXPECT_EQ(GetDeviceNameWithSizeDetails(kDeviceDCIM2),
            observer().last_attached().name);
  EXPECT_EQ(test_path.value(),
            observer().last_attached().location);

  // |kDeviceDCIM2| should be detached here.
  WriteEmptyMtabAndRunLoop();
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());
  EXPECT_EQ(GetDeviceId(kDeviceDCIM2), observer().last_detached().device_id);
}

// Only removable devices are recognized.
TEST_F(RemovableDeviceNotificationLinuxTest, Removable) {
  base::FilePath test_path_a = CreateMountPointWithDCIMDir(kMountPointA);
  ASSERT_FALSE(test_path_a.empty());
  MtabTestData test_data1[] = {
    MtabTestData(kDeviceDCIM1, test_path_a.value(), kValidFS),
  };
  // |kDeviceDCIM1| should be attached as expected.
  AppendToMtabAndRunLoop(test_data1, arraysize(test_data1));

  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());
  EXPECT_EQ(GetDeviceId(kDeviceDCIM1), observer().last_attached().device_id);
  EXPECT_EQ(GetDeviceNameWithSizeDetails(kDeviceDCIM1),
            observer().last_attached().name);
  EXPECT_EQ(test_path_a.value(),
            observer().last_attached().location);

  // This should do nothing, since |kDeviceFixed| is not removable.
  base::FilePath test_path_b = CreateMountPointWithoutDCIMDir(kMountPointB);
  ASSERT_FALSE(test_path_b.empty());
  MtabTestData test_data2[] = {
    MtabTestData(kDeviceFixed, test_path_b.value(), kValidFS),
  };
  AppendToMtabAndRunLoop(test_data2, arraysize(test_data2));
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());

  // |kDeviceDCIM1| should be detached as expected.
  WriteEmptyMtabAndRunLoop();
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());
  EXPECT_EQ(GetDeviceId(kDeviceDCIM1), observer().last_detached().device_id);

  // |kDeviceNoDCIM| should be attached as expected.
  MtabTestData test_data3[] = {
    MtabTestData(kDeviceNoDCIM, test_path_b.value(), kValidFS),
  };
  AppendToMtabAndRunLoop(test_data3, arraysize(test_data3));
  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());
  EXPECT_EQ(GetDeviceId(kDeviceNoDCIM), observer().last_attached().device_id);
  EXPECT_EQ(GetDeviceNameWithSizeDetails(kDeviceNoDCIM),
            observer().last_attached().name);
  EXPECT_EQ(test_path_b.value(),
            observer().last_attached().location);

  // |kDeviceNoDCIM| should be detached as expected.
  WriteEmptyMtabAndRunLoop();
  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(2, observer().detach_calls());
  EXPECT_EQ(GetDeviceId(kDeviceNoDCIM), observer().last_detached().device_id);
}

// More complicated test case with multiple devices on multiple mount points.
TEST_F(RemovableDeviceNotificationLinuxTest, SwapMountPoints) {
  base::FilePath test_path_a = CreateMountPointWithDCIMDir(kMountPointA);
  base::FilePath test_path_b = CreateMountPointWithDCIMDir(kMountPointB);
  ASSERT_FALSE(test_path_a.empty());
  ASSERT_FALSE(test_path_b.empty());

  // Attach two devices.
  // (*'d mounts are those RemovableStorageNotifications knows about.)
  // kDeviceDCIM1 -> kMountPointA *
  // kDeviceDCIM2 -> kMountPointB *
  MtabTestData test_data1[] = {
    MtabTestData(kDeviceDCIM1, test_path_a.value(), kValidFS),
    MtabTestData(kDeviceDCIM2, test_path_b.value(), kValidFS),
  };
  AppendToMtabAndRunLoop(test_data1, arraysize(test_data1));
  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());

  // Detach two devices from old mount points and attach the devices at new
  // mount points.
  // kDeviceDCIM1 -> kMountPointB *
  // kDeviceDCIM2 -> kMountPointA *
  MtabTestData test_data2[] = {
    MtabTestData(kDeviceDCIM1, test_path_b.value(), kValidFS),
    MtabTestData(kDeviceDCIM2, test_path_a.value(), kValidFS),
  };
  OverwriteMtabAndRunLoop(test_data2, arraysize(test_data2));
  EXPECT_EQ(4, observer().attach_calls());
  EXPECT_EQ(2, observer().detach_calls());

  // Detach all devices.
  WriteEmptyMtabAndRunLoop();
  EXPECT_EQ(4, observer().attach_calls());
  EXPECT_EQ(4, observer().detach_calls());
}

// More complicated test case with multiple devices on multiple mount points.
TEST_F(RemovableDeviceNotificationLinuxTest, MultiDevicesMultiMountPoints) {
  base::FilePath test_path_a = CreateMountPointWithDCIMDir(kMountPointA);
  base::FilePath test_path_b = CreateMountPointWithDCIMDir(kMountPointB);
  ASSERT_FALSE(test_path_a.empty());
  ASSERT_FALSE(test_path_b.empty());

  // Attach two devices.
  // (*'d mounts are those RemovableStorageNotifications knows about.)
  // kDeviceDCIM1 -> kMountPointA *
  // kDeviceDCIM2 -> kMountPointB *
  MtabTestData test_data1[] = {
    MtabTestData(kDeviceDCIM1, test_path_a.value(), kValidFS),
    MtabTestData(kDeviceDCIM2, test_path_b.value(), kValidFS),
  };
  AppendToMtabAndRunLoop(test_data1, arraysize(test_data1));
  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());

  // Attach |kDeviceDCIM1| to |kMountPointB|.
  // |kDeviceDCIM2| is inaccessible, so it is detached. |kDeviceDCIM1| has been
  // attached at |kMountPointB|, but is still accessible from |kMountPointA|.
  // kDeviceDCIM1 -> kMountPointA *
  // kDeviceDCIM2 -> kMountPointB
  // kDeviceDCIM1 -> kMountPointB
  MtabTestData test_data2[] = {
    MtabTestData(kDeviceDCIM1, test_path_b.value(), kValidFS),
  };
  AppendToMtabAndRunLoop(test_data2, arraysize(test_data2));
  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());

  // Detach |kDeviceDCIM1| from |kMountPointA|, causing a detach and attach
  // event.
  // kDeviceDCIM2 -> kMountPointB
  // kDeviceDCIM1 -> kMountPointB *
  MtabTestData test_data3[] = {
    MtabTestData(kDeviceDCIM2, test_path_b.value(), kValidFS),
    MtabTestData(kDeviceDCIM1, test_path_b.value(), kValidFS),
  };
  OverwriteMtabAndRunLoop(test_data3, arraysize(test_data3));
  EXPECT_EQ(3, observer().attach_calls());
  EXPECT_EQ(2, observer().detach_calls());

  // Attach |kDeviceDCIM1| to |kMountPointA|.
  // kDeviceDCIM2 -> kMountPointB
  // kDeviceDCIM1 -> kMountPointB *
  // kDeviceDCIM1 -> kMountPointA
  MtabTestData test_data4[] = {
    MtabTestData(kDeviceDCIM1, test_path_a.value(), kValidFS),
  };
  AppendToMtabAndRunLoop(test_data4, arraysize(test_data4));
  EXPECT_EQ(3, observer().attach_calls());
  EXPECT_EQ(2, observer().detach_calls());

  // Detach |kDeviceDCIM1| from |kMountPointB|.
  // kDeviceDCIM1 -> kMountPointA *
  // kDeviceDCIM2 -> kMountPointB *
  OverwriteMtabAndRunLoop(test_data1, arraysize(test_data1));
  EXPECT_EQ(5, observer().attach_calls());
  EXPECT_EQ(3, observer().detach_calls());

  // Detach all devices.
  WriteEmptyMtabAndRunLoop();
  EXPECT_EQ(5, observer().attach_calls());
  EXPECT_EQ(5, observer().detach_calls());
}

TEST_F(RemovableDeviceNotificationLinuxTest,
       MultipleMountPointsWithNonDCIMDevices) {
  base::FilePath test_path_a = CreateMountPointWithDCIMDir(kMountPointA);
  base::FilePath test_path_b = CreateMountPointWithDCIMDir(kMountPointB);
  ASSERT_FALSE(test_path_a.empty());
  ASSERT_FALSE(test_path_b.empty());

  // Attach to one first.
  // (*'d mounts are those RemovableStorageNotifications knows about.)
  // kDeviceDCIM1 -> kMountPointA *
  MtabTestData test_data1[] = {
    MtabTestData(kDeviceDCIM1, test_path_a.value(), kValidFS),
  };
  AppendToMtabAndRunLoop(test_data1, arraysize(test_data1));
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());

  // Attach |kDeviceDCIM1| to |kMountPointB|.
  // kDeviceDCIM1 -> kMountPointA *
  // kDeviceDCIM1 -> kMountPointB
  MtabTestData test_data2[] = {
    MtabTestData(kDeviceDCIM1, test_path_b.value(), kValidFS),
  };
  AppendToMtabAndRunLoop(test_data2, arraysize(test_data2));
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());

  // Attach |kDeviceFixed| (a non-removable device) to |kMountPointA|.
  // kDeviceDCIM1 -> kMountPointA
  // kDeviceDCIM1 -> kMountPointB *
  // kDeviceFixed -> kMountPointA
  MtabTestData test_data3[] = {
    MtabTestData(kDeviceFixed, test_path_a.value(), kValidFS),
  };
  RemoveDCIMDirFromMountPoint(kMountPointA);
  AppendToMtabAndRunLoop(test_data3, arraysize(test_data3));
  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());

  // Detach |kDeviceFixed|.
  // kDeviceDCIM1 -> kMountPointA
  // kDeviceDCIM1 -> kMountPointB *
  MtabTestData test_data4[] = {
    MtabTestData(kDeviceDCIM1, test_path_a.value(), kValidFS),
    MtabTestData(kDeviceDCIM1, test_path_b.value(), kValidFS),
  };
  CreateMountPointWithDCIMDir(kMountPointA);
  OverwriteMtabAndRunLoop(test_data4, arraysize(test_data4));
  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());

  // Attach |kDeviceNoDCIM| (a non-DCIM device) to |kMountPointB|.
  // kDeviceDCIM1  -> kMountPointA *
  // kDeviceDCIM1  -> kMountPointB
  // kDeviceNoDCIM -> kMountPointB *
  MtabTestData test_data5[] = {
    MtabTestData(kDeviceNoDCIM, test_path_b.value(), kValidFS),
  };
  file_util::Delete(test_path_b.Append(kDCIMDirectoryName), false);
  AppendToMtabAndRunLoop(test_data5, arraysize(test_data5));
  EXPECT_EQ(4, observer().attach_calls());
  EXPECT_EQ(2, observer().detach_calls());

  // Detach |kDeviceNoDCIM|.
  // kDeviceDCIM1 -> kMountPointA *
  // kDeviceDCIM1 -> kMountPointB
  MtabTestData test_data6[] = {
    MtabTestData(kDeviceDCIM1, test_path_a.value(), kValidFS),
    MtabTestData(kDeviceDCIM1, test_path_b.value(), kValidFS),
  };
  CreateMountPointWithDCIMDir(kMountPointB);
  OverwriteMtabAndRunLoop(test_data6, arraysize(test_data6));
  EXPECT_EQ(4, observer().attach_calls());
  EXPECT_EQ(3, observer().detach_calls());

  // Detach |kDeviceDCIM1| from |kMountPointB|.
  // kDeviceDCIM1 -> kMountPointA *
  OverwriteMtabAndRunLoop(test_data1, arraysize(test_data1));
  EXPECT_EQ(4, observer().attach_calls());
  EXPECT_EQ(3, observer().detach_calls());

  // Detach all devices.
  WriteEmptyMtabAndRunLoop();
  EXPECT_EQ(4, observer().attach_calls());
  EXPECT_EQ(4, observer().detach_calls());
}

TEST_F(RemovableDeviceNotificationLinuxTest, DeviceLookUp) {
  base::FilePath test_path_a = CreateMountPointWithDCIMDir(kMountPointA);
  base::FilePath test_path_b = CreateMountPointWithoutDCIMDir(kMountPointB);
  base::FilePath test_path_c = CreateMountPointWithoutDCIMDir(kMountPointC);
  ASSERT_FALSE(test_path_a.empty());
  ASSERT_FALSE(test_path_b.empty());
  ASSERT_FALSE(test_path_c.empty());

  // Attach to one first.
  // (*'d mounts are those RemovableStorageNotifications knows about.)
  // kDeviceDCIM1  -> kMountPointA *
  // kDeviceNoDCIM -> kMountPointB *
  // kDeviceFixed  -> kMountPointC
  MtabTestData test_data1[] = {
    MtabTestData(kDeviceDCIM1, test_path_a.value(), kValidFS),
    MtabTestData(kDeviceNoDCIM, test_path_b.value(), kValidFS),
    MtabTestData(kDeviceFixed, test_path_c.value(), kValidFS),
  };
  AppendToMtabAndRunLoop(test_data1, arraysize(test_data1));
  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());

  StorageMonitor::StorageInfo device_info;
  EXPECT_TRUE(notifier()->GetStorageInfoForPath(test_path_a, &device_info));
  EXPECT_EQ(GetDeviceId(kDeviceDCIM1), device_info.device_id);
  EXPECT_EQ(test_path_a.value(), device_info.location);
  EXPECT_EQ(GetDeviceName(kDeviceDCIM1), device_info.name);

  EXPECT_TRUE(notifier()->GetStorageInfoForPath(test_path_b, &device_info));
  EXPECT_EQ(GetDeviceId(kDeviceNoDCIM), device_info.device_id);
  EXPECT_EQ(test_path_b.value(), device_info.location);
  EXPECT_EQ(GetDeviceName(kDeviceNoDCIM), device_info.name);

  EXPECT_TRUE(notifier()->GetStorageInfoForPath(test_path_c, &device_info));
  EXPECT_EQ(GetDeviceId(kDeviceFixed), device_info.device_id);
  EXPECT_EQ(test_path_c.value(), device_info.location);
  EXPECT_EQ(GetDeviceName(kDeviceFixed), device_info.name);

  // An invalid path.
  EXPECT_FALSE(
      notifier()->GetStorageInfoForPath(base::FilePath(kInvalidPath), NULL));

  // Test filling in of the mount point.
  EXPECT_TRUE(
      notifier()->GetStorageInfoForPath(test_path_a.Append("some/other/path"),
      &device_info));
  EXPECT_EQ(GetDeviceId(kDeviceDCIM1), device_info.device_id);
  EXPECT_EQ(test_path_a.value(), device_info.location);
  EXPECT_EQ(GetDeviceName(kDeviceDCIM1), device_info.name);

  // One device attached at multiple points.
  // kDeviceDCIM1 -> kMountPointA *
  // kDeviceFixed -> kMountPointB
  // kDeviceFixed -> kMountPointC
  MtabTestData test_data2[] = {
    MtabTestData(kDeviceDCIM1, test_path_a.value(), kValidFS),
    MtabTestData(kDeviceFixed, test_path_b.value(), kValidFS),
    MtabTestData(kDeviceFixed, test_path_c.value(), kValidFS),
  };
  AppendToMtabAndRunLoop(test_data2, arraysize(test_data2));

  EXPECT_TRUE(notifier()->GetStorageInfoForPath(test_path_a, &device_info));
  EXPECT_EQ(GetDeviceId(kDeviceDCIM1), device_info.device_id);

  EXPECT_TRUE(notifier()->GetStorageInfoForPath(test_path_b, &device_info));
  EXPECT_EQ(GetDeviceId(kDeviceFixed), device_info.device_id);

  EXPECT_TRUE(notifier()->GetStorageInfoForPath(test_path_c, &device_info));
  EXPECT_EQ(GetDeviceId(kDeviceFixed), device_info.device_id);

  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());
}

TEST_F(RemovableDeviceNotificationLinuxTest, DevicePartitionSize) {
  base::FilePath test_path_a = CreateMountPointWithDCIMDir(kMountPointA);
  base::FilePath test_path_b = CreateMountPointWithoutDCIMDir(kMountPointB);
  ASSERT_FALSE(test_path_a.empty());
  ASSERT_FALSE(test_path_b.empty());

  MtabTestData test_data1[] = {
    MtabTestData(kDeviceDCIM1, test_path_a.value(), kValidFS),
    MtabTestData(kDeviceNoDCIM, test_path_b.value(), kValidFS),
    MtabTestData(kDeviceFixed, kInvalidPath, kInvalidFS),
  };
  AppendToMtabAndRunLoop(test_data1, arraysize(test_data1));
  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());

  EXPECT_EQ(GetDevicePartitionSize(kDeviceDCIM1),
            notifier()->GetStorageSize(test_path_a.value()));
  EXPECT_EQ(GetDevicePartitionSize(kDeviceNoDCIM),
            notifier()->GetStorageSize(test_path_b.value()));
  EXPECT_EQ(GetDevicePartitionSize(kInvalidPath),
            notifier()->GetStorageSize(kInvalidPath));
}

}  // namespace

}  // namespace chrome
