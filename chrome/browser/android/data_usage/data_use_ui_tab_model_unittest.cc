// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/data_use_ui_tab_model.h"

#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "chrome/browser/android/data_usage/data_use_tab_model.h"
#include "chrome/browser/android/data_usage/data_use_ui_tab_model_factory.h"
#include "chrome/browser/android/data_usage/external_data_use_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/common/url_constants.h"
#include "components/data_usage/core/data_use_aggregator.h"
#include "components/data_usage/core/data_use_amortizer.h"
#include "components/data_usage/core/data_use_annotator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace chrome {

namespace android {

namespace {

const char kFooLabel[] = "foo_label";
const char kFooPackage[] = "com.foo";

const SessionID::id_type kTabIDFoo = 1;
const SessionID::id_type kTabIDBar = 2;
const SessionID::id_type kTabIDBaz = 3;

const char kURLFoo[] = "https://www.foo.com/#q=abc";

}  // namespace

// Mock observer to track the calls to start and end tracking events.
class MockTabDataUseObserver : public DataUseTabModel::TabDataUseObserver {
 public:
  MOCK_METHOD1(NotifyTrackingStarting, void(SessionID::id_type tab_id));
  MOCK_METHOD1(NotifyTrackingEnding, void(SessionID::id_type tab_id));
  MOCK_METHOD0(OnDataUseTabModelReady, void());
};

class TestDataUseTabModel : public DataUseTabModel {
 public:
  TestDataUseTabModel() {}
  ~TestDataUseTabModel() override {}

  using DataUseTabModel::NotifyObserversOfTrackingStarting;
  using DataUseTabModel::NotifyObserversOfTrackingEnding;
};

class DataUseUITabModelTest : public testing::Test {
 public:
  DataUseUITabModelTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}
  DataUseUITabModel* data_use_ui_tab_model() { return &data_use_ui_tab_model_; }

  TestDataUseTabModel* data_use_tab_model() const {
    return data_use_tab_model_.get();
  }

  void RegisterURLRegexes(const std::vector<std::string>& app_package_name,
                          const std::vector<std::string>& domain_path_regex,
                          const std::vector<std::string>& label) {
    data_use_tab_model_->RegisterURLRegexes(app_package_name, domain_path_regex,
                                            label);
  }

 protected:
  void SetUp() override {
    io_task_runner_ = content::BrowserThread::GetMessageLoopProxyForThread(
        content::BrowserThread::IO);
    ui_task_runner_ = content::BrowserThread::GetMessageLoopProxyForThread(
        content::BrowserThread::UI);

    data_use_aggregator_.reset(
        new data_usage::DataUseAggregator(nullptr, nullptr));

    external_data_use_observer_.reset(new ExternalDataUseObserver(
        data_use_aggregator_.get(), io_task_runner_, ui_task_runner_));
    // Wait for |external_data_use_observer_| to create the Java object.
    base::RunLoop().RunUntilIdle();

    data_use_tab_model_.reset(new TestDataUseTabModel());
    data_use_tab_model_->InitOnUIThread(
        external_data_use_observer_->external_data_use_observer_bridge_);
  }

