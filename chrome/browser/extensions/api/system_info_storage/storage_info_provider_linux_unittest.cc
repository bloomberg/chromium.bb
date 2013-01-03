// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// StorageInfoProviderLinux unit tests.

#include "chrome/browser/extensions/api/system_info_storage/storage_info_provider_linux.h"

#include <map>

#include "base/file_util.h"
#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using api::experimental_system_info_storage::FromStorageUnitTypeString;
using api::experimental_system_info_storage::StorageUnitInfo;
using api::experimental_system_info_storage::ToString;

namespace {

// Test data in mntent format.
const char mtab_test_data[] =
  "proc /proc proc rw,noexec,nosuid,nodev 0 0\n"
  "sysfs /sys sysfs rw,noexec,nosuid,nodev 0 0\n"
  "udev /dev devtmpfs rw,mode=0755 0 0\n"
  "/dev/sda1 /boot ext4 rw 0 0\n"
  "/dev/sda2 / ext4 rw 0 0\n"
  "/dev/sdb /home ext4 rw 0 0";

struct TestMountEntry {
  std::string mnt_path;
  std::string type;
  double capacity;
  double available_capacity;
};

const TestMountEntry mount_entries[] = {
  { "/boot", "harddisk", 100, 50 },
  { "/", "harddisk", 200, 100 },
  { "/home", "removable", 300, 100 }
};

typedef std::map<std::string, struct TestMountEntry> TestMountEntryMap;

}  // namespace

class StorageInfoProviderLinuxWrapper : public StorageInfoProviderLinux {
 public:
  explicit StorageInfoProviderLinuxWrapper(const FilePath& mtab_path)
    : StorageInfoProviderLinux(mtab_path) {
    for (size_t i = 0; i < arraysize(mount_entries); i++) {
      std::string mnt_path = mount_entries[i].mnt_path;
      mount_entry_map_[mnt_path] = mount_entries[i];
    }
   }

  TestMountEntryMap& storage_map() { return mount_entry_map_; }

 private:
  friend class StorageInfoProviderLinuxTest;

  virtual ~StorageInfoProviderLinuxWrapper() {}

  virtual bool QueryUnitInfo(const std::string& mount_path,
                             StorageUnitInfo* info) OVERRIDE {
    std::string type;
    if (!QueryStorageType(mount_path, &type))
      return false;
    info->id = mount_path;
    info->type = FromStorageUnitTypeString(type);
    info->capacity = mount_entry_map_[mount_path].capacity;
    info->available_capacity = mount_entry_map_[mount_path].available_capacity;
    return true;
  }

  virtual bool QueryStorageType(const std::string& mount_path,
                                std::string* type) OVERRIDE {
    if (!ContainsKey(mount_entry_map_, mount_path))
      return false;
    *type = mount_entry_map_[mount_path].type;
    return true;
  }

  TestMountEntryMap mount_entry_map_;
};

class StorageInfoProviderLinuxTest : public testing::Test {
 public:
  StorageInfoProviderLinuxTest() {}
  virtual ~StorageInfoProviderLinuxTest() {}

  bool QueryInfo(StorageInfo* info) {
    return storage_info_provider_->QueryInfo(info);
  }

 protected:
  virtual void SetUp() OVERRIDE {
    // Create and set up a temp file for mtab data.
    ASSERT_TRUE(file_util::CreateTemporaryFile(&mtab_file_));
    int bytes = file_util::WriteFile(mtab_file_, mtab_test_data,
                                     strlen(mtab_test_data));
    ASSERT_EQ(static_cast<int>(strlen(mtab_test_data)), bytes);
    storage_info_provider_ = new StorageInfoProviderLinuxWrapper(mtab_file_);
  }

  virtual void TearDown() OVERRIDE {
    file_util::Delete(mtab_file_, false);
  }

  scoped_refptr<StorageInfoProviderLinuxWrapper> storage_info_provider_;
  FilePath mtab_file_;
};

TEST_F(StorageInfoProviderLinuxTest, QueryInfo) {
  StorageInfo info;
  ASSERT_TRUE(QueryInfo(&info));

  TestMountEntryMap& entries = storage_info_provider_->storage_map();
  ASSERT_EQ(info.size(), entries.size());
  EXPECT_EQ(3u, info.size());
  for (size_t i = 0; i < info.size(); i++) {
    const std::string& id = info[i]->id;
    ASSERT_TRUE(ContainsKey(entries, id));
    EXPECT_EQ(entries[id].mnt_path, id);
    EXPECT_EQ(entries[id].type, ToString(info[i]->type));
    EXPECT_EQ(entries[id].capacity, info[i]->capacity);
    EXPECT_EQ(entries[id].available_capacity, info[i]->available_capacity);
  }
}

TEST_F(StorageInfoProviderLinuxTest, QueryInfoFailed) {
  storage_info_provider_ =
      new StorageInfoProviderLinuxWrapper(FilePath("/invalid/file/path"));
  StorageInfo info;
  ASSERT_FALSE(QueryInfo(&info));
  EXPECT_EQ(0u, info.size());
}

}  // namespace extensions

