// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_black_list.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/previews/core/previews_black_list_item.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_opt_out_store.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using PreviewsBlackListTest = testing::Test;

}  // namespace

namespace previews {

namespace {

void RunLoadCallback(LoadBlackListCallback callback,
                     std::unique_ptr<BlackListItemMap> black_list_item_map) {
  callback.Run(std::move(black_list_item_map));
}

class TestPreviewsOptOutStore : public PreviewsOptOutStore {
 public:
  TestPreviewsOptOutStore() : clear_blacklist_count_(0) {}
  ~TestPreviewsOptOutStore() override {}

  int clear_blacklist_count() { return clear_blacklist_count_; }

 private:
  // PreviewsOptOutStore implementation:
  void AddPreviewNavigation(bool opt_out,
                            const std::string& host_name,
                            PreviewsType type,
                            base::Time now) override {}

  void LoadBlackList(LoadBlackListCallback callback) override {
    std::unique_ptr<BlackListItemMap> black_list_item_map(
        new BlackListItemMap());
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&RunLoadCallback, callback,
                              base::Passed(&black_list_item_map)));
  }

  void ClearBlackList(base::Time begin_time, base::Time end_time) override {
    ++clear_blacklist_count_;
  }

  int clear_blacklist_count_;
};

}  // namespace

TEST_F(PreviewsBlackListTest, BlackListNoStore) {
  // Tests the black list behavior when a null OptOutSture is passed in.
  const GURL url_a("http://www.url_a.com");
  const GURL url_b("http://www.url_b.com");
  const size_t history = 4;
  const int threshold = 2;
  const int duration_in_days = 365;
  // Disable single opt out by setting duration to 0.
  const int single_opt_out_duration = 0;
  base::FieldTrialList field_trial_list(nullptr);
  std::map<std::string, std::string> params;
  params["stored_history_length"] = base::SizeTToString(history);
  params["opt_out_threshold"] = base::IntToString(threshold);
  params["black_list_duration_in_days"] = base::IntToString(duration_in_days);
  params["single_opt_out_duration_in_seconds"] =
      base::IntToString(single_opt_out_duration);
  ASSERT_TRUE(
      variations::AssociateVariationParams("ClientSidePreviews", "Enabled",
                                           params) &&
      base::FieldTrialList::CreateFieldTrial("ClientSidePreviews", "Enabled"));

  base::SimpleTestClock* test_clock = new base::SimpleTestClock();
  base::Time start = test_clock->Now();
  test_clock->Advance(base::TimeDelta::FromSeconds(1));

  std::unique_ptr<PreviewsBlackList> black_list(
      new PreviewsBlackList(nullptr, base::WrapUnique(test_clock)));

  EXPECT_TRUE(black_list->IsLoadedAndAllowed(url_a, PreviewsType::OFFLINE));
  EXPECT_TRUE(black_list->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));

  black_list->AddPreviewNavigation(url_a, true, PreviewsType::OFFLINE);
  black_list->AddPreviewNavigation(url_a, true, PreviewsType::OFFLINE);

  EXPECT_FALSE(black_list->IsLoadedAndAllowed(url_a, PreviewsType::OFFLINE));
  EXPECT_TRUE(black_list->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));

  black_list->AddPreviewNavigation(url_b, true, PreviewsType::OFFLINE);
  black_list->AddPreviewNavigation(url_b, true, PreviewsType::OFFLINE);

  EXPECT_FALSE(black_list->IsLoadedAndAllowed(url_a, PreviewsType::OFFLINE));
  EXPECT_FALSE(black_list->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));

  black_list->AddPreviewNavigation(url_b, false, PreviewsType::OFFLINE);
  black_list->AddPreviewNavigation(url_b, false, PreviewsType::OFFLINE);
  black_list->AddPreviewNavigation(url_b, false, PreviewsType::OFFLINE);

  EXPECT_FALSE(black_list->IsLoadedAndAllowed(url_a, PreviewsType::OFFLINE));
  EXPECT_TRUE(black_list->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));

  test_clock->Advance(base::TimeDelta::FromSeconds(1));
  black_list->ClearBlackList(start, test_clock->Now());

  EXPECT_TRUE(black_list->IsLoadedAndAllowed(url_a, PreviewsType::OFFLINE));
  EXPECT_TRUE(black_list->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));

  variations::testing::ClearAllVariationParams();
}

