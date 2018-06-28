// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_use_measurement/page_load_capping/page_load_capping_blacklist.h"

#include <stdint.h>

#include <map>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/data_use_measurement/page_load_capping/chrome_page_load_capping_features.h"
#include "components/blacklist/opt_out_blacklist/opt_out_blacklist_delegate.h"
#include "components/blacklist/opt_out_blacklist/opt_out_store.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace previews {

namespace {

// Mock class to test that PageLoadCappingBlacklist notifies the delegate with
// correct events (e.g. New host blacklisted, user blacklisted, and blacklist
// cleared).
class TestOptOutBlacklistDelegate : public blacklist::OptOutBlacklistDelegate {
 public:
  TestOptOutBlacklistDelegate() {}

  // PageLoadCappingBlacklistDelegate:
  void OnNewBlacklistedHost(const std::string& host, base::Time time) override {
  }
  void OnUserBlacklistedStatusChange(bool blacklisted) override {}
  void OnBlacklistCleared(base::Time time) override {}
};

class TestPageLoadCappingBlacklist : public PageLoadCappingBlacklist {
 public:
  TestPageLoadCappingBlacklist(
      std::unique_ptr<blacklist::OptOutStore> opt_out_store,
      base::Clock* clock,
      blacklist::OptOutBlacklistDelegate* blacklist_delegate)
      : PageLoadCappingBlacklist(std::move(opt_out_store),
                                 clock,
                                 blacklist_delegate) {}
  ~TestPageLoadCappingBlacklist() override {}

  using PageLoadCappingBlacklist::ShouldUseSessionPolicy;
  using PageLoadCappingBlacklist::ShouldUsePersistentPolicy;
  using PageLoadCappingBlacklist::ShouldUseHostPolicy;
  using PageLoadCappingBlacklist::ShouldUseTypePolicy;
  using PageLoadCappingBlacklist::GetAllowedTypes;
};

class PageLoadCappingBlacklistTest : public testing::Test {
 public:
  PageLoadCappingBlacklistTest() {}
  ~PageLoadCappingBlacklistTest() override {}

  void TearDown() override { variations::testing::ClearAllVariationParams(); }

  void StartTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        data_use_measurement::page_load_capping::features::kDetectingHeavyPages,
        params_);

    black_list_ = std::make_unique<TestPageLoadCappingBlacklist>(
        nullptr, &test_clock_, &blacklist_delegate_);
  }

  void SetParam(const std::string& param, int value) {
    params_[param] = base::NumberToString(value);
  }

  blacklist::BlacklistData::AllowedTypesAndVersions GetAllowedTypes() const {
    return black_list_->GetAllowedTypes();
  }

 protected:
  base::SimpleTestClock test_clock_;

  std::unique_ptr<TestPageLoadCappingBlacklist> black_list_;

 private:
  // Observer to |black_list_|.
  TestOptOutBlacklistDelegate blacklist_delegate_;

  std::map<std::string, std::string> params_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(PageLoadCappingBlacklistTest);
};

TEST_F(PageLoadCappingBlacklistTest, DefaultParams) {
  StartTest();
  {
    base::TimeDelta duration;
    size_t history = 0;
    int threshold = 0;

    EXPECT_TRUE(
        black_list_->ShouldUseSessionPolicy(&duration, &history, &threshold));
    EXPECT_EQ(base::TimeDelta::FromSeconds(600), duration);
    EXPECT_EQ(3u, history);
    EXPECT_EQ(2, threshold);
  }

  {
    base::TimeDelta duration;
    size_t history = 0;
    int threshold = 0;

    EXPECT_TRUE(black_list_->ShouldUsePersistentPolicy(&duration, &history,
                                                       &threshold));
    EXPECT_EQ(base::TimeDelta::FromDays(30), duration);
    EXPECT_EQ(10u, history);
    EXPECT_EQ(9, threshold);
  }

  {
    base::TimeDelta duration;
    size_t history = 0;
    int threshold = 0;
    size_t max_hosts = 0;

    EXPECT_TRUE(black_list_->ShouldUseHostPolicy(&duration, &history,
                                                 &threshold, &max_hosts));
    EXPECT_EQ(base::TimeDelta::FromDays(30), duration);
    EXPECT_EQ(7u, history);
    EXPECT_EQ(6, threshold);
    EXPECT_EQ(50u, max_hosts);
  }

  EXPECT_FALSE(black_list_->ShouldUseTypePolicy(nullptr, nullptr, nullptr));

  blacklist::BlacklistData::AllowedTypesAndVersions types = GetAllowedTypes();
  EXPECT_EQ(1u, types.size());
  const auto iter = types.begin();
  EXPECT_EQ(0, iter->first);
  EXPECT_EQ(0, iter->second);
}

