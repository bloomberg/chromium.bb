// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_black_list.h"

#include <map>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/previews/core/previews_black_list_item.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_opt_out_store.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace previews {

namespace {

void RunLoadCallback(
    LoadBlackListCallback callback,
    std::unique_ptr<BlackListItemMap> black_list_item_map,
    std::unique_ptr<PreviewsBlackListItem> host_indifferent_black_list_item) {
  callback.Run(std::move(black_list_item_map),
               std::move(host_indifferent_black_list_item));
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
    std::unique_ptr<PreviewsBlackListItem> host_indifferent_black_list_item =
        PreviewsBlackList::CreateHostIndifferentBlackListItem();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&RunLoadCallback, callback,
                              base::Passed(&black_list_item_map),
                              base::Passed(&host_indifferent_black_list_item)));
  }

  void ClearBlackList(base::Time begin_time, base::Time end_time) override {
    ++clear_blacklist_count_;
  }

  int clear_blacklist_count_;
};

class PreviewsBlackListTest : public testing::Test {
 public:
  PreviewsBlackListTest() : field_trial_list_(nullptr) {}
  ~PreviewsBlackListTest() override {}

  void TearDown() override { variations::testing::ClearAllVariationParams(); }

  void StartTest(bool null_opt_out) {
    if (params_.size() > 0) {
      ASSERT_TRUE(variations::AssociateVariationParams("ClientSidePreviews",
                                                       "Enabled", params_));
      ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("ClientSidePreviews",
                                                         "Enabled"));
      params_.clear();
    }
    std::unique_ptr<base::SimpleTestClock> test_clock =
        base::MakeUnique<base::SimpleTestClock>();
    test_clock_ = test_clock.get();
    std::unique_ptr<TestPreviewsOptOutStore> opt_out_store =
        null_opt_out ? nullptr : base::MakeUnique<TestPreviewsOptOutStore>();
    opt_out_store_ = opt_out_store.get();
    black_list_ = base::MakeUnique<PreviewsBlackList>(std::move(opt_out_store),
                                                      std::move(test_clock));
    start_ = test_clock_->Now();
  }

  void SetHostHistoryParam(size_t host_history) {
    params_["per_host_max_stored_history_length"] =
        base::SizeTToString(host_history);
  }

  void SetHostIndifferentHistoryParam(size_t host_indifferent_history) {
    params_["host_indifferent_max_stored_history_length"] =
        base::SizeTToString(host_indifferent_history);
  }

  void SetHostThresholdParam(int per_host_threshold) {
    params_["per_host_opt_out_threshold"] =
        base::IntToString(per_host_threshold);
  }

  void SetHostIndifferentThresholdParam(int host_indifferent_threshold) {
    params_["host_indifferent_opt_out_threshold"] =
        base::IntToString(host_indifferent_threshold);
  }

  void SetHostDurationParam(int duration_in_days) {
    params_["per_host_black_list_duration_in_days"] =
        base::IntToString(duration_in_days);
  }

  void SetSingleOptOutDurationParam(int single_opt_out_duration) {
    params_["single_opt_out_duration_in_seconds"] =
        base::IntToString(single_opt_out_duration);
  }

  void SetMaxHostInBlackListParam(size_t max_hosts_in_blacklist) {
    params_["max_hosts_in_blacklist"] =
        base::IntToString(max_hosts_in_blacklist);
  }

  // Adds an opt out and either clears the black list for a time either longer
  // or shorter than the single opt out duration parameter depending on
  // |short_time|.
  void RunClearingBlackListTest(const GURL& url, bool short_time) {
    const size_t host_indifferent_history = 1;
    const int single_opt_out_duration = 5;
    SetHostDurationParam(365);
    SetHostIndifferentHistoryParam(host_indifferent_history);
    SetHostIndifferentThresholdParam(host_indifferent_history + 1);
    SetSingleOptOutDurationParam(single_opt_out_duration);

    StartTest(false /* null_opt_out */);
    if (!short_time)
      test_clock_->Advance(
          base::TimeDelta::FromSeconds(single_opt_out_duration));

    black_list_->AddPreviewNavigation(url, true /* opt_out */,
                                      PreviewsType::OFFLINE);
    test_clock_->Advance(base::TimeDelta::FromSeconds(1));
    black_list_->ClearBlackList(start_, test_clock_->Now());
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::MessageLoop loop_;

  // Unowned raw pointers tied to the lifetime of |black_list_|.
  base::SimpleTestClock* test_clock_;
  TestPreviewsOptOutStore* opt_out_store_;
  base::Time start_;
  std::map<std::string, std::string> params_;
  base::FieldTrialList field_trial_list_;

  std::unique_ptr<PreviewsBlackList> black_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PreviewsBlackListTest);
};

