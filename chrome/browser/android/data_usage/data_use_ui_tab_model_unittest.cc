// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/data_use_ui_tab_model.h"

#include <stdint.h>

#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "chrome/browser/android/data_usage/data_use_tab_model.h"
#include "chrome/browser/android/data_usage/data_use_ui_tab_model_factory.h"
#include "chrome/browser/android/data_usage/external_data_use_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "components/data_usage/core/data_use.h"
#include "components/data_usage/core/data_use_aggregator.h"
#include "components/data_usage/core/data_use_amortizer.h"
#include "components/data_usage/core/data_use_annotator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace chrome {

namespace android {

namespace {

class TestDataUseTabModel : public DataUseTabModel {
 public:
  TestDataUseTabModel(
      ExternalDataUseObserver* external_data_use_observer,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
      : DataUseTabModel(external_data_use_observer, ui_task_runner) {}

  ~TestDataUseTabModel() override {}

  using DataUseTabModel::NotifyObserversOfTrackingStarting;
  using DataUseTabModel::NotifyObserversOfTrackingEnding;
};

class DataUseUITabModelTest : public testing::Test {
 public:
  DataUseUITabModel* data_use_ui_tab_model() {
    return data_use_ui_tab_model_.get();
  }

  ExternalDataUseObserver* external_data_use_observer() const {
    return external_data_use_observer_.get();
  }

  TestDataUseTabModel* data_use_tab_model() const {
    return data_use_tab_model_.get();
  }

  void FetchMatchingRulesDone(const std::vector<std::string>& app_package_name,
                              const std::vector<std::string>& domain_path_regex,
                              const std::vector<std::string>& label) {
    external_data_use_observer_->FetchMatchingRulesDone(
        &app_package_name, &domain_path_regex, &label);
  }

 protected:
  void SetUp() override {
    thread_bundle_.reset(new content::TestBrowserThreadBundle(
        content::TestBrowserThreadBundle::IO_MAINLOOP));
    io_task_runner_ = content::BrowserThread::GetMessageLoopProxyForThread(
        content::BrowserThread::IO);
    data_use_ui_tab_model_.reset(
        new chrome::android::DataUseUITabModel(io_task_runner_));

    ui_task_runner_ = content::BrowserThread::GetMessageLoopProxyForThread(
        content::BrowserThread::UI);

    data_use_aggregator_.reset(
        new data_usage::DataUseAggregator(nullptr, nullptr));

    external_data_use_observer_.reset(new ExternalDataUseObserver(
        data_use_aggregator_.get(), io_task_runner_, ui_task_runner_));
    // Wait for |external_data_use_observer_| to create the Java object.
    base::RunLoop().RunUntilIdle();

    data_use_tab_model_.reset(
        new TestDataUseTabModel(external_data_use_observer(), ui_task_runner_));
    data_use_tab_model_->AddObserver(data_use_ui_tab_model()->GetWeakPtr());
    data_use_ui_tab_model_->SetDataUseTabModel(
        data_use_tab_model_->GetWeakPtr());
  }

 private:
  scoped_ptr<content::TestBrowserThreadBundle> thread_bundle_;
  scoped_ptr<DataUseUITabModel> data_use_ui_tab_model_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_ptr<data_usage::DataUseAggregator> data_use_aggregator_;
  scoped_ptr<ExternalDataUseObserver> external_data_use_observer_;
  scoped_ptr<TestDataUseTabModel> data_use_tab_model_;
};

}  // namespace

// Tests that DataUseTabModel is notified of tab closure and navigation events,
// and DataUseTabModel notifies DataUseUITabModel.
TEST_F(DataUseUITabModelTest, ReportTabEventsTest) {
  const char kFooLabel[] = "foo_label";
  const char kFooPackage[] = "com.foo";

  std::vector<std::string> url_regexes;
  url_regexes.push_back(
      "http://www[.]foo[.]com/#q=.*|https://www[.]foo[.]com/#q=.*");
  FetchMatchingRulesDone(
      std::vector<std::string>(url_regexes.size(), kFooPackage), url_regexes,
      std::vector<std::string>(url_regexes.size(), kFooLabel));

  const struct {
    ui::PageTransition transition_type;
    std::string expected_label;
  } tests[] = {
      {ui::PageTransitionFromInt(ui::PageTransition::PAGE_TRANSITION_LINK |
                                 ui::PAGE_TRANSITION_FROM_API),
       std::string()},
      {ui::PageTransition::PAGE_TRANSITION_LINK, std::string()},
      {ui::PageTransition::PAGE_TRANSITION_TYPED, kFooLabel},
      {ui::PageTransition::PAGE_TRANSITION_AUTO_BOOKMARK, std::string()},
      {ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string()},
      {ui::PageTransition::PAGE_TRANSITION_GENERATED, kFooLabel},
      {ui::PageTransition::PAGE_TRANSITION_RELOAD, std::string()},
  };

  SessionID::id_type foo_tab_id = 100;

  for (size_t i = 0; i < arraysize(tests); ++i) {
    // Start a new tab.
    ++foo_tab_id;
    data_use_ui_tab_model()->ReportBrowserNavigation(
        GURL("https://www.foo.com/#q=abc"), tests[i].transition_type,
        foo_tab_id);
    // Wait for DataUseUITabModel to notify DataUseTabModel, which should notify
    // DataUseUITabModel back.
    base::RunLoop().RunUntilIdle();

    // |data_use_ui_tab_model| should receive callback about starting of
    // tracking of data usage for |foo_tab_id|.
    EXPECT_EQ(!tests[i].expected_label.empty(),
              data_use_ui_tab_model()->HasDataUseTrackingStarted(foo_tab_id))
        << i;
    EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingStarted(foo_tab_id))
        << i;
    EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingEnded(foo_tab_id))
        << i;

