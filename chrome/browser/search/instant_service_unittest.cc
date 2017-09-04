// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_service.h"

#include <string>
#include <vector>

#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/search/instant_unittest_base.h"
#include "chrome/browser/search/search.h"
#include "chrome/common/search/instant_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

class MockInstantServiceObserver : public InstantServiceObserver {
 public:
  MOCK_METHOD1(DefaultSearchProviderChanged, void(bool));
};

class InstantServiceTest : public InstantUnitTestBase {
 protected:
  void SetUp() override {
    InstantUnitTestBase::SetUp();

    instant_service_observer_.reset(new MockInstantServiceObserver());
    instant_service_->AddObserver(instant_service_observer_.get());
  }

  void TearDown() override {
    instant_service_->RemoveObserver(instant_service_observer_.get());
    InstantUnitTestBase::TearDown();
  }

  std::unique_ptr<MockInstantServiceObserver> instant_service_observer_;
};

TEST_F(InstantServiceTest, DispatchDefaultSearchProviderChanged) {
  EXPECT_CALL(*instant_service_observer_.get(),
              DefaultSearchProviderChanged(false)).Times(1);

  const std::string new_base_url = "https://bar.com/";
  SetUserSelectedDefaultSearchProvider(new_base_url);
}

TEST_F(InstantServiceTest, DontDispatchGoogleURLUpdatedForNonGoogleURLs) {
  EXPECT_CALL(*instant_service_observer_.get(),
              DefaultSearchProviderChanged(false)).Times(1);
  const std::string new_dsp_url = "https://bar.com/";
  SetUserSelectedDefaultSearchProvider(new_dsp_url);
  testing::Mock::VerifyAndClearExpectations(instant_service_observer_.get());

  EXPECT_CALL(*instant_service_observer_.get(),
              DefaultSearchProviderChanged(false)).Times(0);
  const std::string new_base_url = "https://www.google.es/";
  NotifyGoogleBaseURLUpdate(new_base_url);
}

TEST_F(InstantServiceTest, DispatchGoogleURLUpdated) {
  EXPECT_CALL(*instant_service_observer_.get(),
              DefaultSearchProviderChanged(true)).Times(1);

  const std::string new_base_url = "https://www.google.es/";
  NotifyGoogleBaseURLUpdate(new_base_url);
}

TEST_F(InstantServiceTest, GetSuggestionFromClientSide) {
  history::MostVisitedURLList url_list;
  url_list.push_back(history::MostVisitedURL());

  instant_service_->OnTopSitesReceived(url_list);

  auto items = instant_service_->most_visited_items_;
  ASSERT_EQ(1, (int)items.size());
  ASSERT_EQ(ntp_tiles::TileSource::TOP_SITES, items[0].source);
}
