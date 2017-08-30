// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_model.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/browser/ui/search/search_model_observer.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"

namespace {

class MockSearchModelObserver : public SearchModelObserver {
 public:
  MockSearchModelObserver();
  ~MockSearchModelObserver() override;

  void ModelChanged(SearchModel::Origin old_origin,
                    SearchModel::Origin new_origin) override;

  void VerifySearchModelStates(SearchModel::Origin expected_old_origin,
                               SearchModel::Origin expected_new_origin);

  void VerifyNotificationCount(int expected_count);

 private:
  // How many times we've seen search model changed notifications.
  int modelchanged_notification_count_;

  SearchModel::Origin actual_old_origin_;
  SearchModel::Origin actual_new_origin_;

  DISALLOW_COPY_AND_ASSIGN(MockSearchModelObserver);
};

MockSearchModelObserver::MockSearchModelObserver()
    : modelchanged_notification_count_(0) {
}

MockSearchModelObserver::~MockSearchModelObserver() {
}

void MockSearchModelObserver::ModelChanged(SearchModel::Origin old_origin,
                                           SearchModel::Origin new_origin) {
  actual_old_origin_ = old_origin;
  actual_new_origin_ = new_origin;
  modelchanged_notification_count_++;
}

void MockSearchModelObserver::VerifySearchModelStates(
    SearchModel::Origin expected_old_origin,
    SearchModel::Origin expected_new_origin) {
  EXPECT_TRUE(actual_old_origin_ == expected_old_origin);
  EXPECT_TRUE(actual_new_origin_ == expected_new_origin);
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

TEST_F(SearchModelTest, UpdateSearchModelOrigin) {
  mock_observer.VerifyNotificationCount(0);
  SearchModel::Origin origin = SearchModel::Origin::NTP;
  SearchModel::Origin expected_old_origin = model->origin();
  SearchModel::Origin expected_new_origin = origin;
  model->SetOrigin(origin);
  mock_observer.VerifySearchModelStates(expected_old_origin,
                                        expected_new_origin);
  mock_observer.VerifyNotificationCount(1);

  origin = SearchModel::Origin::DEFAULT;
  expected_old_origin = expected_new_origin;
  expected_new_origin = origin;
  model->SetOrigin(origin);
  mock_observer.VerifySearchModelStates(expected_old_origin,
                                        expected_new_origin);
  mock_observer.VerifyNotificationCount(2);
  EXPECT_EQ(expected_new_origin, model->origin());
}
