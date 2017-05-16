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

  void ModelChanged(const SearchMode& old_mode,
                    const SearchMode& new_mode) override;

  void VerifySearchModelStates(const SearchMode& expected_old_mode,
                               const SearchMode& expected_new_mode);

  void VerifyNotificationCount(int expected_count);

 private:
  // How many times we've seen search model changed notifications.
  int modelchanged_notification_count_;

  SearchMode actual_old_mode_;
  SearchMode actual_new_mode_;

  DISALLOW_COPY_AND_ASSIGN(MockSearchModelObserver);
};

MockSearchModelObserver::MockSearchModelObserver()
    : modelchanged_notification_count_(0) {
}

MockSearchModelObserver::~MockSearchModelObserver() {
}

void MockSearchModelObserver::ModelChanged(const SearchMode& old_mode,
                                           const SearchMode& new_mode) {
  actual_old_mode_ = old_mode;
  actual_new_mode_ = new_mode;
  modelchanged_notification_count_++;
}

void MockSearchModelObserver::VerifySearchModelStates(
    const SearchMode& expected_old_mode,
    const SearchMode& expected_new_mode) {
  EXPECT_TRUE(actual_old_mode_ == expected_old_mode);
  EXPECT_TRUE(actual_new_mode_ == expected_new_mode);
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

TEST_F(SearchModelTest, UpdateSearchModelMode) {
  mock_observer.VerifyNotificationCount(0);
  SearchMode search_mode(SearchMode::MODE_NTP, SearchMode::ORIGIN_NTP);
  SearchMode expected_old_mode = model->mode();
  SearchMode expected_new_mode = search_mode;
  model->SetMode(search_mode);
  mock_observer.VerifySearchModelStates(expected_old_mode, expected_new_mode);
  mock_observer.VerifyNotificationCount(1);

  search_mode.mode = SearchMode::MODE_SEARCH_SUGGESTIONS;
  expected_old_mode = expected_new_mode;
  expected_new_mode = search_mode;
  model->SetMode(search_mode);
  mock_observer.VerifySearchModelStates(expected_old_mode, expected_new_mode);
  mock_observer.VerifyNotificationCount(2);
  EXPECT_TRUE(model->mode() == expected_new_mode);
}