    // DataUse object should be labeled correctly.
    data_usage::DataUse data_use(GURL("http://foo.com/#q=abc"),
                                 base::TimeTicks::Now(),
                                 GURL("http://foobar.com"), foo_tab_id,
                                 net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
                                 std::string(), 1000, 1000);
    std::string got_label;
    data_use_tab_model()->GetLabelForDataUse(data_use, &got_label);
    EXPECT_EQ(tests[i].expected_label, got_label) << i;

    // Report closure of tab.
    data_use_ui_tab_model()->ReportTabClosure(foo_tab_id);
    base::RunLoop().RunUntilIdle();

    // DataUse object should not be labeled.
    EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingEnded(foo_tab_id));
    data_use.request_start =
        base::TimeTicks::Now() + base::TimeDelta::FromMinutes(10);
    data_use_tab_model()->GetLabelForDataUse(data_use, &got_label);
    EXPECT_EQ(std::string(), got_label) << i;
  }

  const SessionID::id_type bar_tab_id = foo_tab_id + 1;
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingStarted(bar_tab_id));
  data_use_ui_tab_model()->ReportCustomTabInitialNavigation(
      bar_tab_id, std::string(), kFooPackage);
  base::RunLoop().RunUntilIdle();

  // |data_use_ui_tab_model| should receive callback about starting of
  // tracking of data usage for |bar_tab_id|.
  EXPECT_TRUE(data_use_ui_tab_model()->HasDataUseTrackingStarted(bar_tab_id));
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingStarted(bar_tab_id));
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingEnded(bar_tab_id));

  data_use_ui_tab_model()->ReportTabClosure(bar_tab_id);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingEnded(bar_tab_id));
}

// Tests if the Entrance/Exit UI state is tracked correctly.
TEST_F(DataUseUITabModelTest, EntranceExitState) {
  const SessionID::id_type kFooTabId = 1;
  const SessionID::id_type kBarTabId = 2;
  const SessionID::id_type kBazTabId = 3;

  // HasDataUseTrackingStarted should return true only once.
  data_use_tab_model()->NotifyObserversOfTrackingStarting(kFooTabId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_use_ui_tab_model()->HasDataUseTrackingStarted(kFooTabId));
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingStarted(kFooTabId));
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingEnded(kFooTabId));

  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingStarted(kBarTabId));
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingEnded(kBarTabId));

  // HasDataUseTrackingEnded should return true only once.
  data_use_tab_model()->NotifyObserversOfTrackingEnding(kFooTabId);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingStarted(kFooTabId));
  EXPECT_TRUE(data_use_ui_tab_model()->HasDataUseTrackingEnded(kFooTabId));
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingEnded(kFooTabId));
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingStarted(kFooTabId));

  // The tab enters the tracking state again.
  data_use_tab_model()->NotifyObserversOfTrackingStarting(kFooTabId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_use_ui_tab_model()->HasDataUseTrackingStarted(kFooTabId));
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingStarted(kFooTabId));

  // The tab exits the tracking state.
  data_use_tab_model()->NotifyObserversOfTrackingEnding(kFooTabId);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingStarted(kFooTabId));

  // The tab enters the tracking state again.
  data_use_tab_model()->NotifyObserversOfTrackingStarting(kFooTabId);
  base::RunLoop().RunUntilIdle();
  data_use_tab_model()->NotifyObserversOfTrackingStarting(kFooTabId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_use_ui_tab_model()->HasDataUseTrackingStarted(kFooTabId));
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingStarted(kFooTabId));

  // ShowExit should return true only once.
  data_use_tab_model()->NotifyObserversOfTrackingEnding(kBarTabId);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingStarted(kBarTabId));
  EXPECT_TRUE(data_use_ui_tab_model()->HasDataUseTrackingEnded(kBarTabId));
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingEnded(kBarTabId));
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingStarted(kBarTabId));

  data_use_ui_tab_model()->ReportTabClosure(kFooTabId);
  data_use_ui_tab_model()->ReportTabClosure(kBarTabId);

  //  HasDataUseTrackingStarted/Ended should return false for closed tabs.
  data_use_tab_model()->NotifyObserversOfTrackingStarting(kBazTabId);
  base::RunLoop().RunUntilIdle();
  data_use_ui_tab_model()->ReportTabClosure(kBazTabId);
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingStarted(kBazTabId));
  EXPECT_FALSE(data_use_ui_tab_model()->HasDataUseTrackingEnded(kBazTabId));
}

