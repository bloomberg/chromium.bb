// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// StorageInfoProvider unit tests.

#include "base/message_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/api/system_info_storage/storage_info_provider.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using api::experimental_system_info_storage::FromStorageUnitTypeString;
using api::experimental_system_info_storage::StorageUnitInfo;
using api::experimental_system_info_storage::StorageUnitType;
using content::BrowserThread;
using content::RunAllPendingInMessageLoop;
using content::RunMessageLoop;
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

const struct TestUnitInfo kTestingData[] = {
  {"C:", "unknown", 1000, 10, 0},
  {"d:", "removable", 2000, 10, 1 },
  {"/home","harddisk", 3000, 10, 2},
  {"/", "removable", 4000, 10, 3}
};

// The watching interval for unit test is 1 milliseconds.
const size_t kWatchingIntervalMs = 1u;
// The number of times of checking watched storages.
const int kCheckTimes = 10;

class MockStorageObserver : public StorageInfoProvider::Observer {
 public:
  MockStorageObserver() {}
  virtual ~MockStorageObserver() {}

  MOCK_METHOD3(OnStorageFreeSpaceChanged, void(const std::string&,
     double, double));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockStorageObserver);
};

// A testing observer used to provide the statistics of how many times
// that the storage free space has been changed and check the change against
// our expectation.
class TestStorageObserver : public StorageInfoProvider::Observer {
 public:
  TestStorageObserver() {
    for (size_t i = 0; i < arraysize(kTestingData); ++i)
      testing_data_.push_back(kTestingData[i]);
  }

  virtual ~TestStorageObserver() {}

  virtual void OnStorageFreeSpaceChanged(const std::string& id,
                                         double old_value,
                                         double new_value) OVERRIDE {
    // The observer is added on UI thread, so the callback should be also
    // called on UI thread.
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    for (size_t i = 0; i < testing_data_.size(); ++i) {
      if (testing_data_[i].id == id) {
        EXPECT_EQ(old_value, testing_data_[i].available_capacity);
        EXPECT_EQ(new_value, testing_data_[i].available_capacity +
                  testing_data_[i].change_step);
        // Increase the available capacity with the change step for comparison
        // next time.
        testing_data_[i].available_capacity += testing_data_[i].change_step;
        ++free_space_change_times_[id];
        return;
      }
    }
    EXPECT_TRUE(false);
  }

  // Returns the number of change times for the given storage |id|.
  int GetFreeSpaceChangeTimes(const std::string& id) {
    if (ContainsKey(free_space_change_times_, id))
      return free_space_change_times_[id];
    return 0;
  }

 private:
  // A copy of |kTestingData|.
  std::vector<TestUnitInfo> testing_data_;
  // Mapping of storage id and the times of free space has been changed.
  std::map<std::string, int> free_space_change_times_;
};

class TestStorageInfoProvider : public StorageInfoProvider {
 public:
  TestStorageInfoProvider();

 private:
  virtual ~TestStorageInfoProvider();

  virtual bool QueryInfo(StorageInfo* info) OVERRIDE;
  virtual bool QueryUnitInfo(const std::string& id,
                             StorageUnitInfo* info) OVERRIDE;

  // Called each time CheckWatchedStoragesOnBlockingPool is finished.
  void OnCheckWatchedStoragesFinishedForTesting();

  // The testing data maintained on the blocking pool.
  std::vector<TestUnitInfo> testing_data_;
  // The number of times for checking watched storage.
  int checking_times_;
};

//
// TestStorageInfoProvider Impl.
//
TestStorageInfoProvider::TestStorageInfoProvider(): checking_times_(0) {
  for (size_t i = 0; i < arraysize(kTestingData); ++i)
    testing_data_.push_back(kTestingData[i]);
}

TestStorageInfoProvider::~TestStorageInfoProvider() {}

bool TestStorageInfoProvider::QueryInfo(StorageInfo* info) {
  info->clear();

  for (size_t i = 0; i < testing_data_.size(); ++i) {
    linked_ptr<StorageUnitInfo> unit(new StorageUnitInfo());
    QueryUnitInfo(testing_data_[i].id, unit.get());
    info->push_back(unit);
  }
  return true;
}

bool TestStorageInfoProvider::QueryUnitInfo(
    const std::string& id, StorageUnitInfo* info) {
  for (size_t i = 0; i < testing_data_.size(); ++i) {
    if (testing_data_[i].id == id) {
      info->id = testing_data_[i].id;
      info->type = FromStorageUnitTypeString(testing_data_[i].type);
      info->capacity = testing_data_[i].capacity;
      info->available_capacity = testing_data_[i].available_capacity;
      // Increase the available capacity with a fixed change step.
      testing_data_[i].available_capacity += testing_data_[i].change_step;
      return true;
    }
  }
  return false;
}

void TestStorageInfoProvider::OnCheckWatchedStoragesFinishedForTesting() {
  ++checking_times_;
  if (checking_times_ < kCheckTimes)
    return;
  checking_times_ = 0;
  // Once the number of checking times reaches up to kCheckTimes, we need to
  // quit the message loop to given UI thread a chance to verify the results.
  // Note the QuitClosure is actually bound to QuitCurrentWhenIdle, it means
  // that the UI thread wil continue to process pending messages util idle.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      MessageLoop::QuitClosure());
}

class StorageInfoProviderTest : public testing::Test {
 public:
  StorageInfoProviderTest();
  virtual ~StorageInfoProviderTest();

 protected:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // Run message loop and flush blocking pool to make sure there is no pending
  // tasks on blocking pool.
  static void RunLoopAndFlushBlockingPool();
  static void RunAllPendingAndFlushBlockingPool();

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_refptr<TestStorageInfoProvider> storage_info_provider_;
};

