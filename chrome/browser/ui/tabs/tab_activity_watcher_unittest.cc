// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_activity_watcher.h"

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_metrics_logger.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContentsTester;
using testing::_;
using testing::StrictMock;

namespace {

const char* kTestUrls[] = {"http://example.com", "https://google.fake"};

// Mocks the LogBackgroundTab method for verification.
class MockTabMetricsLogger : public TabMetricsLogger {
 public:
  MockTabMetricsLogger() = default;

  // TabMetricsLogger:
  MOCK_METHOD2(LogBackgroundTab,
               void(ukm::SourceId ukm_source_id,
                    content::WebContents* web_contents));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTabMetricsLogger);
};

// Helper class to respond to WebContents lifecycle events we can't
// trigger/simulate.
class TestWebContentsObserver : public content::WebContentsObserver {
 public:
  explicit TestWebContentsObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  // content::WebContentsObserver:
  void WebContentsDestroyed() override {
    // Verify that a WebContents being destroyed does not trigger logging.
    web_contents()->WasHidden();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWebContentsObserver);
};

}  // namespace

// Inherits from ChromeRenderViewHostTestHarness to use TestWebContents and
// Profile.
class TabActivityWatcherTest : public ChromeRenderViewHostTestHarness {
 protected:
  TabActivityWatcherTest() {
    // Create and pass a unique_ptr, but retain a pointer to the object.
    auto mock_logger_transient =
        std::make_unique<StrictMock<MockTabMetricsLogger>>();
    mock_logger_ = mock_logger_transient.get();
    TabActivityWatcher::GetInstance()->SetTabMetricsLoggerForTest(
        std::move(mock_logger_transient));
    TabActivityWatcher::GetInstance()->DisableLogTimeoutForTest();
  }

  ~TabActivityWatcherTest() override {
    TabActivityWatcher::GetInstance()->SetTabMetricsLoggerForTest(nullptr);
  }

  // Creates a new WebContents suitable for testing, adds it to the tab strip
  // and navigates it to |initial_url|. The result is owned by the
  // TabStripModel, so its tab must be closed later (e.g. via CloseAllTabs()).
  content::WebContents* AddWebContentsAndNavigate(
      TabStripModel* tab_strip_model,
      const GURL& initial_url) {
    content::WebContents* test_contents =
        WebContentsTester::CreateTestWebContents(profile(), nullptr);

    // Create the TestWebContentsObserver to observe |test_contents|. When the
    // WebContents is destroyed, the observer will be reset automatically.
    observers_.push_back(
        std::make_unique<TestWebContentsObserver>(test_contents));

    tab_strip_model->AppendWebContents(test_contents, false);
    return test_contents;
  }

  StrictMock<MockTabMetricsLogger>* mock_logger() { return mock_logger_; }

 private:
  // Owned by TabActivityWatcher.
  StrictMock<MockTabMetricsLogger>* mock_logger_ = nullptr;

  // Owns the observers we've created.
  std::vector<std::unique_ptr<TestWebContentsObserver>> observers_;

  DISALLOW_COPY_AND_ASSIGN(TabActivityWatcherTest);
};

TEST_F(TabActivityWatcherTest, Basic) {
  Browser::CreateParams params(profile(), true);
  auto browser = CreateBrowserWithTestWindowForParams(&params);

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  content::WebContents* test_contents_1 =
      AddWebContentsAndNavigate(tab_strip_model, GURL(kTestUrls[0]));
  content::WebContents* test_contents_2 =
      AddWebContentsAndNavigate(tab_strip_model, GURL(kTestUrls[1]));

  // Start with the leftmost tab activated.
  tab_strip_model->ActivateTabAt(0, false);

  WebContentsTester::For(test_contents_1)
      ->NavigateAndCommit(GURL(kTestUrls[0]));
  WebContentsTester::For(test_contents_2)
      ->NavigateAndCommit(GURL(kTestUrls[1]));

  // Pinning or unpinning test_contents_2 triggers the log.
  // Note that pinning the tab moves it to index 0.
  EXPECT_CALL(*mock_logger(), LogBackgroundTab(_, test_contents_2));
  tab_strip_model->SetTabPinned(1, true);

  EXPECT_CALL(*mock_logger(), LogBackgroundTab(_, test_contents_2));
  tab_strip_model->SetTabPinned(0, false);

  // Activate test_contents_2. Normally this would hide the current tab's
  // aura::Window, which is what actually triggers TabActivityWatcher to log the
  // change.
  EXPECT_CALL(*mock_logger(), LogBackgroundTab(_, test_contents_1));
  tab_strip_model->ActivateTabAt(0, true);
  test_contents_1->WasHidden();

  // Closing the tabs destroys the WebContentses and should not trigger logging.
  EXPECT_CALL(*mock_logger(), LogBackgroundTab(_, _)).Times(0);
  tab_strip_model->CloseAllTabs();
}

// Tests that logging happens when the browser window is hidden (even if the
// WebContents is still the active tab).
TEST_F(TabActivityWatcherTest, HideWindow) {
  Browser::CreateParams params(profile(), true);
  auto browser = CreateBrowserWithTestWindowForParams(&params);

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  content::WebContents* test_contents_1 =
      AddWebContentsAndNavigate(tab_strip_model, GURL(kTestUrls[0]));
  tab_strip_model->ActivateTabAt(0, false);

  // Hiding the window triggers the log.
  EXPECT_CALL(*mock_logger(), LogBackgroundTab(_, test_contents_1));
  test_contents_1->WasHidden();

  EXPECT_CALL(*mock_logger(), LogBackgroundTab(_, _)).Times(0);
  tab_strip_model->CloseAllTabs();
}