// Checks if page transition type is converted correctly.
TEST_F(DataUseUITabModelTest, ConvertTransitionType) {
  DataUseTabModel::TransitionType transition_type;
  EXPECT_TRUE(data_use_ui_tab_model()->ConvertTransitionType(
      ui::PageTransition(ui::PAGE_TRANSITION_TYPED), &transition_type));
  EXPECT_EQ(DataUseTabModel::TRANSITION_OMNIBOX_NAVIGATION, transition_type);
  EXPECT_TRUE(data_use_ui_tab_model()->ConvertTransitionType(
      ui::PageTransition(ui::PAGE_TRANSITION_TYPED | 0xFF00),
      &transition_type));
  EXPECT_EQ(DataUseTabModel::TRANSITION_OMNIBOX_NAVIGATION, transition_type);
  EXPECT_TRUE(data_use_ui_tab_model()->ConvertTransitionType(
      ui::PageTransition(ui::PAGE_TRANSITION_TYPED | 0xFFFF00),
      &transition_type));
  EXPECT_EQ(DataUseTabModel::TRANSITION_OMNIBOX_NAVIGATION, transition_type);
  EXPECT_TRUE(data_use_ui_tab_model()->ConvertTransitionType(
      ui::PageTransition(ui::PAGE_TRANSITION_TYPED | 0x12FFFF00),
      &transition_type));
  EXPECT_EQ(DataUseTabModel::TRANSITION_OMNIBOX_NAVIGATION, transition_type);

  EXPECT_TRUE(data_use_ui_tab_model()->ConvertTransitionType(
      ui::PageTransition(ui::PAGE_TRANSITION_AUTO_BOOKMARK), &transition_type));
  EXPECT_EQ(DataUseTabModel::TRANSITION_BOOKMARK, transition_type);
  EXPECT_TRUE(data_use_ui_tab_model()->ConvertTransitionType(
      ui::PageTransition(ui::PAGE_TRANSITION_AUTO_BOOKMARK | 0xFF00),
      &transition_type));
  EXPECT_EQ(DataUseTabModel::TRANSITION_BOOKMARK, transition_type);
  EXPECT_TRUE(data_use_ui_tab_model()->ConvertTransitionType(
      ui::PageTransition(ui::PAGE_TRANSITION_AUTO_BOOKMARK | 0xFFFF00),
      &transition_type));
  EXPECT_EQ(DataUseTabModel::TRANSITION_BOOKMARK, transition_type);
  EXPECT_TRUE(data_use_ui_tab_model()->ConvertTransitionType(
      ui::PageTransition(ui::PAGE_TRANSITION_AUTO_BOOKMARK | 0x12FFFF00),
      &transition_type));
  EXPECT_EQ(DataUseTabModel::TRANSITION_BOOKMARK, transition_type);

  EXPECT_TRUE(data_use_ui_tab_model()->ConvertTransitionType(
      ui::PageTransition(ui::PAGE_TRANSITION_GENERATED), &transition_type));
  EXPECT_EQ(DataUseTabModel::TRANSITION_OMNIBOX_SEARCH, transition_type);
  EXPECT_TRUE(data_use_ui_tab_model()->ConvertTransitionType(
      ui::PageTransition(ui::PAGE_TRANSITION_GENERATED | 0xFF00),
      &transition_type));
  EXPECT_EQ(DataUseTabModel::TRANSITION_OMNIBOX_SEARCH, transition_type);
  EXPECT_TRUE(data_use_ui_tab_model()->ConvertTransitionType(
      ui::PageTransition(ui::PAGE_TRANSITION_GENERATED | 0xFFFF00),
      &transition_type));
  EXPECT_EQ(DataUseTabModel::TRANSITION_OMNIBOX_SEARCH, transition_type);
  EXPECT_TRUE(data_use_ui_tab_model()->ConvertTransitionType(
      ui::PageTransition(ui::PAGE_TRANSITION_GENERATED | 0x12FFFF00),
      &transition_type));
  EXPECT_EQ(DataUseTabModel::TRANSITION_OMNIBOX_SEARCH, transition_type);

  EXPECT_FALSE(data_use_ui_tab_model()->ConvertTransitionType(
      ui::PageTransition(ui::PAGE_TRANSITION_AUTO_SUBFRAME), &transition_type));
  EXPECT_FALSE(data_use_ui_tab_model()->ConvertTransitionType(
      ui::PageTransition(ui::PAGE_TRANSITION_MANUAL_SUBFRAME),
      &transition_type));
  EXPECT_FALSE(data_use_ui_tab_model()->ConvertTransitionType(
      ui::PageTransition(ui::PAGE_TRANSITION_FORM_SUBMIT), &transition_type));
}

}  // namespace android

}  // namespace chrome