StorageInfoProviderTest::StorageInfoProviderTest()
    : message_loop_(MessageLoop::TYPE_UI),
      ui_thread_(BrowserThread::UI, &message_loop_) {
}

StorageInfoProviderTest::~StorageInfoProviderTest() {
}

void StorageInfoProviderTest::SetUp() {
  ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
  storage_info_provider_= new TestStorageInfoProvider();
  storage_info_provider_->set_watching_interval(kWatchingIntervalMs);
}

void StorageInfoProviderTest::TearDown() {
  RunAllPendingAndFlushBlockingPool();
}

void StorageInfoProviderTest::RunLoopAndFlushBlockingPool() {
  RunMessageLoop();
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
}

void StorageInfoProviderTest::RunAllPendingAndFlushBlockingPool() {
  RunAllPendingInMessageLoop();
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
}

TEST_F(StorageInfoProviderTest, WatchingNoChangedStorage) {
  // Case 1: watching a storage that the free space is not changed.
  MockStorageObserver observer;
  storage_info_provider_->AddObserver(&observer);
  storage_info_provider_->StartWatching(kTestingData[0].id);
  EXPECT_CALL(observer, OnStorageFreeSpaceChanged(kTestingData[0].id,  _,  _))
      .Times(0);
  RunLoopAndFlushBlockingPool();

  storage_info_provider_->RemoveObserver(&observer);
  storage_info_provider_->StopWatching(kTestingData[0].id);
  RunAllPendingAndFlushBlockingPool();
}

TEST_F(StorageInfoProviderTest, WatchingOneStorage) {
  // Case 2: only watching one storage.
  TestStorageObserver observer;
  storage_info_provider_->AddObserver(&observer);
  storage_info_provider_->StartWatching(kTestingData[1].id);
  RunLoopAndFlushBlockingPool();

  // The times of free space change is at least |kCheckTimes|, it is not easy
  // to anticiapte the accurate number since it is still possible for blocking
  // pool to run the pending checking task after completing |kCheckTimes|
  // checking tasks and posting Quit to the message loop of UI thread. The
  // observer guarantees that the free space is changed with the expected
  // delta value.
  EXPECT_GE(observer.GetFreeSpaceChangeTimes(kTestingData[1].id),
            kCheckTimes);
  // Other storages should not be changed. The first two entries are skipped.
  for (size_t i = 2; i < arraysize(kTestingData); ++i) {
    EXPECT_EQ(0, observer.GetFreeSpaceChangeTimes(kTestingData[i].id));
  }

  storage_info_provider_->StopWatching(kTestingData[1].id);
  // Give a chance to run StopWatching task on the blocking pool.
  RunAllPendingAndFlushBlockingPool();

  MockStorageObserver mock_observer;
  storage_info_provider_->AddObserver(&mock_observer);
  // The watched storage won't get free space change notification.
  EXPECT_CALL(mock_observer,
      OnStorageFreeSpaceChanged(kTestingData[1].id, _, _)).Times(0);
  RunAllPendingAndFlushBlockingPool();

  storage_info_provider_->RemoveObserver(&observer);
  storage_info_provider_->RemoveObserver(&mock_observer);
}

TEST_F(StorageInfoProviderTest, WatchingMultipleStorages) {
  // Case 2: watching multiple storages. We ignore the first entry in
  // |kTestingData| since its change_step is zero.
  TestStorageObserver observer;
  storage_info_provider_->AddObserver(&observer);

  for (size_t k = 1; k < arraysize(kTestingData); ++k) {
    storage_info_provider_->StartWatching(kTestingData[k].id);
  }
  RunLoopAndFlushBlockingPool();

  // Right now let's verify the results.
  for (size_t k = 1; k < arraysize(kTestingData); ++k) {
    // See the above comments about the reason why the times of free space
    // changes is at least kCheckTimes.
    EXPECT_GE(observer.GetFreeSpaceChangeTimes(kTestingData[k].id),
              kCheckTimes);
  }

  // Stop watching the first storage.
  storage_info_provider_->StopWatching(kTestingData[1].id);
  RunAllPendingAndFlushBlockingPool();

  MockStorageObserver mock_observer;
  storage_info_provider_->AddObserver(&mock_observer);
  for (size_t k = 2; k < arraysize(kTestingData); ++k) {
    EXPECT_CALL(mock_observer,
        OnStorageFreeSpaceChanged(kTestingData[k].id,  _, _))
        .WillRepeatedly(Return());
  }

  // After stopping watching, the observer won't get change notification.
  EXPECT_CALL(mock_observer,
      OnStorageFreeSpaceChanged(kTestingData[1].id, _, _)).Times(0);
  RunLoopAndFlushBlockingPool();

  for (size_t k = 1; k < arraysize(kTestingData); ++k) {
    storage_info_provider_->StopWatching(kTestingData[k].id);
  }
  RunAllPendingAndFlushBlockingPool();
  storage_info_provider_->RemoveObserver(&observer);
  storage_info_provider_->RemoveObserver(&mock_observer);
}

TEST_F(StorageInfoProviderTest, WatchingInvalidStorage) {
  // Case 3: watching an invalid storage.
  std::string invalid_id("invalid_id");
  MockStorageObserver mock_observer;
  storage_info_provider_->AddObserver(&mock_observer);
  storage_info_provider_->StartWatching(invalid_id);
  EXPECT_CALL(mock_observer,
      OnStorageFreeSpaceChanged(invalid_id, _, _)).Times(0);
  RunAllPendingAndFlushBlockingPool();
  storage_info_provider_->RemoveObserver(&mock_observer);
}

} // namespace extensions