  void SetUpDataUseUITabModel() {
    data_use_ui_tab_model_.SetDataUseTabModel(data_use_tab_model_.get());
    data_use_tab_model_->OnControlAppInstallStateChange(true);
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  DataUseUITabModel data_use_ui_tab_model_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  std::unique_ptr<data_usage::DataUseAggregator> data_use_aggregator_;
  std::unique_ptr<ExternalDataUseObserver> external_data_use_observer_;
  std::unique_ptr<TestDataUseTabModel> data_use_tab_model_;
};

// Tests that DataUseTabModel is notified of tab closure and navigation events,
// and DataUseTabModel notifies DataUseUITabModel.
TEST_F(DataUseUITabModelTest, ReportTabEventsTest) {
  SetUpDataUseUITabModel();

  std::vector<std::string> url_regexes;
  url_regexes.push_back(
      "http://www[.]foo[.]com/#q=.*|https://www[.]foo[.]com/#q=.*");
  RegisterURLRegexes(std::vector<std::string>(url_regexes.size(), kFooPackage),
                     url_regexes,
                     std::vector<std::string>(url_regexes.size(), kFooLabel));

  const struct {
    ui::PageTransition transition_type;
    std::string expected_label;
  } tests[] = {
      {ui::PageTransitionFromInt(ui::PageTransition::PAGE_TRANSITION_LINK |
                                 ui::PAGE_TRANSITION_FROM_API),
       kFooLabel},
      {ui::PageTransition::PAGE_TRANSITION_LINK, kFooLabel},
      {ui::PageTransition::PAGE_TRANSITION_TYPED, kFooLabel},
      {ui::PageTransition::PAGE_TRANSITION_AUTO_BOOKMARK, kFooLabel},
      {ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string()},
      {ui::PageTransition::PAGE_TRANSITION_GENERATED, kFooLabel},
      {ui::PageTransition::PAGE_TRANSITION_RELOAD, kFooLabel},
  };

  SessionID::id_type foo_tab_id = 100;

  for (size_t i = 0; i < arraysize(tests); ++i) {
    // Start a new tab.
    ++foo_tab_id;
    data_use_ui_tab_model()->ReportBrowserNavigation(
        GURL("https://www.foo.com/#q=abc"), tests[i].transition_type,
        foo_tab_id);

    // |data_use_ui_tab_model| should receive callback about starting of
    // tracking of data usage for |foo_tab_id|.
    EXPECT_EQ(!tests[i].expected_label.empty(),
              data_use_ui_tab_model()->CheckAndResetDataUseTrackingStarted(
                  foo_tab_id))
        << i;
    EXPECT_FALSE(data_use_ui_tab_model()->CheckAndResetDataUseTrackingStarted(
        foo_tab_id))
        << i;
    EXPECT_FALSE(
        data_use_ui_tab_model()->CheckAndResetDataUseTrackingEnded(foo_tab_id))
        << i;

    std::string got_label;
    data_use_tab_model()->GetLabelForTabAtTime(
        foo_tab_id, base::TimeTicks::Now(), &got_label);
    EXPECT_EQ(tests[i].expected_label, got_label) << i;

    // Report closure of tab.
    data_use_ui_tab_model()->ReportTabClosure(foo_tab_id);

    // DataUse object should not be labeled.
    EXPECT_FALSE(
        data_use_ui_tab_model()->CheckAndResetDataUseTrackingEnded(foo_tab_id));
    data_use_tab_model()->GetLabelForTabAtTime(
        foo_tab_id, base::TimeTicks::Now() + base::TimeDelta::FromMinutes(10),
        &got_label);
    EXPECT_EQ(std::string(), got_label) << i;
  }

  // Start a custom tab with matching package name and verify if tracking
  // started is not being set.
  const SessionID::id_type bar_tab_id = foo_tab_id + 1;
  EXPECT_FALSE(
      data_use_ui_tab_model()->CheckAndResetDataUseTrackingStarted(bar_tab_id));
  data_use_ui_tab_model()->ReportCustomTabInitialNavigation(
      bar_tab_id, kFooPackage, std::string());

  EXPECT_FALSE(
      data_use_ui_tab_model()->CheckAndResetDataUseTrackingStarted(bar_tab_id));
  EXPECT_FALSE(
      data_use_ui_tab_model()->CheckAndResetDataUseTrackingEnded(bar_tab_id));

  data_use_ui_tab_model()->ReportTabClosure(bar_tab_id);
  EXPECT_FALSE(
      data_use_ui_tab_model()->CheckAndResetDataUseTrackingEnded(bar_tab_id));
}

// Tests if the Entrance/Exit UI state is tracked correctly.
TEST_F(DataUseUITabModelTest, EntranceExitState) {
  SetUpDataUseUITabModel();

  const SessionID::id_type kFooTabId = 1;
  const SessionID::id_type kBarTabId = 2;
  const SessionID::id_type kBazTabId = 3;

  // CheckAndResetDataUseTrackingStarted should return true only once.
  data_use_tab_model()->NotifyObserversOfTrackingStarting(kFooTabId);
  EXPECT_TRUE(
      data_use_ui_tab_model()->CheckAndResetDataUseTrackingStarted(kFooTabId));
  EXPECT_FALSE(
      data_use_ui_tab_model()->CheckAndResetDataUseTrackingStarted(kFooTabId));
  EXPECT_FALSE(
      data_use_ui_tab_model()->CheckAndResetDataUseTrackingEnded(kFooTabId));

  EXPECT_FALSE(
      data_use_ui_tab_model()->CheckAndResetDataUseTrackingStarted(kBarTabId));
  EXPECT_FALSE(
      data_use_ui_tab_model()->CheckAndResetDataUseTrackingEnded(kBarTabId));

  // CheckAndResetDataUseTrackingEnded should return true only once.
  data_use_tab_model()->NotifyObserversOfTrackingEnding(kFooTabId);
  EXPECT_FALSE(
      data_use_ui_tab_model()->CheckAndResetDataUseTrackingStarted(kFooTabId));
  EXPECT_TRUE(
      data_use_ui_tab_model()->CheckAndResetDataUseTrackingEnded(kFooTabId));
  EXPECT_FALSE(
      data_use_ui_tab_model()->CheckAndResetDataUseTrackingEnded(kFooTabId));
  EXPECT_FALSE(
      data_use_ui_tab_model()->CheckAndResetDataUseTrackingStarted(kFooTabId));

  // The tab enters the tracking state again.
  data_use_tab_model()->NotifyObserversOfTrackingStarting(kFooTabId);
  EXPECT_TRUE(
      data_use_ui_tab_model()->CheckAndResetDataUseTrackingStarted(kFooTabId));
  EXPECT_FALSE(
      data_use_ui_tab_model()->CheckAndResetDataUseTrackingStarted(kFooTabId));

  // The tab exits the tracking state.
  data_use_tab_model()->NotifyObserversOfTrackingEnding(kFooTabId);
  EXPECT_FALSE(
      data_use_ui_tab_model()->CheckAndResetDataUseTrackingStarted(kFooTabId));

  // The tab enters the tracking state again.
  data_use_tab_model()->NotifyObserversOfTrackingStarting(kFooTabId);
  data_use_tab_model()->NotifyObserversOfTrackingStarting(kFooTabId);
  EXPECT_TRUE(
      data_use_ui_tab_model()->CheckAndResetDataUseTrackingStarted(kFooTabId));
  EXPECT_FALSE(
      data_use_ui_tab_model()->CheckAndResetDataUseTrackingStarted(kFooTabId));

  // ShowExit should return true only once.
  data_use_tab_model()->NotifyObserversOfTrackingEnding(kBarTabId);
  EXPECT_FALSE(
      data_use_ui_tab_model()->CheckAndResetDataUseTrackingStarted(kBarTabId));
  EXPECT_TRUE(
      data_use_ui_tab_model()->CheckAndResetDataUseTrackingEnded(kBarTabId));
  EXPECT_FALSE(
      data_use_ui_tab_model()->CheckAndResetDataUseTrackingEnded(kBarTabId));
  EXPECT_FALSE(
      data_use_ui_tab_model()->CheckAndResetDataUseTrackingStarted(kBarTabId));

  data_use_ui_tab_model()->ReportTabClosure(kFooTabId);
  data_use_ui_tab_model()->ReportTabClosure(kBarTabId);

  //  CheckAndResetDataUseTrackingStarted/Ended should return false for closed
  //  tabs.
  data_use_tab_model()->NotifyObserversOfTrackingStarting(kBazTabId);
  data_use_ui_tab_model()->ReportTabClosure(kBazTabId);
  EXPECT_FALSE(
      data_use_ui_tab_model()->CheckAndResetDataUseTrackingStarted(kBazTabId));
  EXPECT_FALSE(
      data_use_ui_tab_model()->CheckAndResetDataUseTrackingEnded(kBazTabId));
}

// Tests if the Entrance/Exit UI state is tracked correctly.
TEST_F(DataUseUITabModelTest, EntranceExitStateForDialog) {
  SetUpDataUseUITabModel();

  const SessionID::id_type kFooTabId = 1;

  std::vector<std::string> url_regexes;
  url_regexes.push_back(
      "http://www[.]foo[.]com/#q=.*|https://www[.]foo[.]com/#q=.*");
  RegisterURLRegexes(std::vector<std::string>(url_regexes.size(), kFooPackage),
                     url_regexes,
                     std::vector<std::string>(url_regexes.size(), kFooLabel));

  SessionID::id_type foo_tab_id = kFooTabId;

  const struct {
    // True if a dialog box was shown to the user. It may not be shown if the
    // user has previously selected the option to opt out.
    bool continue_dialog_box_shown;
    bool user_proceeded_with_navigation;
  } tests[] = {
      {false, false}, {true, true}, {true, false},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    // Start a new tab.
    ++foo_tab_id;
    data_use_ui_tab_model()->ReportBrowserNavigation(
        GURL(kURLFoo), ui::PAGE_TRANSITION_GENERATED, foo_tab_id);

    // |data_use_ui_tab_model| should receive callback about starting of
    // tracking of data usage for |foo_tab_id|.
    EXPECT_TRUE(data_use_ui_tab_model()->CheckAndResetDataUseTrackingStarted(
        foo_tab_id))
        << i;
    EXPECT_FALSE(data_use_ui_tab_model()->CheckAndResetDataUseTrackingStarted(
        foo_tab_id))
        << i;
    EXPECT_FALSE(
        data_use_ui_tab_model()->CheckAndResetDataUseTrackingEnded(foo_tab_id))
        << i;

    std::string got_label;
    data_use_tab_model()->GetLabelForTabAtTime(
        foo_tab_id, base::TimeTicks::Now(), &got_label);
    EXPECT_EQ(kFooLabel, got_label) << i;

    // Tab enters non-tracking state.
    data_use_ui_tab_model()->ReportBrowserNavigation(
        GURL("https://www.bar.com/#q=abc"),
        ui::PageTransition::PAGE_TRANSITION_TYPED, foo_tab_id);

    EXPECT_TRUE(
        data_use_ui_tab_model()->CheckAndResetDataUseTrackingEnded(foo_tab_id))
        << i;
    EXPECT_FALSE(
        data_use_ui_tab_model()->CheckAndResetDataUseTrackingEnded(foo_tab_id))
        << i;

    // Tab enters tracking state.
    data_use_ui_tab_model()->ReportBrowserNavigation(
        GURL(kURLFoo), ui::PAGE_TRANSITION_GENERATED, foo_tab_id);

    EXPECT_TRUE(data_use_ui_tab_model()->CheckAndResetDataUseTrackingStarted(
        foo_tab_id))
        << i;

    // Tab may enter non-tracking state.
    EXPECT_TRUE(data_use_ui_tab_model()->WouldDataUseTrackingEnd(
        "https://www.bar.com/#q=abc", ui::PageTransition::PAGE_TRANSITION_TYPED,
        foo_tab_id));
    if (tests[i].continue_dialog_box_shown &&
        tests[i].user_proceeded_with_navigation) {
      data_use_ui_tab_model()->UserClickedContinueOnDialogBox(foo_tab_id);
    }

    if (tests[i].user_proceeded_with_navigation) {
      data_use_ui_tab_model()->ReportBrowserNavigation(
          GURL("https://www.bar.com/#q=abc"),
          ui::PageTransition::PAGE_TRANSITION_TYPED, foo_tab_id);
    }

    const std::string expected_label =
        tests[i].user_proceeded_with_navigation ? "" : kFooLabel;
    data_use_tab_model()->GetLabelForTabAtTime(
        foo_tab_id, base::TimeTicks::Now(), &got_label);
    EXPECT_EQ(expected_label, got_label) << i;

    if (tests[i].user_proceeded_with_navigation) {
      // No UI element should be shown afterwards if the dialog box was shown
      // before.
      EXPECT_NE(tests[i].continue_dialog_box_shown,
                data_use_ui_tab_model()->CheckAndResetDataUseTrackingEnded(
                    foo_tab_id))
          << i;
    } else {
      EXPECT_FALSE(data_use_ui_tab_model()->CheckAndResetDataUseTrackingEnded(
          foo_tab_id))
          << i;
    }
  }
}

// Checks if page transition type is converted correctly.
TEST_F(DataUseUITabModelTest, ConvertTransitionType) {
  SetUpDataUseUITabModel();

  const struct {
    ui::PageTransition page_transition;
    const std::string url;
    bool expected_return;
    DataUseTabModel::TransitionType expected_transition_type;
  } tests[] = {
      {ui::PageTransition(ui::PAGE_TRANSITION_TYPED), std::string(), true,
       DataUseTabModel::TRANSITION_OMNIBOX_NAVIGATION},
      {ui::PageTransition(ui::PAGE_TRANSITION_TYPED | 0xFF00), std::string(),
       true, DataUseTabModel::TRANSITION_OMNIBOX_NAVIGATION},
      {ui::PageTransition(ui::PAGE_TRANSITION_TYPED | 0xFFFF00), std::string(),
       true, DataUseTabModel::TRANSITION_OMNIBOX_NAVIGATION},
      {ui::PageTransition(ui::PAGE_TRANSITION_TYPED | 0x12FFFF00),
       std::string(), true, DataUseTabModel::TRANSITION_OMNIBOX_NAVIGATION},
      {ui::PageTransition(ui::PAGE_TRANSITION_AUTO_BOOKMARK), std::string(),
       true, DataUseTabModel::TRANSITION_BOOKMARK},
      {ui::PageTransition(ui::PAGE_TRANSITION_AUTO_BOOKMARK | 0xFF00),
       std::string(), true, DataUseTabModel::TRANSITION_BOOKMARK},
      {ui::PageTransition(ui::PAGE_TRANSITION_AUTO_BOOKMARK | 0xFFFF00),
       std::string(), true, DataUseTabModel::TRANSITION_BOOKMARK},
      {ui::PageTransition(ui::PAGE_TRANSITION_AUTO_BOOKMARK | 0x12FFFF00),
       std::string(), true, DataUseTabModel::TRANSITION_BOOKMARK},
      {ui::PageTransition(ui::PAGE_TRANSITION_GENERATED), std::string(), true,
       DataUseTabModel::TRANSITION_OMNIBOX_SEARCH},
      {ui::PageTransition(ui::PAGE_TRANSITION_GENERATED | 0xFF00),
       std::string(), true, DataUseTabModel::TRANSITION_OMNIBOX_SEARCH},
      {ui::PageTransition(ui::PAGE_TRANSITION_GENERATED | 0xFFFF00),
       std::string(), true, DataUseTabModel::TRANSITION_OMNIBOX_SEARCH},
      {ui::PageTransition(ui::PAGE_TRANSITION_GENERATED | 0x12FFFF00),
       std::string(), true, DataUseTabModel::TRANSITION_OMNIBOX_SEARCH},
      {ui::PageTransition(ui::PAGE_TRANSITION_RELOAD), std::string(), true,
       DataUseTabModel::TRANSITION_RELOAD},
      {ui::PageTransition(ui::PAGE_TRANSITION_RELOAD | 0xFF00), std::string(),
       true, DataUseTabModel::TRANSITION_RELOAD},
      {ui::PageTransition(ui::PAGE_TRANSITION_RELOAD | 0xFFFF00), std::string(),
       true, DataUseTabModel::TRANSITION_RELOAD},
      {ui::PageTransition(ui::PAGE_TRANSITION_RELOAD | 0x12FFFF00),
       std::string(), true, DataUseTabModel::TRANSITION_RELOAD},

      // Navigating back or forward.
      {ui::PageTransition(ui::PAGE_TRANSITION_RELOAD |
                          ui::PAGE_TRANSITION_FORWARD_BACK),
       std::string(), false},
      {ui::PageTransition(ui::PAGE_TRANSITION_RELOAD |
                          ui::PAGE_TRANSITION_AUTO_SUBFRAME),
       std::string(), false},
      {ui::PageTransition(ui::PAGE_TRANSITION_RELOAD |
                          ui::PAGE_TRANSITION_MANUAL_SUBFRAME),
       std::string(), false},
      {ui::PageTransition(ui::PAGE_TRANSITION_RELOAD |
                          ui::PAGE_TRANSITION_FORM_SUBMIT),
       std::string(), false},

      // Navigating history.
      {ui::PageTransition(ui::PAGE_TRANSITION_AUTO_TOPLEVEL),
       kChromeUIHistoryFrameURL, true,
       DataUseTabModel::TRANSITION_HISTORY_ITEM},
      {ui::PageTransition(ui::PAGE_TRANSITION_AUTO_TOPLEVEL),
       kChromeUIHistoryURL, true, DataUseTabModel::TRANSITION_HISTORY_ITEM},
      {ui::PageTransition(ui::PAGE_TRANSITION_AUTO_TOPLEVEL), std::string(),
       false},
  };

  for (const auto& test : tests) {
    DataUseTabModel::TransitionType transition_type;
    EXPECT_EQ(test.expected_return,
              data_use_ui_tab_model()->ConvertTransitionType(
                  test.page_transition, GURL(test.url), &transition_type));
    if (test.expected_return)
      EXPECT_EQ(test.expected_transition_type, transition_type);
  }
}

// Tests that UI navigation events are buffered until control app not installed
// callback is received.
TEST_F(DataUseUITabModelTest,
       ProcessBufferedNavigationEventsAfterControlAppNotInstalled) {
  MockTabDataUseObserver mock_observer;
  data_use_tab_model()->AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnDataUseTabModelReady()).Times(0);
  EXPECT_CALL(mock_observer, NotifyTrackingStarting(testing::_)).Times(0);
  data_use_ui_tab_model()->ReportBrowserNavigation(
      GURL(kURLFoo), ui::PAGE_TRANSITION_LINK, kTabIDFoo);

