// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// StorageInfoProvider unit tests.

#include "base/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/api/system_info_storage/storage_info_provider.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using api::experimental_system_info_storage::FromStorageUnitTypeString;
using api::experimental_system_info_storage::StorageUnitInfo;
using api::experimental_system_info_storage::StorageUnitType;
using content::BrowserThread;
using testing::AnyNumber;
using testing::Return;
using testing::_;

struct TestUnitInfo {
  std::string id;
  std::string type;
  double capacity;
  double available_capacity;
  // The change step of free space.
  int change_step;
};

struct TestUnitInfo testing_data[] = {
  {"C:", "unknown", 1000, 10, 0},
  {"d:", "removable", 2000, 10, 1 },
  {"/home","harddisk", 3000, 10, 2},
  {"/", "removable", 4000, 10, 3}
};

// The watching interval for unit test is 1 milliseconds.
const unsigned int kWatchingIntervalMs = 1u;
const int kCallTimes = 3;

class MockStorageObserver : public StorageInfoProvider::Observer {
 public:
  MockStorageObserver() {}
  virtual ~MockStorageObserver() {}

  MOCK_METHOD3(OnStorageFreeSpaceChanged, void(const std::string&,
     double, double));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockStorageObserver);
};

class TestStorageInfoProvider : public StorageInfoProvider {
 public:
  TestStorageInfoProvider() {}

 private:
  virtual ~TestStorageInfoProvider() {}

  virtual bool QueryInfo(StorageInfo* info) OVERRIDE {
    info->clear();

    for (size_t i = 0; i < arraysize(testing_data); i++) {
      linked_ptr<StorageUnitInfo> unit(new StorageUnitInfo());
      QueryUnitInfo(testing_data[i].id, unit.get());
      info->push_back(unit);
    }
    return true;
  }

  virtual bool QueryUnitInfo(const std::string& id,
                             StorageUnitInfo* info) OVERRIDE {
    for (size_t i = 0; i < arraysize(testing_data); i++) {
      if (testing_data[i].id == id) {
        info->id = testing_data[i].id;
        info->type = FromStorageUnitTypeString(testing_data[i].type);
        info->capacity = testing_data[i].capacity;
        info->available_capacity = testing_data[i].available_capacity;
        // Increase the available capacity with a fix change step.
        testing_data[i].available_capacity += testing_data[i].change_step;
        return true;
      }
    }
    return false;
  }
};

class StorageInfoProviderTest : public testing::Test {
 public:
  StorageInfoProviderTest();
  virtual ~StorageInfoProviderTest();

  MockStorageObserver& observer() { return observer_; }

 protected:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // Run message loop until the given |ms| milliseconds has passed.
  void RunMessageLoopUntilTimeout(int ms);

  void RunMessageLoopUntilIdle();

  // Reset the testing data once a round of querying operation is completed.
  void ResetTestingData();

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  MockStorageObserver observer_;
};

StorageInfoProviderTest::StorageInfoProviderTest()
    : message_loop_(MessageLoop::TYPE_UI),
      ui_thread_(BrowserThread::UI, &message_loop_),
      file_thread_(BrowserThread::FILE, &message_loop_) {
}

StorageInfoProviderTest::~StorageInfoProviderTest() {
}

void StorageInfoProviderTest::SetUp() {
  ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TestStorageInfoProvider* provider = new TestStorageInfoProvider();
  provider->set_watching_interval(kWatchingIntervalMs);
  // Now the provider is owned by the singleton instance.
  StorageInfoProvider::InitializeForTesting(provider);
  StorageInfoProvider::Get()->AddObserver(&observer_);
}

void StorageInfoProviderTest::TearDown() {
  StorageInfoProvider::Get()->RemoveObserver(&observer_);
}

void StorageInfoProviderTest::RunMessageLoopUntilTimeout(int ms) {
  base::RunLoop run_loop;
  message_loop_.PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(ms));
  run_loop.Run();
  ResetTestingData();
}

void StorageInfoProviderTest::RunMessageLoopUntilIdle() {
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  ResetTestingData();
}

void StorageInfoProviderTest::ResetTestingData() {
  for (size_t i = 0; i < arraysize(testing_data); ++i)
    testing_data[i].available_capacity = 10;
}

TEST_F(StorageInfoProviderTest, WatchingNoChangedStorage) {
  // Case 1: watching a storage that the free space is not changed.
  StorageInfoProvider::Get()->StartWatching(testing_data[0].id);
  EXPECT_CALL(observer(), OnStorageFreeSpaceChanged(testing_data[0].id,  _,  _))
      .Times(0);
  RunMessageLoopUntilTimeout(10 * kWatchingIntervalMs);
  StorageInfoProvider::Get()->StopWatching(testing_data[0].id);
  RunMessageLoopUntilIdle();
}