TEST_F(PreviewsBlackListTest, BlackListWithStore) {
  // Tests the black list behavior when a non-null OptOutSture is passed in.
  const GURL url_a1("http://www.url_a.com/a1");
  const GURL url_a2("http://www.url_a.com/a2");
  const GURL url_b("http://www.url_b.com");
  const size_t history = 4;
  const int threshold = 2;
  const int duration_in_days = 365;
  // Disable single opt out by setting duration to 0.
  const int single_opt_out_duration = 0;
  base::FieldTrialList field_trial_list(nullptr);
  std::map<std::string, std::string> params;
  params["stored_history_length"] = base::SizeTToString(history);
  params["opt_out_threshold"] = base::IntToString(threshold);
  params["black_list_duration_in_days"] = base::IntToString(duration_in_days);
  params["single_opt_out_duration_in_seconds"] =
      base::IntToString(single_opt_out_duration);
  ASSERT_TRUE(
      variations::AssociateVariationParams("ClientSidePreviews", "Enabled",
                                           params) &&
      base::FieldTrialList::CreateFieldTrial("ClientSidePreviews", "Enabled"));

  base::MessageLoop loop;

  base::SimpleTestClock* test_clock = new base::SimpleTestClock();
  base::Time start = test_clock->Now();
  test_clock->Advance(base::TimeDelta::FromSeconds(1));

  TestPreviewsOptOutStore* opt_out_store = new TestPreviewsOptOutStore();

  std::unique_ptr<PreviewsBlackList> black_list(new PreviewsBlackList(
      base::WrapUnique(opt_out_store), base::WrapUnique(test_clock)));

  EXPECT_FALSE(black_list->IsLoadedAndAllowed(url_a1, PreviewsType::OFFLINE));
  EXPECT_FALSE(black_list->IsLoadedAndAllowed(url_a2, PreviewsType::OFFLINE));
  EXPECT_FALSE(black_list->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(black_list->IsLoadedAndAllowed(url_a1, PreviewsType::OFFLINE));
  EXPECT_TRUE(black_list->IsLoadedAndAllowed(url_a2, PreviewsType::OFFLINE));
  EXPECT_TRUE(black_list->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));

  black_list->AddPreviewNavigation(url_a1, true, PreviewsType::OFFLINE);
  black_list->AddPreviewNavigation(url_a1, true, PreviewsType::OFFLINE);

  EXPECT_FALSE(black_list->IsLoadedAndAllowed(url_a1, PreviewsType::OFFLINE));
  EXPECT_FALSE(black_list->IsLoadedAndAllowed(url_a2, PreviewsType::OFFLINE));
  EXPECT_TRUE(black_list->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));

  black_list->AddPreviewNavigation(url_b, true, PreviewsType::OFFLINE);
  black_list->AddPreviewNavigation(url_b, true, PreviewsType::OFFLINE);

  EXPECT_FALSE(black_list->IsLoadedAndAllowed(url_a1, PreviewsType::OFFLINE));
  EXPECT_FALSE(black_list->IsLoadedAndAllowed(url_a2, PreviewsType::OFFLINE));
  EXPECT_FALSE(black_list->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));

  black_list->AddPreviewNavigation(url_b, false, PreviewsType::OFFLINE);
  black_list->AddPreviewNavigation(url_b, false, PreviewsType::OFFLINE);
  black_list->AddPreviewNavigation(url_b, false, PreviewsType::OFFLINE);

  EXPECT_FALSE(black_list->IsLoadedAndAllowed(url_a1, PreviewsType::OFFLINE));
  EXPECT_FALSE(black_list->IsLoadedAndAllowed(url_a2, PreviewsType::OFFLINE));
  EXPECT_TRUE(black_list->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));

  test_clock->Advance(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(0, opt_out_store->clear_blacklist_count());
  black_list->ClearBlackList(start, base::Time::Now());
  EXPECT_EQ(1, opt_out_store->clear_blacklist_count());

  EXPECT_FALSE(black_list->IsLoadedAndAllowed(url_a1, PreviewsType::OFFLINE));
  EXPECT_FALSE(black_list->IsLoadedAndAllowed(url_a2, PreviewsType::OFFLINE));
  EXPECT_FALSE(black_list->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, opt_out_store->clear_blacklist_count());

  EXPECT_TRUE(black_list->IsLoadedAndAllowed(url_a1, PreviewsType::OFFLINE));
  EXPECT_TRUE(black_list->IsLoadedAndAllowed(url_a1, PreviewsType::OFFLINE));
  EXPECT_TRUE(black_list->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));

  variations::testing::ClearAllVariationParams();
}