TEST_F(PageLoadCappingBlacklistTest, SessionParams) {
  int session_duration_seconds = 5;
  int session_history = 6;
  int session_threshold = 7;

  SetParam("session-duration-seconds", session_duration_seconds);
  SetParam("session-history", session_history);
  SetParam("session-threshold", session_threshold);

  StartTest();

  base::TimeDelta duration;
  size_t history = 0;
  int threshold = 0;

  EXPECT_TRUE(
      black_list_->ShouldUseSessionPolicy(&duration, &history, &threshold));
  EXPECT_EQ(base::TimeDelta::FromSeconds(session_duration_seconds), duration);
  EXPECT_EQ(session_history, static_cast<int>(history));
  EXPECT_EQ(session_threshold, threshold);
}

TEST_F(PageLoadCappingBlacklistTest, PersistentParams) {
  int persistent_duration_seconds = 5;
  int persistent_history = 6;
  int persistent_threshold = 7;

  SetParam("persistent-duration-days", persistent_duration_seconds);
  SetParam("persistent-history", persistent_history);
  SetParam("persistent-threshold", persistent_threshold);

  StartTest();

  base::TimeDelta duration;
  size_t history = 0;
  int threshold = 0;

  EXPECT_TRUE(
      black_list_->ShouldUsePersistentPolicy(&duration, &history, &threshold));
  EXPECT_EQ(base::TimeDelta::FromDays(persistent_duration_seconds), duration);
  EXPECT_EQ(persistent_history, static_cast<int>(history));
  EXPECT_EQ(persistent_threshold, threshold);
}

TEST_F(PageLoadCappingBlacklistTest, HostParams) {
  int host_max_hosts = 11;
  int host_duration_days = 5;
  int host_history = 6;
  int host_threshold = 7;

  SetParam("host-duration-days", host_duration_days);
  SetParam("host-history", host_history);
  SetParam("host-threshold", host_threshold);
  SetParam("hosts-in-memory", host_max_hosts);

  StartTest();

  base::TimeDelta duration;
  size_t history = 0;
  int threshold = 0;
  size_t max_hosts = 0;

  EXPECT_TRUE(black_list_->ShouldUseHostPolicy(&duration, &history, &threshold,
                                               &max_hosts));
  EXPECT_EQ(base::TimeDelta::FromDays(host_duration_days), duration);
  EXPECT_EQ(host_history, static_cast<int>(history));
  EXPECT_EQ(host_threshold, threshold);
  EXPECT_EQ(host_max_hosts, static_cast<int>(max_hosts));
}

TEST_F(PageLoadCappingBlacklistTest, TypeParams) {
  StartTest();
  EXPECT_FALSE(black_list_->ShouldUseTypePolicy(nullptr, nullptr, nullptr));
}

TEST_F(PageLoadCappingBlacklistTest, TypeVersionParam) {
  int version = 17;
  SetParam("type-version", version);
  StartTest();
  blacklist::BlacklistData::AllowedTypesAndVersions types = GetAllowedTypes();
  EXPECT_EQ(1u, types.size());
  const auto iter = types.begin();
  EXPECT_EQ(0, iter->first);
  EXPECT_EQ(version, iter->second);
}

}  // namespace

}  // namespace previews
