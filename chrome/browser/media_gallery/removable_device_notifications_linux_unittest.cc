// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// RemovableDeviceNotificationsLinux unit tests.

#include "chrome/browser/media_gallery/removable_device_notifications_linux.h"

#include <mntent.h>
#include <stdio.h>

#include <string>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/system_monitor/system_monitor.h"
#include "base/test/mock_devices_changed_observer.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/media_gallery/media_storage_util.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

namespace {

using testing::_;

const char kValidFS[] = "vfat";
const char kInvalidFS[] = "invalidfs";

const char kInvalidPath[] = "invalid path does not exist";

const char kDevice1[] = "d1";
const char kDevice2[] = "d2";
const char kDevice3[] = "d3";
const char kDevice4[] = "d4";
const char kDevice5[] = "d5";

const char kInvalidDevice[] = "invalid_device";

const char kMountPointA[] = "mnt_a";
const char kMountPointB[] = "mnt_b";
const char kMountPointC[] = "mnt_c";

const char kDCIM[] = "DCIM";

struct TestDeviceData {
  const char* device_path;
  const char* unique_id;
  const char* device_name;
  MediaStorageUtil::Type type;
};

const TestDeviceData kTestDeviceData[] = {
  {kDevice1, "UUID:FFF0-000F", "TEST_USB_MODEL_1",
   MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM},
  {kDevice2, "VendorModelSerial:ComName:Model2010:89898989", "TEST_USB_MODEL_2",
   MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM},
  {kDevice3, "VendorModelSerial:::WEM319X792", "TEST_USB_MODEL_3",
   MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM},
  {kDevice4, "UUID:ABCD-1234", "TEST_USB_MODEL_4",
   MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM},
  {kDevice5, "UUID:743A91FD2349", "TEST_USB_MODEL_5",
   MediaStorageUtil::FIXED_MASS_STORAGE},
};

bool GetDeviceInfo(const FilePath& device_path, std::string* id,
                   string16* name, bool* removable) {
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
      return true;
    }
  }
  return false;
}