  data_use_ui_tab_model()->SetDataUseTabModel(data_use_tab_model());
  data_use_ui_tab_model()->ReportBrowserNavigation(
      GURL(kURLFoo), ui::PAGE_TRANSITION_LINK, kTabIDBar);

  // Navigation events will be buffered, and tracking will not start.
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // When control app not installed callback is received, ready event should be
  // triggered, buffered navigation events should be processed, and tracking
  // should not start.
  EXPECT_CALL(mock_observer, OnDataUseTabModelReady()).Times(1);
  EXPECT_CALL(mock_observer, NotifyTrackingStarting(testing::_)).Times(0);
  data_use_tab_model()->OnControlAppInstallStateChange(false);
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Subsequent navigation events should not start tracking.
  EXPECT_CALL(mock_observer, NotifyTrackingStarting(testing::_)).Times(0);
  data_use_ui_tab_model()->ReportBrowserNavigation(
      GURL(kURLFoo), ui::PAGE_TRANSITION_LINK, kTabIDBaz);
}

// Tests that UI navigation events are buffered until control app is installed
// and matching rules are fetched.
TEST_F(DataUseUITabModelTest, ProcessBufferedNavigationEventsAfterRuleFetch) {
  std::vector<std::string> url_regexes;
  url_regexes.push_back(
      "http://www[.]foo[.]com/#q=.*|https://www[.]foo[.]com/#q=.*");

  MockTabDataUseObserver mock_observer;
  data_use_tab_model()->AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnDataUseTabModelReady()).Times(0);
  EXPECT_CALL(mock_observer, NotifyTrackingStarting(testing::_)).Times(0);
  data_use_ui_tab_model()->ReportBrowserNavigation(
      GURL(kURLFoo), ui::PAGE_TRANSITION_LINK, kTabIDFoo);

  data_use_ui_tab_model()->SetDataUseTabModel(data_use_tab_model());
  data_use_ui_tab_model()->ReportBrowserNavigation(
      GURL(kURLFoo), ui::PAGE_TRANSITION_LINK, kTabIDBar);

  // Navigation events will be buffered, and tracking will not start.
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // When control app installed callback is received, ready event should be
  // triggered, buffered navigation events should be processed, and tracking
  // should start.
  EXPECT_CALL(mock_observer, OnDataUseTabModelReady()).Times(1);
  EXPECT_CALL(mock_observer, NotifyTrackingStarting(kTabIDFoo)).Times(1);
  EXPECT_CALL(mock_observer, NotifyTrackingStarting(kTabIDBar)).Times(1);

  base::RunLoop().RunUntilIdle();
  data_use_tab_model()->OnControlAppInstallStateChange(true);

  RegisterURLRegexes(std::vector<std::string>(url_regexes.size(), kFooPackage),
                     url_regexes,
                     std::vector<std::string>(url_regexes.size(), kFooLabel));
  base::RunLoop().RunUntilIdle();

  // Subsequent navigation events should immediately start tracking.
  EXPECT_CALL(mock_observer, NotifyTrackingStarting(kTabIDBaz)).Times(1);
  data_use_ui_tab_model()->ReportBrowserNavigation(
      GURL(kURLFoo), ui::PAGE_TRANSITION_LINK, kTabIDBaz);
}