TEST_F(PreviewsBlackListTest, QueueBehavior) {
  // Tests the black list asynchronous queue behavior. Methods called while
  // loading are queued and should run in the order they were queued.
  const GURL url("http://www.url.com");
  const GURL url2("http://www.url2.com");
  const int duration_in_days = 365;
  // Disable single opt out by setting duration to 0.
  const int single_opt_out_duration = 0;
  base::FieldTrialList field_trial_list(nullptr);
  std::map<std::string, std::string> params;
  params["black_list_duration_in_days"] = base::IntToString(duration_in_days);
  params["single_opt_out_duration_in_seconds"] =
      base::IntToString(single_opt_out_duration);
  ASSERT_TRUE(
      variations::AssociateVariationParams("ClientSidePreviews", "Enabled",
                                           params) &&
      base::FieldTrialList::CreateFieldTrial("ClientSidePreviews", "Enabled"));

  base::MessageLoop loop;

  std::vector<bool> test_opt_out{true, false};

  for (auto opt_out : test_opt_out) {
    base::SimpleTestClock* test_clock = new base::SimpleTestClock();

    TestPreviewsOptOutStore* opt_out_store = new TestPreviewsOptOutStore();

    std::unique_ptr<PreviewsBlackList> black_list(new PreviewsBlackList(
        base::WrapUnique(opt_out_store), base::WrapUnique(test_clock)));

    EXPECT_FALSE(black_list->IsLoadedAndAllowed(url, PreviewsType::OFFLINE));
    black_list->AddPreviewNavigation(url, opt_out, PreviewsType::OFFLINE);
    black_list->AddPreviewNavigation(url, opt_out, PreviewsType::OFFLINE);
    EXPECT_FALSE(black_list->IsLoadedAndAllowed(url, PreviewsType::OFFLINE));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(!opt_out,
              black_list->IsLoadedAndAllowed(url, PreviewsType::OFFLINE));

    base::Time start = test_clock->Now();
    test_clock->Advance(base::TimeDelta::FromSeconds(1));
    black_list->AddPreviewNavigation(url, opt_out, PreviewsType::OFFLINE);
    black_list->AddPreviewNavigation(url, opt_out, PreviewsType::OFFLINE);
    test_clock->Advance(base::TimeDelta::FromSeconds(1));
    EXPECT_EQ(0, opt_out_store->clear_blacklist_count());
    black_list->ClearBlackList(
        start, test_clock->Now() + base::TimeDelta::FromSeconds(1));
    EXPECT_EQ(1, opt_out_store->clear_blacklist_count());
    black_list->AddPreviewNavigation(url2, opt_out, PreviewsType::OFFLINE);
    black_list->AddPreviewNavigation(url2, opt_out, PreviewsType::OFFLINE);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(1, opt_out_store->clear_blacklist_count());

    EXPECT_TRUE(black_list->IsLoadedAndAllowed(url, PreviewsType::OFFLINE));
    EXPECT_EQ(!opt_out,
              black_list->IsLoadedAndAllowed(url2, PreviewsType::OFFLINE));
  }

  variations::testing::ClearAllVariationParams();
}

