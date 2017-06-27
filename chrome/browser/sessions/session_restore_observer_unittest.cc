// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore_observer.h"

#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/tab_loader.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"

using content::WebContentsTester;

namespace {

const char kDefaultUrl[] = "https://www.google.com";

}  // namespace

class MockSessionRestoreObserver : public SessionRestoreObserver {
 public:
  MockSessionRestoreObserver() { SessionRestore::AddObserver(this); }

  ~MockSessionRestoreObserver() { SessionRestore::RemoveObserver(this); }

  enum class SessionRestoreEvent {
    STARTED_LOADING_TABS,
    FINISHED_LOADING_TABS
  };

  const std::vector<SessionRestoreEvent>& session_restore_events() const {
    return session_restore_events_;
  }

  // SessionRestoreObserver implementation:
  void OnSessionRestoreStartedLoadingTabs() override {
    session_restore_events_.emplace_back(
        SessionRestoreEvent::STARTED_LOADING_TABS);
  }

  void OnSessionRestoreFinishedLoadingTabs() override {
    session_restore_events_.emplace_back(
        SessionRestoreEvent::FINISHED_LOADING_TABS);
  }

 private:
  std::vector<SessionRestoreEvent> session_restore_events_;

  DISALLOW_COPY_AND_ASSIGN(MockSessionRestoreObserver);
};

class SessionRestoreObserverTest : public ChromeRenderViewHostTestHarness {
 public:
  using RestoredTab = SessionRestoreDelegate::RestoredTab;

  SessionRestoreObserverTest() {}

  // testing::Test:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    restored_tabs_.emplace_back(web_contents(), false, false, false);
  }

  void TearDown() override {
    ChromeRenderViewHostTestHarness::TearDown();
    restored_tabs_.clear();
  }

 protected:
  void RestoreTabs() {
    TabLoader::RestoreTabs(restored_tabs_, base::TimeTicks());
  }

  void LoadWebContents(content::WebContents* contents) {
    WebContentsTester::For(contents)->NavigateAndCommit(GURL(kDefaultUrl));
    WebContentsTester::For(contents)->TestSetIsLoading(false);
  }

  const std::vector<MockSessionRestoreObserver::SessionRestoreEvent>&
  session_restore_events() const {
    return mock_observer_.session_restore_events();
  }

  size_t number_of_session_restore_events() const {
    return session_restore_events().size();
  }

 private:
  MockSessionRestoreObserver mock_observer_;
  std::vector<RestoredTab> restored_tabs_;

  DISALLOW_COPY_AND_ASSIGN(SessionRestoreObserverTest);
};

TEST_F(SessionRestoreObserverTest, SingleSessionRestore) {
  RestoreTabs();
  ASSERT_EQ(1u, number_of_session_restore_events());
  EXPECT_EQ(
      MockSessionRestoreObserver::SessionRestoreEvent::STARTED_LOADING_TABS,
      session_restore_events()[0]);

  LoadWebContents(web_contents());
  ASSERT_EQ(2u, number_of_session_restore_events());
  EXPECT_EQ(
      MockSessionRestoreObserver::SessionRestoreEvent::FINISHED_LOADING_TABS,
      session_restore_events()[1]);
}

TEST_F(SessionRestoreObserverTest, SequentialSessionRestore) {
  const int number_of_session_restores = 3;
  size_t event_index = 0;
  for (int i = 0; i < number_of_session_restores; ++i) {
    RestoreTabs();
    ASSERT_EQ(event_index + 1, number_of_session_restore_events());
    EXPECT_EQ(
        MockSessionRestoreObserver::SessionRestoreEvent::STARTED_LOADING_TABS,
        session_restore_events()[event_index++]);

    LoadWebContents(web_contents());
    ASSERT_EQ(event_index + 1, number_of_session_restore_events());
    EXPECT_EQ(
        MockSessionRestoreObserver::SessionRestoreEvent::FINISHED_LOADING_TABS,
        session_restore_events()[event_index++]);
  }
}

TEST_F(SessionRestoreObserverTest, ConcurrentSessionRestore) {
  std::vector<RestoredTab> another_restored_tabs;
  std::unique_ptr<content::WebContents> test_contents(
      WebContentsTester::CreateTestWebContents(browser_context(), nullptr));
  another_restored_tabs.emplace_back(test_contents.get(), false, false, false);

  RestoreTabs();
  TabLoader::RestoreTabs(another_restored_tabs, base::TimeTicks());
  ASSERT_EQ(1u, number_of_session_restore_events());
  EXPECT_EQ(
      MockSessionRestoreObserver::SessionRestoreEvent::STARTED_LOADING_TABS,
      session_restore_events()[0]);

  LoadWebContents(web_contents());
  LoadWebContents(test_contents.get());

  ASSERT_EQ(2u, number_of_session_restore_events());
  EXPECT_EQ(
      MockSessionRestoreObserver::SessionRestoreEvent::FINISHED_LOADING_TABS,
      session_restore_events()[1]);
}