TEST_F(StorageInfoProviderTest, WatchingOneStorage) {
  // Case 2: only watching one storage.
  StorageInfoProvider::Get()->StartWatching(testing_data[1].id);
  RunMessageLoopUntilIdle();

  EXPECT_CALL(observer(), OnStorageFreeSpaceChanged(testing_data[1].id,  _,  _))
      .WillRepeatedly(Return());
  double base_value = testing_data[1].available_capacity;
  int step = testing_data[1].change_step;
  for (int i = 0; i < kCallTimes; i++) {
    double expected_old_value = base_value + i * step;
    double expected_new_value = base_value + (i + 1) * step;
    EXPECT_CALL(observer(), OnStorageFreeSpaceChanged(testing_data[1].id,
        expected_old_value, expected_new_value)).WillOnce(Return());
  }
  // The other storages won't get free space change notification.
  for (size_t i = 2; i < arraysize(testing_data); ++i) {
    EXPECT_CALL(observer(), OnStorageFreeSpaceChanged(testing_data[i].id, _, _))
        .Times(0);
  }
  RunMessageLoopUntilTimeout(100 * kWatchingIntervalMs);

  StorageInfoProvider::Get()->StopWatching(testing_data[1].id);
  RunMessageLoopUntilIdle();
  // The watched storage won't get free space change notification after
  // stopping.
  EXPECT_CALL(observer(), OnStorageFreeSpaceChanged(testing_data[1].id, _, _))
      .Times(0);
  RunMessageLoopUntilIdle();
}

TEST_F(StorageInfoProviderTest, DISABLED_WatchingMultipleStorages) {
  // Case 2: watching multiple storages. We ignore the first entry in
  // |testing_data| since its change_step is zero.
  for (size_t k = 1; k < arraysize(testing_data); ++k) {
    StorageInfoProvider::Get()->StartWatching(testing_data[k].id);
  }
  // Run the message loop to given a chance for storage info provider to start
  // watching.
  RunMessageLoopUntilIdle();

  for (size_t k = 1; k < arraysize(testing_data); ++k) {
    EXPECT_CALL(observer(),
        OnStorageFreeSpaceChanged(testing_data[k].id,  _, _))
        .WillRepeatedly(Return());

    double base_value = testing_data[k].available_capacity;
    int step = testing_data[k].change_step;
    for (int i = 0; i < kCallTimes; i++) {
      double expected_old_value = base_value + i * step;
      double expected_new_value = base_value + (i + 1) * step;
      EXPECT_CALL(observer(), OnStorageFreeSpaceChanged(testing_data[k].id,
          expected_old_value, expected_new_value)).WillOnce(Return());
    }
  }
  RunMessageLoopUntilTimeout(100 * kWatchingIntervalMs);

  // Stop watching the first storage.
  StorageInfoProvider::Get()->StopWatching(testing_data[1].id);
  RunMessageLoopUntilIdle();

  for (size_t k = 2; k < arraysize(testing_data); ++k) {
    EXPECT_CALL(observer(),
        OnStorageFreeSpaceChanged(testing_data[k].id,  _, _))
        .WillRepeatedly(Return());

    double base_value = testing_data[k].available_capacity;
    int step = testing_data[k].change_step;
    for (int i = 0; i < kCallTimes; i++) {
      double expected_old_value = base_value + i * step;
      double expected_new_value = base_value + (i + 1) * step;
      EXPECT_CALL(observer(), OnStorageFreeSpaceChanged(testing_data[k].id,
          expected_old_value, expected_new_value)).WillOnce(Return());
    }
  }
  // After stopping watching, the callback won't get called.
  EXPECT_CALL(observer(), OnStorageFreeSpaceChanged(testing_data[1].id, _, _))
      .Times(0);
  RunMessageLoopUntilTimeout(100 * kWatchingIntervalMs);

  // Stop watching all storages.
  for (size_t k = 1; k < arraysize(testing_data); ++k) {
    StorageInfoProvider::Get()->StopWatching(testing_data[k].id);
  }
  // Run the message loop to given a chance for storage info provider to stop
  // watching.
  RunMessageLoopUntilIdle();

  // After stopping watching, the callback won't get called.
  for (size_t k = 1; k < arraysize(testing_data); ++k) {
    EXPECT_CALL(observer(), OnStorageFreeSpaceChanged(testing_data[k].id, _, _))
        .Times(0);
  }
  RunMessageLoopUntilIdle();
}

TEST_F(StorageInfoProviderTest, WatchingInvalidStorage) {
  // Case 3: watching an invalid storage.
  std::string invalid_id("invalid_id");
  StorageInfoProvider::Get()->StartWatching(invalid_id);
  EXPECT_CALL(observer(), OnStorageFreeSpaceChanged(invalid_id, _, _))
     .Times(0);
  RunMessageLoopUntilTimeout(10 * kWatchingIntervalMs);
  StorageInfoProvider::Get()->StopWatching(invalid_id);
  RunMessageLoopUntilIdle();
}

} // namespace extensions