TEST_F(PreviewsBlackListTest, MaxHosts) {
  // Test that the black list only stores n hosts, and it stores the correct n
  // hosts.
  const GURL url_a("http://www.url_a.com");
  const GURL url_b("http://www.url_b.com");
  const GURL url_c("http://www.url_c.com");
  const GURL url_d("http://www.url_d.com");
  const GURL url_e("http://www.url_e.com");
  const size_t stored_history_length = 1;
  const int opt_out_threshold = 1;
  const int black_list_duration_in_days = 365;
  // Disable single opt out by setting duration to 0.
  const int single_opt_out_duration = 0;
  const size_t max_hosts_in_blacklist = 2;
  base::FieldTrialList field_trial_list(nullptr);
  std::map<std::string, std::string> params;
  params["stored_history_length"] = base::SizeTToString(stored_history_length);
  params["opt_out_threshold"] = base::IntToString(opt_out_threshold);
  params["black_list_duration_in_days"] =
      base::IntToString(black_list_duration_in_days);
  params["max_hosts_in_blacklist"] =
      base::SizeTToString(max_hosts_in_blacklist);
  params["single_opt_out_duration_in_seconds"] =
      base::IntToString(single_opt_out_duration);
  ASSERT_TRUE(
      variations::AssociateVariationParams("ClientSidePreviews", "Enabled",
                                           params) &&
      base::FieldTrialList::CreateFieldTrial("ClientSidePreviews", "Enabled"));

  base::MessageLoop loop;

  base::SimpleTestClock* test_clock = new base::SimpleTestClock();

  std::unique_ptr<PreviewsBlackList> black_list(
      new PreviewsBlackList(nullptr, base::WrapUnique(test_clock)));

  black_list->AddPreviewNavigation(url_a, true, PreviewsType::OFFLINE);
  test_clock->Advance(base::TimeDelta::FromSeconds(1));
  black_list->AddPreviewNavigation(url_b, false, PreviewsType::OFFLINE);
  test_clock->Advance(base::TimeDelta::FromSeconds(1));
  black_list->AddPreviewNavigation(url_c, false, PreviewsType::OFFLINE);
  // url_a should stay in the map, since it has an opt out time.
  EXPECT_FALSE(black_list->IsLoadedAndAllowed(url_a, PreviewsType::OFFLINE));
  EXPECT_TRUE(black_list->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));
  EXPECT_TRUE(black_list->IsLoadedAndAllowed(url_c, PreviewsType::OFFLINE));

  test_clock->Advance(base::TimeDelta::FromSeconds(1));
  black_list->AddPreviewNavigation(url_d, true, PreviewsType::OFFLINE);
  test_clock->Advance(base::TimeDelta::FromSeconds(1));
  black_list->AddPreviewNavigation(url_e, true, PreviewsType::OFFLINE);
  // url_d and url_e should remain in the map, but url_a should be evicted.
  EXPECT_TRUE(black_list->IsLoadedAndAllowed(url_a, PreviewsType::OFFLINE));
  EXPECT_FALSE(black_list->IsLoadedAndAllowed(url_d, PreviewsType::OFFLINE));
  EXPECT_FALSE(black_list->IsLoadedAndAllowed(url_e, PreviewsType::OFFLINE));

  variations::testing::ClearAllVariationParams();
}

TEST_F(PreviewsBlackListTest, SingleOptOut) {
  // Test that when a user opts out of a preview, previews won't be shown until
  // |single_opt_out_duration| has elapsed.
  const GURL url_a("http://www.url_a.com");
  const GURL url_b("http://www.url_b.com");
  const GURL url_c("http://www.url_c.com");
  const size_t stored_history_length = 1;
  const int opt_out_threshold = 2;
  const int black_list_duration_in_days = 365;
  const int single_opt_out_duration = 5;
  const size_t max_hosts_in_blacklist = 10;
  base::FieldTrialList field_trial_list(nullptr);
  std::map<std::string, std::string> params;
  params["stored_history_length"] = base::SizeTToString(stored_history_length);
  params["opt_out_threshold"] = base::IntToString(opt_out_threshold);
  params["black_list_duration_in_days"] =
      base::IntToString(black_list_duration_in_days);
  params["max_hosts_in_blacklist"] =
      base::SizeTToString(max_hosts_in_blacklist);
  params["single_opt_out_duration_in_seconds"] =
      base::IntToString(single_opt_out_duration);
  ASSERT_TRUE(
      variations::AssociateVariationParams("ClientSidePreviews", "Enabled",
                                           params) &&
      base::FieldTrialList::CreateFieldTrial("ClientSidePreviews", "Enabled"));

  base::SimpleTestClock* test_clock = new base::SimpleTestClock();

  std::unique_ptr<PreviewsBlackList> black_list(
      new PreviewsBlackList(nullptr, base::WrapUnique(test_clock)));

  black_list->AddPreviewNavigation(url_a, false, PreviewsType::OFFLINE);
  EXPECT_TRUE(black_list->IsLoadedAndAllowed(url_a, PreviewsType::OFFLINE));
  EXPECT_TRUE(black_list->IsLoadedAndAllowed(url_c, PreviewsType::OFFLINE));

  test_clock->Advance(
      base::TimeDelta::FromSeconds(single_opt_out_duration + 1));

  black_list->AddPreviewNavigation(url_b, true, PreviewsType::OFFLINE);
  EXPECT_FALSE(black_list->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));
  EXPECT_FALSE(black_list->IsLoadedAndAllowed(url_c, PreviewsType::OFFLINE));

  test_clock->Advance(
      base::TimeDelta::FromSeconds(single_opt_out_duration - 1));

  EXPECT_FALSE(black_list->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));
  EXPECT_FALSE(black_list->IsLoadedAndAllowed(url_c, PreviewsType::OFFLINE));

  test_clock->Advance(
      base::TimeDelta::FromSeconds(single_opt_out_duration + 1));

  EXPECT_TRUE(black_list->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));
  EXPECT_TRUE(black_list->IsLoadedAndAllowed(url_c, PreviewsType::OFFLINE));

  variations::testing::ClearAllVariationParams();
}

}  // namespace previews
