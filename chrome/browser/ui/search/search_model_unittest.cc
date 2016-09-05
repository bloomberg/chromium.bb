// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_model.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/browser/ui/search/search_model_observer.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/search/search_types.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"

namespace {

class MockSearchModelObserver : public SearchModelObserver {
 public:
  MockSearchModelObserver();
  ~MockSearchModelObserver() override;

  void ModelChanged(const SearchModel::State& old_state,
                    const SearchModel::State& new_state) override;

  void VerifySearchModelStates(const SearchModel::State& expected_old_state,
                               const SearchModel::State& expected_new_state);

  void VerifyNotificationCount(int expected_count);

 private:
  // How many times we've seen search model changed notifications.
  int modelchanged_notification_count_;

  SearchModel::State actual_old_state_;
  SearchModel::State actual_new_state_;

  DISALLOW_COPY_AND_ASSIGN(MockSearchModelObserver);
};

MockSearchModelObserver::MockSearchModelObserver()
    : modelchanged_notification_count_(0) {
}

MockSearchModelObserver::~MockSearchModelObserver() {
}

void MockSearchModelObserver::ModelChanged(
    const SearchModel::State& old_state,
    const SearchModel::State& new_state) {
  actual_old_state_ = old_state;
  actual_new_state_ = new_state;
  modelchanged_notification_count_++;
}

void MockSearchModelObserver::VerifySearchModelStates(
    const SearchModel::State& expected_old_state,
    const SearchModel::State& expected_new_state) {
  EXPECT_TRUE(actual_old_state_ == expected_old_state);
  EXPECT_TRUE(actual_new_state_ == expected_new_state);
}

void MockSearchModelObserver::VerifyNotificationCount(int expected_count) {
  EXPECT_EQ(modelchanged_notification_count_, expected_count);
}

}  // namespace

class SearchModelTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override;
  void TearDown() override;

  MockSearchModelObserver mock_observer;
  SearchModel* model;
};

void SearchModelTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  SearchTabHelper::CreateForWebContents(web_contents());
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_TRUE(search_tab_helper != NULL);
  model = search_tab_helper->model();
  model->AddObserver(&mock_observer);
}

void SearchModelTest::TearDown() {
  model->RemoveObserver(&mock_observer);
  ChromeRenderViewHostTestHarness::TearDown();
}

TEST_F(SearchModelTest, UpdateSearchModelInstantSupport) {
  mock_observer.VerifyNotificationCount(0);
  EXPECT_TRUE(model->instant_support() == INSTANT_SUPPORT_UNKNOWN);
  SearchModel::State expected_old_state = model->state();
  SearchModel::State expected_new_state(model->state());
  expected_new_state.instant_support = INSTANT_SUPPORT_YES;

  model->SetInstantSupportState(INSTANT_SUPPORT_YES);
  mock_observer.VerifySearchModelStates(expected_old_state, expected_new_state);
  mock_observer.VerifyNotificationCount(1);
  EXPECT_TRUE(model->instant_support() == INSTANT_SUPPORT_YES);

  expected_old_state = expected_new_state;
  expected_new_state.instant_support = INSTANT_SUPPORT_NO;
  model->SetInstantSupportState(INSTANT_SUPPORT_NO);
  mock_observer.VerifySearchModelStates(expected_old_state, expected_new_state);
  mock_observer.VerifyNotificationCount(2);

  // Notify the observer only if the search model state is changed.
  model->SetInstantSupportState(INSTANT_SUPPORT_NO);
  EXPECT_TRUE(model->state() == expected_new_state);
  EXPECT_TRUE(model->instant_support() == INSTANT_SUPPORT_NO);
  mock_observer.VerifyNotificationCount(2);
}

TEST_F(SearchModelTest, UpdateSearchModelMode) {
  mock_observer.VerifyNotificationCount(0);
  SearchMode search_mode(SearchMode::MODE_NTP, SearchMode::ORIGIN_NTP);
  SearchModel::State expected_old_state = model->state();
  SearchModel::State expected_new_state(model->state());
  expected_new_state.mode = search_mode;

  model->SetMode(search_mode);
  mock_observer.VerifySearchModelStates(expected_old_state, expected_new_state);
  mock_observer.VerifyNotificationCount(1);

  search_mode.mode = SearchMode::MODE_SEARCH_SUGGESTIONS;
  expected_old_state = expected_new_state;
  expected_new_state.mode = search_mode;
  model->SetMode(search_mode);
  mock_observer.VerifySearchModelStates(expected_old_state, expected_new_state);
  mock_observer.VerifyNotificationCount(2);
  EXPECT_TRUE(model->state() == expected_new_state);
}

TEST_F(SearchModelTest, UpdateSearchModelState) {
  SearchModel::State expected_new_state(model->state());
  expected_new_state.instant_support = INSTANT_SUPPORT_NO;
  EXPECT_FALSE(model->state() == expected_new_state);
  model->SetState(expected_new_state);
  mock_observer.VerifyNotificationCount(1);
  EXPECT_TRUE(model->state() == expected_new_state);
}