std::string GetDeviceId(const std::string& device) {
  for (size_t i = 0; i < arraysize(kTestDeviceData); i++) {
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
  RemovableDeviceNotificationsLinuxTestWrapper(const FilePath& path,
                                               MessageLoop* message_loop)
      : RemovableDeviceNotificationsLinux(path, &GetDeviceInfo),
        message_loop_(message_loop) {
  }

 private:
  // Avoids code deleting the object while there are references to it.
  // Aside from the base::RefCountedThreadSafe friend class, any attempts to
  // call this dtor will result in a compile-time error.
  ~RemovableDeviceNotificationsLinuxTestWrapper() {}

  virtual void OnFilePathChanged(const FilePath& path, bool error) OVERRIDE {
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
    mock_devices_changed_observer_.reset(new base::MockDevicesChangedObserver);
    system_monitor_.AddDevicesChangedObserver(
        mock_devices_changed_observer_.get());

    // Create and set up a temp dir with files for the test.
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    FilePath test_dir = scoped_temp_dir_.path().AppendASCII("test_etc");
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
    notifications_->Init();
    message_loop_.RunAllPending();
  }

  virtual void TearDown() OVERRIDE {
    message_loop_.RunAllPending();
    notifications_ = NULL;
    system_monitor_.RemoveDevicesChangedObserver(
        mock_devices_changed_observer_.get());
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
  FilePath CreateMountPointWithDCIMDir(const std::string& dir) {
    return CreateMountPoint(dir, true  /* create DCIM dir */);
  }

  // Create a directory named |dir| relative to the test directory.
  // It does not have a DCIM directory, so RemovableDeviceNotificationsLinux
  // does not recognizes it as a media directory.
  FilePath CreateMountPointWithoutDCIMDir(const std::string& dir) {
    return CreateMountPoint(dir, false  /* do not create DCIM dir */);
  }

  void RemoveDCIMDirFromMountPoint(const std::string& dir) {
    FilePath dcim(scoped_temp_dir_.path().AppendASCII(dir).AppendASCII(kDCIM));
    file_util::Delete(dcim, false);
  }

  base::MockDevicesChangedObserver& observer() {
    return *mock_devices_changed_observer_;
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
  FilePath CreateMountPoint(const std::string& dir, bool with_dcim_dir) {
    FilePath return_path(scoped_temp_dir_.path());
    return_path = return_path.AppendASCII(dir);
    FilePath path(return_path);
    if (with_dcim_dir)
      path = path.AppendASCII(kDCIM);
    if (!file_util::CreateDirectory(path))
      return FilePath();
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

  // SystemMonitor and DevicesChangedObserver to hook together to test.
  base::SystemMonitor system_monitor_;
  scoped_ptr<base::MockDevicesChangedObserver> mock_devices_changed_observer_;

  // Temporary directory for created test data.
  ScopedTempDir scoped_temp_dir_;
  // Path to the test mtab file.
  FilePath mtab_file_;

  scoped_refptr<RemovableDeviceNotificationsLinuxTestWrapper> notifications_;

  DISALLOW_COPY_AND_ASSIGN(RemovableDeviceNotificationLinuxTest);
};

// Simple test case where we attach and detach a media device.
TEST_F(RemovableDeviceNotificationLinuxTest, BasicAttachDetach) {
  testing::Sequence mock_sequence;
  FilePath test_path = CreateMountPointWithDCIMDir(kMountPointA);
  ASSERT_FALSE(test_path.empty());
  MtabTestData test_data[] = {
    MtabTestData(kDevice1, kInvalidPath, kValidFS),
    MtabTestData(kDevice2, test_path.value(), kValidFS),
  };
  // Only |kDevice2| should be attached, since |kDevice1| has a bad path.
  EXPECT_CALL(observer(), OnRemovableStorageAttached(GetDeviceId(kDevice2),
                                                     GetDeviceName(kDevice2),
                                                     test_path.value()))
      .InSequence(mock_sequence);
  AppendToMtabAndRunLoop(test_data, arraysize(test_data));

  // |kDevice2| should be detached here.
  EXPECT_CALL(observer(), OnRemovableStorageDetached(GetDeviceId(kDevice2)))
      .InSequence(mock_sequence);
  WriteEmptyMtabAndRunLoop();
}

// Only mount points with DCIM directories are recognized.
TEST_F(RemovableDeviceNotificationLinuxTest, DCIM) {
  testing::Sequence mock_sequence;
  FilePath test_path_a = CreateMountPointWithDCIMDir(kMountPointA);
  ASSERT_FALSE(test_path_a.empty());
  MtabTestData test_data1[] = {
    MtabTestData(kDevice1, test_path_a.value(), kValidFS),
  };
  // |kDevice1| should be attached as expected.
  EXPECT_CALL(observer(), OnRemovableStorageAttached(GetDeviceId(kDevice1),
                                                     GetDeviceName(kDevice1),
                                                     test_path_a.value()))
      .InSequence(mock_sequence);
  AppendToMtabAndRunLoop(test_data1, arraysize(test_data1));

  // This should do nothing, since |kMountPointB| does not have a DCIM dir.
  FilePath test_path_b = CreateMountPointWithoutDCIMDir(kMountPointB);
  ASSERT_FALSE(test_path_b.empty());
  MtabTestData test_data2[] = {
    MtabTestData(kDevice2, test_path_b.value(), kValidFS),
  };
  AppendToMtabAndRunLoop(test_data2, arraysize(test_data2));

  // |kDevice1| should be detached as expected.
  EXPECT_CALL(observer(), OnRemovableStorageDetached(GetDeviceId(kDevice1)))
      .InSequence(mock_sequence);
  WriteEmptyMtabAndRunLoop();
}

// More complicated test case with multiple devices on multiple mount points.
TEST_F(RemovableDeviceNotificationLinuxTest, SwapMountPoints) {
  FilePath test_path_a = CreateMountPointWithDCIMDir(kMountPointA);
  FilePath test_path_b = CreateMountPointWithDCIMDir(kMountPointB);
  ASSERT_FALSE(test_path_a.empty());
  ASSERT_FALSE(test_path_b.empty());

  // Attach two devices.
  // kDevice1 -> kMountPointA
  // kDevice2 -> kMountPointB
  MtabTestData test_data1[] = {
    MtabTestData(kDevice1, test_path_a.value(), kValidFS),
    MtabTestData(kDevice2, test_path_b.value(), kValidFS),
  };
  EXPECT_CALL(observer(), OnRemovableStorageAttached(_, _, _)).Times(2);
  EXPECT_CALL(observer(), OnRemovableStorageDetached(_)).Times(0);
  AppendToMtabAndRunLoop(test_data1, arraysize(test_data1));

  // Detach two devices from old mount points and attach the devices at new
  // mount points.
  // kDevice1 -> kMountPointB
  // kDevice2 -> kMountPointA
  MtabTestData test_data2[] = {
    MtabTestData(kDevice1, test_path_b.value(), kValidFS),
    MtabTestData(kDevice2, test_path_a.value(), kValidFS),
  };
  EXPECT_CALL(observer(), OnRemovableStorageAttached(_, _, _)).Times(2);
  EXPECT_CALL(observer(), OnRemovableStorageDetached(_)).Times(2);
  OverwriteMtabAndRunLoop(test_data2, arraysize(test_data2));

  // Detach all devices.
  EXPECT_CALL(observer(), OnRemovableStorageAttached(_, _, _)).Times(0);
  EXPECT_CALL(observer(), OnRemovableStorageDetached(_)).Times(2);
  WriteEmptyMtabAndRunLoop();
}

// More complicated test case with multiple devices on multiple mount points.
TEST_F(RemovableDeviceNotificationLinuxTest, MultiDevicesMultiMountPoints) {
  FilePath test_path_a = CreateMountPointWithDCIMDir(kMountPointA);
  FilePath test_path_b = CreateMountPointWithDCIMDir(kMountPointB);
  ASSERT_FALSE(test_path_a.empty());
  ASSERT_FALSE(test_path_b.empty());

  // Attach two devices.
  // kDevice1 -> kMountPointA *
  // kDevice2 -> kMountPointB *
  MtabTestData test_data1[] = {
    MtabTestData(kDevice1, test_path_a.value(), kValidFS),
    MtabTestData(kDevice2, test_path_b.value(), kValidFS),
  };
  EXPECT_CALL(observer(), OnRemovableStorageAttached(_, _, _)).Times(2);
  EXPECT_CALL(observer(), OnRemovableStorageDetached(_)).Times(0);
  AppendToMtabAndRunLoop(test_data1, arraysize(test_data1));

  // Attach |kDevice1| to |kMountPointB|.
  // |kDevice2| is inaccessible, so it is detached. |kDevice1| has been
  // attached at |kMountPointB|, but is still accessible from |kMountPointA|.
  // kDevice1 -> kMountPointA *
  // kDevice2 -> kMountPointB
  // kDevice1 -> kMountPointB
  MtabTestData test_data2[] = {
    MtabTestData(kDevice1, test_path_b.value(), kValidFS),
  };
  EXPECT_CALL(observer(), OnRemovableStorageAttached(_, _, _)).Times(0);
  EXPECT_CALL(observer(), OnRemovableStorageDetached(_)).Times(1);
  AppendToMtabAndRunLoop(test_data2, arraysize(test_data2));

  // Detach |kDevice1| from |kMountPointA|, causing a detach and attach event.
  // kDevice2 -> kMountPointB
  // kDevice1 -> kMountPointB *
  MtabTestData test_data3[] = {
    MtabTestData(kDevice2, test_path_b.value(), kValidFS),
    MtabTestData(kDevice1, test_path_b.value(), kValidFS),
  };
  EXPECT_CALL(observer(), OnRemovableStorageAttached(_, _, _)).Times(1);
  EXPECT_CALL(observer(), OnRemovableStorageDetached(_)).Times(1);
  OverwriteMtabAndRunLoop(test_data3, arraysize(test_data3));

  // Attach |kDevice1| to |kMountPointA|.
  // kDevice2 -> kMountPointB
  // kDevice1 -> kMountPointB *
  // kDevice1 -> kMountPointA
  MtabTestData test_data4[] = {
    MtabTestData(kDevice1, test_path_a.value(), kValidFS),
  };
  EXPECT_CALL(observer(), OnRemovableStorageAttached(_, _, _)).Times(0);
  EXPECT_CALL(observer(), OnRemovableStorageDetached(_)).Times(0);
  AppendToMtabAndRunLoop(test_data4, arraysize(test_data4));

  // Detach |kDevice1| from |kMountPointB|.
  // kDevice1 -> kMountPointA *
  // kDevice2 -> kMountPointB *
  EXPECT_CALL(observer(), OnRemovableStorageAttached(_, _, _)).Times(2);
  EXPECT_CALL(observer(), OnRemovableStorageDetached(_)).Times(1);
  OverwriteMtabAndRunLoop(test_data1, arraysize(test_data1));

  // Detach all devices.
  EXPECT_CALL(observer(), OnRemovableStorageAttached(_, _, _)).Times(0);
  EXPECT_CALL(observer(), OnRemovableStorageDetached(_)).Times(2);
  WriteEmptyMtabAndRunLoop();
}

TEST_F(RemovableDeviceNotificationLinuxTest,
       MultipleMountPointsWithNonDCIMDevices) {
  FilePath test_path_a = CreateMountPointWithDCIMDir(kMountPointA);
  FilePath test_path_b = CreateMountPointWithDCIMDir(kMountPointB);
  ASSERT_FALSE(test_path_a.empty());
  ASSERT_FALSE(test_path_b.empty());

  // Attach to one first.
  // kDevice1 -> kMountPointA *
  MtabTestData test_data1[] = {
    MtabTestData(kDevice1, test_path_a.value(), kValidFS),
  };
  EXPECT_CALL(observer(), OnRemovableStorageAttached(_, _, _)).Times(1);
  EXPECT_CALL(observer(), OnRemovableStorageDetached(_)).Times(0);
  AppendToMtabAndRunLoop(test_data1, arraysize(test_data1));

  // Attach |kDevice1| to |kMountPointB|.
  // kDevice1 -> kMountPointA *
  // kDevice1 -> kMountPointB
  MtabTestData test_data2[] = {
    MtabTestData(kDevice1, test_path_b.value(), kValidFS),
  };
  EXPECT_CALL(observer(), OnRemovableStorageAttached(_, _, _)).Times(0);
  EXPECT_CALL(observer(), OnRemovableStorageDetached(_)).Times(0);
  AppendToMtabAndRunLoop(test_data2, arraysize(test_data2));

  // Attach |kDevice4| (a non-DCIM device) to |kMountPointA|.
  // kDevice1 -> kMountPointA
  // kDevice1 -> kMountPointB *
  // kDevice4 -> kMountPointA
  MtabTestData test_data3[] = {
    MtabTestData(kDevice4, test_path_a.value(), kValidFS),
  };
  EXPECT_CALL(observer(), OnRemovableStorageAttached(_, _, _)).Times(1);
  EXPECT_CALL(observer(), OnRemovableStorageDetached(_)).Times(1);
  RemoveDCIMDirFromMountPoint(kMountPointA);
  AppendToMtabAndRunLoop(test_data3, arraysize(test_data3));

  // Detach |kDevice4|.
  // kDevice1 -> kMountPointA
  // kDevice1 -> kMountPointB *
  MtabTestData test_data4[] = {
    MtabTestData(kDevice1, test_path_a.value(), kValidFS),
    MtabTestData(kDevice1, test_path_b.value(), kValidFS),
  };
  EXPECT_CALL(observer(), OnRemovableStorageAttached(_, _, _)).Times(0);
  EXPECT_CALL(observer(), OnRemovableStorageDetached(_)).Times(0);
  OverwriteMtabAndRunLoop(test_data4, arraysize(test_data4));

  // Detach |kDevice1| from |kMountPointA|.
  // kDevice1 -> kMountPointB *
  EXPECT_CALL(observer(), OnRemovableStorageAttached(_, _, _)).Times(0);
  EXPECT_CALL(observer(), OnRemovableStorageDetached(_)).Times(0);
  OverwriteMtabAndRunLoop(test_data2, arraysize(test_data2));

  // Detach all devices.
  EXPECT_CALL(observer(), OnRemovableStorageAttached(_, _, _)).Times(0);
  EXPECT_CALL(observer(), OnRemovableStorageDetached(_)).Times(1);
  WriteEmptyMtabAndRunLoop();
}

TEST_F(RemovableDeviceNotificationLinuxTest, MountLookUp) {
  FilePath test_path_a = CreateMountPointWithDCIMDir(kMountPointA);
  FilePath test_path_b = CreateMountPointWithoutDCIMDir(kMountPointB);
  FilePath test_path_c = CreateMountPointWithoutDCIMDir(kMountPointC);
  ASSERT_FALSE(test_path_a.empty());
  ASSERT_FALSE(test_path_b.empty());
  ASSERT_FALSE(test_path_c.empty());

  // Attach to one first.
  // kDevice1 -> kMountPointA *
  // kDevice4 -> kMountPointB
  // kDevice5 -> kMountPointC
  MtabTestData test_data1[] = {
    MtabTestData(kDevice1, test_path_a.value(), kValidFS),
    MtabTestData(kDevice4, test_path_b.value(), kValidFS),
    MtabTestData(kDevice5, test_path_c.value(), kValidFS),
  };
  EXPECT_CALL(observer(), OnRemovableStorageAttached(_, _, _)).Times(1);
  EXPECT_CALL(observer(), OnRemovableStorageDetached(_)).Times(0);
  AppendToMtabAndRunLoop(test_data1, arraysize(test_data1));

  EXPECT_EQ(test_path_a,
            notifier()->GetDeviceMountPoint(GetDeviceId(kDevice1)));
  EXPECT_EQ(test_path_b,
            notifier()->GetDeviceMountPoint(GetDeviceId(kDevice4)));
  EXPECT_EQ(test_path_c,
            notifier()->GetDeviceMountPoint(GetDeviceId(kDevice5)));
  EXPECT_EQ(FilePath(),
            notifier()->GetDeviceMountPoint(GetDeviceId(kInvalidDevice)));

  // Make |kDevice1| mounted in multiple places.
  // kDevice1 -> kMountPointA *
  // kDevice1 -> kMountPointB
  // kDevice1 -> kMountPointC
  MtabTestData test_data2[] = {
    MtabTestData(kDevice1, test_path_a.value(), kValidFS),
    MtabTestData(kDevice1, test_path_b.value(), kValidFS),
    MtabTestData(kDevice1, test_path_c.value(), kValidFS),
  };
  EXPECT_CALL(observer(), OnRemovableStorageAttached(_, _, _)).Times(0);
  EXPECT_CALL(observer(), OnRemovableStorageDetached(_)).Times(0);
  // Add DCIM dirs.
  CreateMountPointWithDCIMDir(kMountPointB);
  CreateMountPointWithDCIMDir(kMountPointC);
  OverwriteMtabAndRunLoop(test_data2, arraysize(test_data2));

  EXPECT_EQ(test_path_a,
            notifier()->GetDeviceMountPoint(GetDeviceId(kDevice1)));

  // Unmount |kDevice1| from |kMountPointA|.
  // kDevice1 -> kMountPointB *
  // kDevice1 -> kMountPointC
  // Either |kMountPointB| or |kMountPointC| is a valid new default, but
  // it turns out that it will be B.
  MtabTestData test_data3[] = {
    MtabTestData(kDevice1, test_path_b.value(), kValidFS),
  };
  EXPECT_CALL(observer(), OnRemovableStorageAttached(_, _, _)).Times(1);
  EXPECT_CALL(observer(), OnRemovableStorageDetached(_)).Times(1);
  OverwriteMtabAndRunLoop(test_data3, arraysize(test_data3));

  EXPECT_EQ(test_path_b,
            notifier()->GetDeviceMountPoint(GetDeviceId(kDevice1)));

  // Mount a non-removable device in multiple places.
  // kDevice5 -> kMountPointA
  // kDevice5 -> kMountPointB
  // kDevice5 -> kMountPointC
  MtabTestData test_data4[] = {
    MtabTestData(kDevice5, test_path_a.value(), kValidFS),
    MtabTestData(kDevice5, test_path_b.value(), kValidFS),
    MtabTestData(kDevice5, test_path_c.value(), kValidFS),
  };
  EXPECT_CALL(observer(), OnRemovableStorageAttached(_, _, _)).Times(0);
  EXPECT_CALL(observer(), OnRemovableStorageDetached(_)).Times(1);
  // Remove DCIM dirs.
  RemoveDCIMDirFromMountPoint(kMountPointA);
  RemoveDCIMDirFromMountPoint(kMountPointB);
  RemoveDCIMDirFromMountPoint(kMountPointC);
  OverwriteMtabAndRunLoop(test_data4, arraysize(test_data4));

  // Any of the mount points would be valid.
  EXPECT_EQ(test_path_a.value(),
            notifier()->GetDeviceMountPoint(GetDeviceId(kDevice5)).value());
}

TEST_F(RemovableDeviceNotificationLinuxTest, DeviceLookUp) {
  FilePath test_path_a = CreateMountPointWithDCIMDir(kMountPointA);
  FilePath test_path_b = CreateMountPointWithoutDCIMDir(kMountPointB);
  FilePath test_path_c = CreateMountPointWithoutDCIMDir(kMountPointC);
  ASSERT_FALSE(test_path_a.empty());
  ASSERT_FALSE(test_path_b.empty());
  ASSERT_FALSE(test_path_c.empty());

  // Attach to one first.
  // kDevice1 -> kMountPointA *
  // kDevice4 -> kMountPointB
  // kDevice5 -> kMountPointC
  MtabTestData test_data1[] = {
    MtabTestData(kDevice1, test_path_a.value(), kValidFS),
    MtabTestData(kDevice4, test_path_b.value(), kValidFS),
    MtabTestData(kDevice5, test_path_c.value(), kValidFS),
  };
  EXPECT_CALL(observer(), OnRemovableStorageAttached(_, _, _)).Times(1);
  EXPECT_CALL(observer(), OnRemovableStorageDetached(_)).Times(0);
  AppendToMtabAndRunLoop(test_data1, arraysize(test_data1));

  FilePath mount_point;
  EXPECT_EQ(GetDeviceId(kDevice1),
            notifier()->GetDeviceIdForPath(test_path_a, &mount_point));
  EXPECT_EQ(test_path_a.value(), mount_point.value());
  EXPECT_EQ(GetDeviceId(kDevice4),
            notifier()->GetDeviceIdForPath(test_path_b, &mount_point));
  EXPECT_EQ(test_path_b.value(), mount_point.value());
  EXPECT_EQ(GetDeviceId(kDevice5),
            notifier()->GetDeviceIdForPath(test_path_c, NULL));

  // An invalid path.
  EXPECT_EQ(std::string(),
            notifier()->GetDeviceIdForPath(FilePath(kInvalidPath), NULL));

  // Test filling in of the mount point.
  EXPECT_EQ(GetDeviceId(kDevice1), notifier()->GetDeviceIdForPath(
      test_path_a.Append("some/other/path"), &mount_point));
  EXPECT_EQ(test_path_a.value(), mount_point.value());

  // One device attached at multiple points.
  // kDevice1 -> kMountPointA *
  // kDevice5 -> kMountPointB
  // kDevice5 -> kMountPointC
  MtabTestData test_data2[] = {
    MtabTestData(kDevice1, test_path_a.value(), kValidFS),
    MtabTestData(kDevice5, test_path_b.value(), kValidFS),
    MtabTestData(kDevice5, test_path_c.value(), kValidFS),
  };
  EXPECT_CALL(observer(), OnRemovableStorageAttached(_, _, _)).Times(0);
  EXPECT_CALL(observer(), OnRemovableStorageDetached(_)).Times(0);
  AppendToMtabAndRunLoop(test_data2, arraysize(test_data2));

  EXPECT_EQ(GetDeviceId(kDevice1),
            notifier()->GetDeviceIdForPath(test_path_a, NULL));
  EXPECT_EQ(GetDeviceId(kDevice5),
            notifier()->GetDeviceIdForPath(test_path_b, NULL));
  EXPECT_EQ(GetDeviceId(kDevice5),
            notifier()->GetDeviceIdForPath(test_path_c, NULL));
}

}  // namespace

}  // namespace chrome
