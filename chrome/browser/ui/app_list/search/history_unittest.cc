// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/sequenced_worker_pool_owner.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/ui/app_list/search/history_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/search/dictionary_data_store.h"
#include "ui/app_list/search/history.h"
#include "ui/app_list/search/history_data.h"
#include "ui/app_list/search/history_data_observer.h"
#include "ui/app_list/search/history_data_store.h"

namespace app_list {
namespace test {

namespace {

const size_t kMaxPrimary = 3;
const size_t kMaxSecondary = 2;

// HistoryDataLoadWaiter waits for give |data| to be loaded from underlying
// store on the blocking pool. The waiter waits on the main message loop until
// OnHistoryDataLoadedFromStore() is invoked or the maximum allowed wait time
// has passed.
class HistoryDataLoadWaiter : public HistoryDataObserver {
 public:
  explicit HistoryDataLoadWaiter(HistoryData* data) : data_(data)  {}
  ~HistoryDataLoadWaiter() override {}

  void Wait() {
    data_->AddObserver(this);

    run_loop_.reset(new base::RunLoop);
    run_loop_->Run();

    data_->RemoveObserver(this);
  }

 private:
  // HistoryDataObserver overrides:
  void OnHistoryDataLoadedFromStore() override { run_loop_->Quit(); }

  HistoryData* data_;  // Not owned.
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(HistoryDataLoadWaiter);
};

// StoreFlushWaiter waits for the given |store| to flush its data to disk.
// The flush and disk write happens on the blocking pool. The waiter waits
// on the main message loop until the OnFlushed() is invoked or the maximum
// allowed wait time has passed.
class StoreFlushWaiter {
 public:
  explicit StoreFlushWaiter(HistoryDataStore* store) : store_(store) {}
  ~StoreFlushWaiter() {}

  void Wait() {
    store_->Flush(
        base::Bind(&StoreFlushWaiter::OnFlushed, base::Unretained(this)));

    run_loop_.reset(new base::RunLoop);
    run_loop_->Run();
  }

 private:
  void OnFlushed() {
    run_loop_->Quit();
  }

  HistoryDataStore* store_;  // Not owned.
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(StoreFlushWaiter);
};

}  // namespace

class SearchHistoryTest : public testing::Test {
 public:
  SearchHistoryTest() {}
  ~SearchHistoryTest() override {}

  // testing::Test overrides:
  void SetUp() override {
    worker_pool_owner_.reset(
        new base::SequencedWorkerPoolOwner(2, "AppLauncherTest"));
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    CreateHistory();
  }
  void TearDown() override {
    Flush();
  }

  void CreateHistory() {
    const char kStoreDataFileName[] = "app-launcher-test";
    const base::FilePath data_file =
        temp_dir_.GetPath().AppendASCII(kStoreDataFileName);
    scoped_refptr<DictionaryDataStore> dictionary_data_store(
        new DictionaryDataStore(data_file, worker_pool_owner_->pool().get()));
    history_.reset(new History(scoped_refptr<HistoryDataStore>(
        new HistoryDataStore(dictionary_data_store))));

    // Replace |data_| with test params.
    history_->data_->RemoveObserver(history_.get());
    history_->data_.reset(
        new HistoryData(history_->store_.get(), kMaxPrimary, kMaxSecondary));
    history_->data_->AddObserver(history_.get());

    HistoryDataLoadWaiter(history_->data_.get()).Wait();
    ASSERT_TRUE(history_->IsReady());
  }

  void Flush() {
    StoreFlushWaiter(history_->store_.get()).Wait();
  }

  size_t GetKnownResults(const std::string& query) {
    known_results_ = history()->GetKnownResults(query);
    return known_results_->size();
  }

  KnownResultType GetResultType(const std::string& result_id) {
    return known_results_->find(result_id) != known_results_->end()
               ? (*known_results_.get())[result_id]
               : UNKNOWN_RESULT;
  }

  History* history() { return history_.get(); }
  const HistoryData::Associations& associations() const {
    return history_->data_->associations();
  }

 private:
  base::MessageLoopForUI message_loop_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::SequencedWorkerPoolOwner> worker_pool_owner_;

  std::unique_ptr<History> history_;
  std::unique_ptr<KnownResults> known_results_;