// Tests that UI navigation events are buffered until the buffer reaches a
// maximum imposed limit.
TEST_F(DataUseUITabModelTest, ProcessBufferedNavigationEventsAfterMaxLimit) {
  std::vector<std::string> url_regexes;
  url_regexes.push_back(
      "http://www[.]foo[.]com/#q=.*|https://www[.]foo[.]com/#q=.*");

  MockTabDataUseObserver mock_observer;
  data_use_tab_model()->AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, NotifyTrackingStarting(kTabIDFoo)).Times(0);

  const size_t kDefaultMaxNavigationEventsBuffered = 1000;
  // Expect that the max imposed limit will be reached, and the buffer will be
  // cleared.
  for (size_t count = 0; count < kDefaultMaxNavigationEventsBuffered; ++count) {
    data_use_ui_tab_model()->ReportBrowserNavigation(
        GURL(kURLFoo), ui::PAGE_TRANSITION_LINK, kTabIDFoo);
  }

  // When control app installed callback is received, ready event will be
  // triggered. But tracking should not start since the buffer is cleared.
  EXPECT_CALL(mock_observer, OnDataUseTabModelReady()).Times(1);
  data_use_ui_tab_model()->SetDataUseTabModel(data_use_tab_model());
  data_use_tab_model()->OnControlAppInstallStateChange(true);
  base::RunLoop().RunUntilIdle();
  RegisterURLRegexes(std::vector<std::string>(url_regexes.size(), kFooPackage),
                     url_regexes,
                     std::vector<std::string>(url_regexes.size(), kFooLabel));
  base::RunLoop().RunUntilIdle();
}

}  // namespace android

}  // namespace chrome