TEST_F(PreviewsBlackListTest, PerHostBlackListNoStore) {
  // Tests the black list behavior when a null OptOutStore is passed in.
  const GURL url_a("http://www.url_a.com");
  const GURL url_b("http://www.url_b.com");

  // Host indifferent blacklisting should have no effect with the following
  // params.
  const size_t host_indifferent_history = 1;
  SetHostHistoryParam(4);
  SetHostIndifferentHistoryParam(host_indifferent_history);
  SetHostThresholdParam(2);
  SetHostIndifferentThresholdParam(host_indifferent_history + 1);
  SetHostDurationParam(365);
  // Disable single opt out by setting duration to 0.
  SetSingleOptOutDurationParam(0);

  StartTest(true /* null_opt_out */);

  test_clock_->Advance(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(url_a, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));

  black_list_->AddPreviewNavigation(url_a, true, PreviewsType::OFFLINE);
  test_clock_->Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddPreviewNavigation(url_a, true, PreviewsType::OFFLINE);
  test_clock_->Advance(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(PreviewsEligibilityReason::HOST_BLACKLISTED,
            black_list_->IsLoadedAndAllowed(url_a, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));

  black_list_->AddPreviewNavigation(url_b, true, PreviewsType::OFFLINE);
  test_clock_->Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddPreviewNavigation(url_b, true, PreviewsType::OFFLINE);
  test_clock_->Advance(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(PreviewsEligibilityReason::HOST_BLACKLISTED,
            black_list_->IsLoadedAndAllowed(url_a, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::HOST_BLACKLISTED,
            black_list_->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));

  black_list_->AddPreviewNavigation(url_b, false, PreviewsType::OFFLINE);
  test_clock_->Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddPreviewNavigation(url_b, false, PreviewsType::OFFLINE);
  test_clock_->Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddPreviewNavigation(url_b, false, PreviewsType::OFFLINE);
  test_clock_->Advance(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(PreviewsEligibilityReason::HOST_BLACKLISTED,
            black_list_->IsLoadedAndAllowed(url_a, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));

  black_list_->ClearBlackList(start_, test_clock_->Now());

  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(url_a, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));
}

TEST_F(PreviewsBlackListTest, PerHostBlackListWithStore) {
  // Tests the black list behavior when a non-null OptOutStore is passed in.
  const GURL url_a1("http://www.url_a.com/a1");
  const GURL url_a2("http://www.url_a.com/a2");
  const GURL url_b("http://www.url_b.com");

  // Host indifferent blacklisting should have no effect with the following
  // params.
  const size_t host_indifferent_history = 1;
  SetHostHistoryParam(4);
  SetHostIndifferentHistoryParam(host_indifferent_history);
  SetHostThresholdParam(2);
  SetHostIndifferentThresholdParam(host_indifferent_history + 1);
  SetHostDurationParam(365);
  // Disable single opt out by setting duration to 0.
  SetSingleOptOutDurationParam(0);

  StartTest(false /* null_opt_out */);

  test_clock_->Advance(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED,
            black_list_->IsLoadedAndAllowed(url_a1, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED,
            black_list_->IsLoadedAndAllowed(url_a2, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED,
            black_list_->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(url_a1, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(url_a2, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));

  black_list_->AddPreviewNavigation(url_a1, true, PreviewsType::OFFLINE);
  test_clock_->Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddPreviewNavigation(url_a1, true, PreviewsType::OFFLINE);
  test_clock_->Advance(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(PreviewsEligibilityReason::HOST_BLACKLISTED,
            black_list_->IsLoadedAndAllowed(url_a1, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::HOST_BLACKLISTED,
            black_list_->IsLoadedAndAllowed(url_a2, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));

  black_list_->AddPreviewNavigation(url_b, true, PreviewsType::OFFLINE);
  test_clock_->Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddPreviewNavigation(url_b, true, PreviewsType::OFFLINE);
  test_clock_->Advance(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(PreviewsEligibilityReason::HOST_BLACKLISTED,
            black_list_->IsLoadedAndAllowed(url_a1, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::HOST_BLACKLISTED,
            black_list_->IsLoadedAndAllowed(url_a2, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::HOST_BLACKLISTED,
            black_list_->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));

  black_list_->AddPreviewNavigation(url_b, false, PreviewsType::OFFLINE);
  test_clock_->Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddPreviewNavigation(url_b, false, PreviewsType::OFFLINE);
  test_clock_->Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddPreviewNavigation(url_b, false, PreviewsType::OFFLINE);
  test_clock_->Advance(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(PreviewsEligibilityReason::HOST_BLACKLISTED,
            black_list_->IsLoadedAndAllowed(url_a1, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::HOST_BLACKLISTED,
            black_list_->IsLoadedAndAllowed(url_a2, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));

  EXPECT_EQ(0, opt_out_store_->clear_blacklist_count());
  black_list_->ClearBlackList(start_, base::Time::Now());
  EXPECT_EQ(1, opt_out_store_->clear_blacklist_count());

  EXPECT_EQ(PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED,
            black_list_->IsLoadedAndAllowed(url_a1, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED,
            black_list_->IsLoadedAndAllowed(url_a2, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED,
            black_list_->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, opt_out_store_->clear_blacklist_count());

  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(url_a1, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(url_a1, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));
}

TEST_F(PreviewsBlackListTest, HostIndifferentBlackList) {
  // Tests the black list behavior when a null OptOutStore is passed in.
  const GURL urls[] = {
      GURL("http://www.url_0.com"), GURL("http://www.url_1.com"),
      GURL("http://www.url_2.com"), GURL("http://www.url_3.com"),
  };

  // Per host blacklisting should have no effect with the following params.
  const size_t per_host_history = 1;
  const size_t host_indifferent_history = 4;
  const size_t host_indifferent_threshold = host_indifferent_history;
  SetHostHistoryParam(per_host_history);
  SetHostIndifferentHistoryParam(host_indifferent_history);
  SetHostThresholdParam(per_host_history + 1);
  SetHostIndifferentThresholdParam(host_indifferent_threshold);
  SetHostDurationParam(365);
  // Disable single opt out by setting duration to 0.
  SetSingleOptOutDurationParam(0);

  StartTest(true /* null_opt_out */);
  test_clock_->Advance(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(urls[0], PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(urls[1], PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(urls[2], PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(urls[3], PreviewsType::OFFLINE));

  for (size_t i = 0; i < host_indifferent_threshold; i++) {
    black_list_->AddPreviewNavigation(urls[i], true, PreviewsType::OFFLINE);
    EXPECT_EQ(i != 3 ? PreviewsEligibilityReason::ALLOWED
                     : PreviewsEligibilityReason::USER_BLACKLISTED,
              black_list_->IsLoadedAndAllowed(urls[0], PreviewsType::OFFLINE));
    test_clock_->Advance(base::TimeDelta::FromSeconds(1));
  }

  EXPECT_EQ(PreviewsEligibilityReason::USER_BLACKLISTED,
            black_list_->IsLoadedAndAllowed(urls[0], PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::USER_BLACKLISTED,
            black_list_->IsLoadedAndAllowed(urls[1], PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::USER_BLACKLISTED,
            black_list_->IsLoadedAndAllowed(urls[2], PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::USER_BLACKLISTED,
            black_list_->IsLoadedAndAllowed(urls[3], PreviewsType::OFFLINE));

  black_list_->AddPreviewNavigation(urls[3], false, PreviewsType::OFFLINE);
  test_clock_->Advance(base::TimeDelta::FromSeconds(1));

  // New non-opt-out entry will cause these to be allowed now.
  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(urls[0], PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(urls[1], PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(urls[2], PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(urls[3], PreviewsType::OFFLINE));
}

TEST_F(PreviewsBlackListTest, QueueBehavior) {
  // Tests the black list asynchronous queue behavior. Methods called while
  // loading the opt-out store are queued and should run in the order they were
  // queued.
  const GURL url("http://www.url.com");
  const GURL url2("http://www.url2.com");

  // Host indifferent blacklisting should have no effect with the following
  // params.
  const size_t host_indifferent_history = 1;
  SetHostIndifferentHistoryParam(host_indifferent_history);
  SetHostIndifferentThresholdParam(host_indifferent_history + 1);
  SetHostDurationParam(365);
  // Disable single opt out by setting duration to 0.
  SetSingleOptOutDurationParam(0);

  std::vector<bool> test_opt_out{true, false};

  for (auto opt_out : test_opt_out) {
    StartTest(false /* null_opt_out */);

    EXPECT_EQ(PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED,
              black_list_->IsLoadedAndAllowed(url, PreviewsType::OFFLINE));
    black_list_->AddPreviewNavigation(url, opt_out, PreviewsType::OFFLINE);
    test_clock_->Advance(base::TimeDelta::FromSeconds(1));
    black_list_->AddPreviewNavigation(url, opt_out, PreviewsType::OFFLINE);
    test_clock_->Advance(base::TimeDelta::FromSeconds(1));
    EXPECT_EQ(PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED,
              black_list_->IsLoadedAndAllowed(url, PreviewsType::OFFLINE));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(opt_out ? PreviewsEligibilityReason::HOST_BLACKLISTED
                      : PreviewsEligibilityReason::ALLOWED,
              black_list_->IsLoadedAndAllowed(url, PreviewsType::OFFLINE));
    black_list_->AddPreviewNavigation(url, opt_out, PreviewsType::OFFLINE);
    test_clock_->Advance(base::TimeDelta::FromSeconds(1));
    black_list_->AddPreviewNavigation(url, opt_out, PreviewsType::OFFLINE);
    test_clock_->Advance(base::TimeDelta::FromSeconds(1));
    EXPECT_EQ(0, opt_out_store_->clear_blacklist_count());
    black_list_->ClearBlackList(
        start_, test_clock_->Now() + base::TimeDelta::FromSeconds(1));
    EXPECT_EQ(1, opt_out_store_->clear_blacklist_count());
    black_list_->AddPreviewNavigation(url2, opt_out, PreviewsType::OFFLINE);
    test_clock_->Advance(base::TimeDelta::FromSeconds(1));
    black_list_->AddPreviewNavigation(url2, opt_out, PreviewsType::OFFLINE);
    test_clock_->Advance(base::TimeDelta::FromSeconds(1));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(1, opt_out_store_->clear_blacklist_count());

    EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
              black_list_->IsLoadedAndAllowed(url, PreviewsType::OFFLINE));
    EXPECT_EQ(opt_out ? PreviewsEligibilityReason::HOST_BLACKLISTED
                      : PreviewsEligibilityReason::ALLOWED,
              black_list_->IsLoadedAndAllowed(url2, PreviewsType::OFFLINE));
  }
}

TEST_F(PreviewsBlackListTest, MaxHosts) {
  // Test that the black list only stores n hosts, and it stores the correct n
  // hosts.
  const GURL url_a("http://www.url_a.com");
  const GURL url_b("http://www.url_b.com");
  const GURL url_c("http://www.url_c.com");
  const GURL url_d("http://www.url_d.com");
  const GURL url_e("http://www.url_e.com");

  // Host indifferent blacklisting should have no effect with the following
  // params.
  const size_t host_indifferent_history = 1;
  const size_t stored_history_length = 1;
  SetHostHistoryParam(stored_history_length);
  SetHostIndifferentHistoryParam(host_indifferent_history);
  SetHostIndifferentThresholdParam(host_indifferent_history + 1);
  SetMaxHostInBlackListParam(2);
  SetHostThresholdParam(stored_history_length);
  SetHostDurationParam(365);
  // Disable single opt out by setting duration to 0.
  SetSingleOptOutDurationParam(0);

  StartTest(true /* null_opt_out */);

  black_list_->AddPreviewNavigation(url_a, true, PreviewsType::OFFLINE);
  test_clock_->Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddPreviewNavigation(url_b, false, PreviewsType::OFFLINE);
  test_clock_->Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddPreviewNavigation(url_c, false, PreviewsType::OFFLINE);
  // url_a should stay in the map, since it has an opt out time.
  EXPECT_EQ(PreviewsEligibilityReason::HOST_BLACKLISTED,
            black_list_->IsLoadedAndAllowed(url_a, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(url_c, PreviewsType::OFFLINE));

  test_clock_->Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddPreviewNavigation(url_d, true, PreviewsType::OFFLINE);
  test_clock_->Advance(base::TimeDelta::FromSeconds(1));
  black_list_->AddPreviewNavigation(url_e, true, PreviewsType::OFFLINE);
  // url_d and url_e should remain in the map, but url_a should be evicted.
  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(url_a, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::HOST_BLACKLISTED,
            black_list_->IsLoadedAndAllowed(url_d, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::HOST_BLACKLISTED,
            black_list_->IsLoadedAndAllowed(url_e, PreviewsType::OFFLINE));
}

TEST_F(PreviewsBlackListTest, SingleOptOut) {
  // Test that when a user opts out of a preview, previews won't be shown until
  // |single_opt_out_duration| has elapsed.
  const GURL url_a("http://www.url_a.com");
  const GURL url_b("http://www.url_b.com");
  const GURL url_c("http://www.url_c.com");

  // Host indifferent blacklisting should have no effect with the following
  // params.
  const size_t host_indifferent_history = 1;
  const int single_opt_out_duration = 5;
  SetHostHistoryParam(1);
  SetHostIndifferentHistoryParam(2);
  SetHostDurationParam(365);
  SetMaxHostInBlackListParam(10);
  SetHostIndifferentHistoryParam(host_indifferent_history);
  SetHostIndifferentThresholdParam(host_indifferent_history + 1);
  SetSingleOptOutDurationParam(single_opt_out_duration);

  StartTest(true /* null_opt_out */);

  black_list_->AddPreviewNavigation(url_a, false, PreviewsType::OFFLINE);
  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(url_a, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(url_c, PreviewsType::OFFLINE));

  test_clock_->Advance(
      base::TimeDelta::FromSeconds(single_opt_out_duration + 1));

  black_list_->AddPreviewNavigation(url_b, true, PreviewsType::OFFLINE);
  EXPECT_EQ(PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT,
            black_list_->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT,
            black_list_->IsLoadedAndAllowed(url_c, PreviewsType::OFFLINE));

  test_clock_->Advance(
      base::TimeDelta::FromSeconds(single_opt_out_duration - 1));

  EXPECT_EQ(PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT,
            black_list_->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT,
            black_list_->IsLoadedAndAllowed(url_c, PreviewsType::OFFLINE));

  test_clock_->Advance(
      base::TimeDelta::FromSeconds(single_opt_out_duration + 1));

  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(url_b, PreviewsType::OFFLINE));
  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(url_c, PreviewsType::OFFLINE));
}

TEST_F(PreviewsBlackListTest, AddPreviewUMA) {
  base::HistogramTester histogram_tester;
  const GURL url("http://www.url.com");

  StartTest(false /* null_opt_out */);

  black_list_->AddPreviewNavigation(url, false, PreviewsType::OFFLINE);
  histogram_tester.ExpectUniqueSample("Previews.OptOut.UserOptedOut.Offline", 0,
                                      1);
  black_list_->AddPreviewNavigation(url, true, PreviewsType::OFFLINE);
  histogram_tester.ExpectBucketCount("Previews.OptOut.UserOptedOut.Offline", 1,
                                     1);
}

TEST_F(PreviewsBlackListTest, ClearShortTime) {
  // Tests that clearing the black list for a short amount of time (relative to
  // "SetSingleOptOutDurationParam") does not reset the blacklist's recent
  // opt out rule.

  const GURL url("http://www.url.com");
  RunClearingBlackListTest(url, true /* short_time */);
  EXPECT_EQ(PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT,
            black_list_->IsLoadedAndAllowed(url, PreviewsType::OFFLINE));
}

TEST_F(PreviewsBlackListTest, ClearingBlackListClearsRecentNavigation) {
  // Tests that clearing the black list for a long amount of time (relative to
  // "single_opt_out_duration_in_seconds") resets the blacklist's recent opt out
  // rule.

  const GURL url("http://www.url.com");
  RunClearingBlackListTest(url, false /* short_time */);

  EXPECT_EQ(PreviewsEligibilityReason::ALLOWED,
            black_list_->IsLoadedAndAllowed(url, PreviewsType::OFFLINE));
}

}  // namespace

}  // namespace previews