  DISALLOW_COPY_AND_ASSIGN(SearchHistoryTest);
};

TEST_F(SearchHistoryTest, Persistence) {
  // Ensure it's empty.
  EXPECT_EQ(0u, GetKnownResults("cal"));

  // Add one launch event.
  history()->AddLaunchEvent("cal", "calendar");
  EXPECT_EQ(1u, GetKnownResults("cal"));

  // Flush and recreate the history object.
  Flush();
  CreateHistory();

  // History should be initialized with data just added.
  EXPECT_EQ(1u, GetKnownResults("cal"));
}

TEST_F(SearchHistoryTest, PerfectAndPrefixMatch) {
  const char kQuery[] = "cal";
  const char kQueryPrefix[] = "c";
  const char kPrimary[] = "calendar";
  const char kSecondary[] = "calculator";

  history()->AddLaunchEvent(kQuery, kPrimary);
  history()->AddLaunchEvent(kQuery, kSecondary);

  EXPECT_EQ(2u, GetKnownResults(kQuery));
  EXPECT_EQ(PERFECT_PRIMARY, GetResultType(kPrimary));
  EXPECT_EQ(PERFECT_SECONDARY, GetResultType(kSecondary));

  EXPECT_EQ(2u, GetKnownResults(kQueryPrefix));
  EXPECT_EQ(PREFIX_PRIMARY, GetResultType(kPrimary));
  EXPECT_EQ(PREFIX_SECONDARY, GetResultType(kSecondary));
}

TEST_F(SearchHistoryTest, StickyPrimary) {
  const char kQuery[] = "cal";
  const char kPrimary[] = "calendar";
  const char kSecondary[] = "calculator";
  const char kOther[] = "other";

  // Add two launch events. kPrimary becomes primary.
  history()->AddLaunchEvent(kQuery, kPrimary);
  history()->AddLaunchEvent(kQuery, kSecondary);

  EXPECT_EQ(2u, GetKnownResults(kQuery));
  EXPECT_EQ(PERFECT_PRIMARY, GetResultType(kPrimary));
  EXPECT_EQ(PERFECT_SECONDARY, GetResultType(kSecondary));

  // These launch events should not change primary.
  history()->AddLaunchEvent(kQuery, kPrimary);
  history()->AddLaunchEvent(kQuery, kSecondary);
  history()->AddLaunchEvent(kQuery, kPrimary);
  history()->AddLaunchEvent(kQuery, kSecondary);
  history()->AddLaunchEvent(kQuery, kPrimary);
  history()->AddLaunchEvent(kQuery, kSecondary);
  history()->AddLaunchEvent(kQuery, kOther);
  history()->AddLaunchEvent(kQuery, kSecondary);
  history()->AddLaunchEvent(kQuery, kOther);
  history()->AddLaunchEvent(kQuery, kSecondary);
  history()->AddLaunchEvent(kQuery, kOther);

  EXPECT_EQ(3u, GetKnownResults(kQuery));
  EXPECT_EQ(PERFECT_PRIMARY, GetResultType(kPrimary));
  EXPECT_EQ(PERFECT_SECONDARY, GetResultType(kSecondary));
  EXPECT_EQ(PERFECT_SECONDARY, GetResultType(kOther));
}

TEST_F(SearchHistoryTest, PromoteSecondary) {
  const char kQuery[] = "cal";
  const char kPrimary[] = "calendar";
  const char kSecondary[] = "calculator";

  history()->AddLaunchEvent(kQuery, kPrimary);
  history()->AddLaunchEvent(kQuery, kSecondary);

  EXPECT_EQ(2u, GetKnownResults(kQuery));
  EXPECT_EQ(PERFECT_PRIMARY, GetResultType(kPrimary));
  EXPECT_EQ(PERFECT_SECONDARY, GetResultType(kSecondary));

  // The 2nd launch in a row promotes it to be primary.
  history()->AddLaunchEvent(kQuery, kSecondary);

  EXPECT_EQ(2u, GetKnownResults(kQuery));
  EXPECT_EQ(PERFECT_PRIMARY, GetResultType(kSecondary));
  EXPECT_EQ(PERFECT_SECONDARY, GetResultType(kPrimary));
}

TEST_F(SearchHistoryTest, MaxPrimary) {
  for (size_t i = 0; i < kMaxPrimary; ++i) {
    std::string query = base::SizeTToString(i);
    history()->AddLaunchEvent(query, "app");
  }
  EXPECT_EQ(kMaxPrimary, associations().size());

  // Oldest entries still exists.
  EXPECT_TRUE(associations().find("0") != associations().end());
  EXPECT_TRUE(associations().find("1") != associations().end());

  // Primary entry trimming is based on time. The code could run here too fast
  // and Time::Now has not changed on some platform. Sleep a bit here to ensure
  // that some time has passed to get rid of the flake.
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(25));

  // Touches the oldest and 2nd oldest becomes oldest now.
  history()->AddLaunchEvent("0", "app");

  // Adds one more
  history()->AddLaunchEvent("extra", "app");

  // Number of entries are capped to kMaxPrimary.
  EXPECT_EQ(kMaxPrimary, associations().size());

  // Oldest entry is trimmed.
  EXPECT_FALSE(associations().find("1") != associations().end());

  // The touched oldest survived.
  EXPECT_TRUE(associations().find("0") != associations().end());
}

TEST_F(SearchHistoryTest, MaxSecondary) {
  const char kQuery[] = "query";
  history()->AddLaunchEvent(kQuery, "primary");
  for (size_t i = 0; i < kMaxSecondary; ++i) {
    std::string result_id = base::SizeTToString(i);
    history()->AddLaunchEvent(kQuery, result_id);
  }

  EXPECT_EQ(kMaxSecondary + 1, GetKnownResults(kQuery));
  EXPECT_EQ(PERFECT_SECONDARY, GetResultType("0"));
  EXPECT_EQ(PERFECT_SECONDARY, GetResultType("1"));

  // Touches the oldest secondary.
  history()->AddLaunchEvent(kQuery, "0");

  // Adds one more.
  history()->AddLaunchEvent(kQuery, "extra");

  // Total number of results is capped.
  EXPECT_EQ(kMaxSecondary + 1, GetKnownResults(kQuery));

  // The oldest secondary is gone.
  EXPECT_EQ(UNKNOWN_RESULT, GetResultType("1"));

  // Touched oldest survived.
  EXPECT_EQ(PERFECT_SECONDARY, GetResultType("0"));
}

}  // namespace test
}  // namespace app_list
