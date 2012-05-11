// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/sessions/session_backend.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebHTTPBody.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebHistoryItem.h"
#include "webkit/glue/glue_serialize.h"

using content::NavigationEntry;

class SessionServiceTest : public BrowserWithTestWindowTest,
                           public content::NotificationObserver {
 public:
  SessionServiceTest() : window_bounds(0, 1, 2, 3), sync_save_count_(0){}

 protected:
  virtual void SetUp() {
    BrowserWithTestWindowTest::SetUp();
    std::string b = base::Int64ToString(base::Time::Now().ToInternalValue());

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    path_ = temp_dir_.path().Append(FILE_PATH_LITERAL("SessionTestDirs"));
    ASSERT_TRUE(file_util::CreateDirectory(path_));
    path_ = path_.AppendASCII(b);

    SessionService* session_service = new SessionService(path_);
    helper_.set_service(session_service);

    service()->SetWindowType(
        window_id, Browser::TYPE_TABBED, SessionService::TYPE_NORMAL);
    service()->SetWindowBounds(window_id,
                               window_bounds,
                               ui::SHOW_STATE_NORMAL);
  }

  // Upon notification, increment the sync_save_count variable
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) {
    ASSERT_EQ(type, chrome::NOTIFICATION_SESSION_SERVICE_SAVED);
    sync_save_count_++;
  }

  virtual void TearDown() {
    helper_.set_service(NULL);
  }

  void UpdateNavigation(const SessionID& window_id,
                        const SessionID& tab_id,
                        const TabNavigation& navigation,
                        int index,
                        bool select) {
    scoped_ptr<NavigationEntry> entry(NavigationEntry::Create());
    entry->SetURL(navigation.virtual_url());
    entry->SetReferrer(navigation.referrer());
    entry->SetTitle(navigation.title());
    entry->SetContentState(navigation.state());
    entry->SetTransitionType(navigation.transition());
    entry->SetHasPostData(
        navigation.type_mask() & TabNavigation::HAS_POST_DATA);
    entry->SetOriginalRequestURL(navigation.original_request_url());
    entry->SetIsOverridingUserAgent(navigation.is_overriding_user_agent());
    service()->UpdateTabNavigation(window_id, tab_id, index, *entry.get());
    if (select)
      service()->SetSelectedNavigationIndex(window_id, tab_id, index);
  }

  void ReadWindows(std::vector<SessionWindow*>* windows) {
    // Forces closing the file.
    helper_.set_service(NULL);

    SessionService* session_service = new SessionService(path_);
    helper_.set_service(session_service);
    helper_.ReadWindows(windows);
  }

  // Configures the session service with one window with one tab and a single
  // navigation. If |pinned_state| is true or |write_always| is true, the
  // pinned state of the tab is updated. The session service is then recreated
  // and the pinned state of the read back tab is returned.
  bool CreateAndWriteSessionWithOneTab(bool pinned_state, bool write_always) {
    SessionID tab_id;
    TabNavigation nav1(0, GURL("http://google.com"),
                       content::Referrer(GURL("http://www.referrer.com"),
                                         WebKit::WebReferrerPolicyDefault),
                       ASCIIToUTF16("abc"), "def",
                       content::PAGE_TRANSITION_QUALIFIER_MASK);

    helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
    UpdateNavigation(window_id, tab_id, nav1, 0, true);

    if (pinned_state || write_always)
      helper_.service()->SetPinnedState(window_id, tab_id, pinned_state);

    ScopedVector<SessionWindow> windows;
    ReadWindows(&(windows.get()));

    EXPECT_EQ(1U, windows->size());
    if (HasFatalFailure())
      return false;
    EXPECT_EQ(1U, windows[0]->tabs.size());
    if (HasFatalFailure())
      return false;

    SessionTab* tab = windows[0]->tabs[0];
    helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);

    return tab->pinned;
  }

  SessionService* service() { return helper_.service(); }

  SessionBackend* backend() { return helper_.backend(); }

  const gfx::Rect window_bounds;

  SessionID window_id;

  int sync_save_count_;

  // Path used in testing.
  ScopedTempDir temp_dir_;
  FilePath path_;

  SessionServiceTestHelper helper_;
};

TEST_F(SessionServiceTest, Basic) {
  SessionID tab_id;
  ASSERT_NE(window_id.id(), tab_id.id());

  TabNavigation nav1(0, GURL("http://google.com"),
                     content::Referrer(GURL("http://www.referrer.com"),
                                       WebKit::WebReferrerPolicyDefault),
                     ASCIIToUTF16("abc"), "def",
                     content::PAGE_TRANSITION_QUALIFIER_MASK);
  nav1.set_original_request_url(GURL("http://original.request.com"));

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, 0, true);

  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));

  ASSERT_EQ(1U, windows->size());
  ASSERT_TRUE(window_bounds == windows[0]->bounds);
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(window_id.id(), windows[0]->window_id.id());
  ASSERT_EQ(1U, windows[0]->tabs.size());
  ASSERT_EQ(Browser::TYPE_TABBED, windows[0]->type);

  SessionTab* tab = windows[0]->tabs[0];
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);

  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
}

// Make sure we persist post entries.
TEST_F(SessionServiceTest, PersistPostData) {
  SessionID tab_id;
  ASSERT_NE(window_id.id(), tab_id.id());

  TabNavigation nav1(0, GURL("http://google.com"), content::Referrer(),
                     ASCIIToUTF16("abc"), std::string(),
                     content::PAGE_TRANSITION_QUALIFIER_MASK);
  nav1.set_type_mask(TabNavigation::HAS_POST_DATA);

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, 0, true);

  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));

  helper_.AssertSingleWindowWithSingleTab(windows.get(), 1);
}

TEST_F(SessionServiceTest, ClosingTabStaysClosed) {
  SessionID tab_id;
  SessionID tab2_id;
  ASSERT_NE(tab_id.id(), tab2_id.id());

  TabNavigation nav1(0, GURL("http://google.com"), content::Referrer(),
                     ASCIIToUTF16("abc"), "def",
                     content::PAGE_TRANSITION_QUALIFIER_MASK);
  TabNavigation nav2(0, GURL("http://google2.com"), content::Referrer(),
                     ASCIIToUTF16("abcd"), "defg",
                     content::PAGE_TRANSITION_AUTO_BOOKMARK);

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, 0, true);

  helper_.PrepareTabInWindow(window_id, tab2_id, 1, false);
  UpdateNavigation(window_id, tab2_id, nav2, 0, true);
  service()->TabClosed(window_id, tab2_id, false);

  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));

  ASSERT_EQ(1U, windows->size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(window_id.id(), windows[0]->window_id.id());
  ASSERT_EQ(1U, windows[0]->tabs.size());

  SessionTab* tab = windows[0]->tabs[0];
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);

  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
}

TEST_F(SessionServiceTest, Pruning) {
  SessionID tab_id;

  TabNavigation nav1(0, GURL("http://google.com"), content::Referrer(),
                     ASCIIToUTF16("abc"), "def",
                     content::PAGE_TRANSITION_QUALIFIER_MASK);
  TabNavigation nav2(0, GURL("http://google2.com"), content::Referrer(),
                     ASCIIToUTF16("abcd"), "defg",
                     content::PAGE_TRANSITION_AUTO_BOOKMARK);

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  for (int i = 0; i < 6; ++i) {
    TabNavigation& nav = (i % 2) == 0 ? nav1 : nav2;
    UpdateNavigation(window_id, tab_id, nav, i, true);
  }
  service()->TabNavigationPathPrunedFromBack(window_id, tab_id, 3);

  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));

  ASSERT_EQ(1U, windows->size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(1U, windows[0]->tabs.size());

  SessionTab* tab = windows[0]->tabs[0];
  // We left the selected index at 5, then pruned. When rereading the
  // index should get reset to last valid navigation, which is 2.
  helper_.AssertTabEquals(window_id, tab_id, 0, 2, 3, *tab);

  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
  helper_.AssertNavigationEquals(nav2, tab->navigations[1]);
  helper_.AssertNavigationEquals(nav1, tab->navigations[2]);
}

TEST_F(SessionServiceTest, TwoWindows) {
  SessionID window2_id;
  SessionID tab1_id;
  SessionID tab2_id;

  TabNavigation nav1(0, GURL("http://google.com"), content::Referrer(),
                     ASCIIToUTF16("abc"), "def",
                     content::PAGE_TRANSITION_QUALIFIER_MASK);
  TabNavigation nav2(0, GURL("http://google2.com"), content::Referrer(),
                     ASCIIToUTF16("abcd"), "defg",
                     content::PAGE_TRANSITION_AUTO_BOOKMARK);

  helper_.PrepareTabInWindow(window_id, tab1_id, 0, true);
  UpdateNavigation(window_id, tab1_id, nav1, 0, true);

  const gfx::Rect window2_bounds(3, 4, 5, 6);
  service()->SetWindowType(
      window2_id, Browser::TYPE_TABBED, SessionService::TYPE_NORMAL);
  service()->SetWindowBounds(window2_id,
                             window2_bounds,
                             ui::SHOW_STATE_MAXIMIZED);
  helper_.PrepareTabInWindow(window2_id, tab2_id, 0, true);
  UpdateNavigation(window2_id, tab2_id, nav2, 0, true);

  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));

  ASSERT_EQ(2U, windows->size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(0, windows[1]->selected_tab_index);
  ASSERT_EQ(1U, windows[0]->tabs.size());
  ASSERT_EQ(1U, windows[1]->tabs.size());

  SessionTab* rt1;
  SessionTab* rt2;
  if (windows[0]->window_id.id() == window_id.id()) {
    ASSERT_EQ(window2_id.id(), windows[1]->window_id.id());
    ASSERT_EQ(ui::SHOW_STATE_NORMAL, windows[0]->show_state);
    ASSERT_EQ(ui::SHOW_STATE_MAXIMIZED, windows[1]->show_state);
    rt1 = windows[0]->tabs[0];
    rt2 = windows[1]->tabs[0];
  } else {
    ASSERT_EQ(window2_id.id(), windows[0]->window_id.id());
    ASSERT_EQ(window_id.id(), windows[1]->window_id.id());
    ASSERT_EQ(ui::SHOW_STATE_MAXIMIZED, windows[0]->show_state);
    ASSERT_EQ(ui::SHOW_STATE_NORMAL, windows[1]->show_state);
    rt1 = windows[1]->tabs[0];
    rt2 = windows[0]->tabs[0];
  }
  SessionTab* tab = rt1;
  helper_.AssertTabEquals(window_id, tab1_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);

  tab = rt2;
  helper_.AssertTabEquals(window2_id, tab2_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav2, tab->navigations[0]);
}

TEST_F(SessionServiceTest, WindowWithNoTabsGetsPruned) {
  SessionID window2_id;
  SessionID tab1_id;
  SessionID tab2_id;

  TabNavigation nav1(0, GURL("http://google.com"), content::Referrer(),
                     ASCIIToUTF16("abc"), "def",
                     content::PAGE_TRANSITION_QUALIFIER_MASK);

  helper_.PrepareTabInWindow(window_id, tab1_id, 0, true);
  UpdateNavigation(window_id, tab1_id, nav1, 0, true);

  const gfx::Rect window2_bounds(3, 4, 5, 6);
  service()->SetWindowType(
      window2_id, Browser::TYPE_TABBED, SessionService::TYPE_NORMAL);
  service()->SetWindowBounds(window2_id,
                             window2_bounds,
                             ui::SHOW_STATE_NORMAL);
  helper_.PrepareTabInWindow(window2_id, tab2_id, 0, true);

  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));

  ASSERT_EQ(1U, windows->size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(1U, windows[0]->tabs.size());
  ASSERT_EQ(window_id.id(), windows[0]->window_id.id());

  SessionTab* tab = windows[0]->tabs[0];
  helper_.AssertTabEquals(window_id, tab1_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
}

TEST_F(SessionServiceTest, ClosingWindowDoesntCloseTabs) {
  SessionID tab_id;
  SessionID tab2_id;
  ASSERT_NE(tab_id.id(), tab2_id.id());

  TabNavigation nav1(0, GURL("http://google.com"), content::Referrer(),
                     ASCIIToUTF16("abc"), "def",
                     content::PAGE_TRANSITION_QUALIFIER_MASK);
  TabNavigation nav2(0, GURL("http://google2.com"), content::Referrer(),
                     ASCIIToUTF16("abcd"), "defg",
                     content::PAGE_TRANSITION_AUTO_BOOKMARK);

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, 0, true);

  helper_.PrepareTabInWindow(window_id, tab2_id, 1, false);
  UpdateNavigation(window_id, tab2_id, nav2, 0, true);

  service()->WindowClosing(window_id);

  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));

  ASSERT_EQ(1U, windows->size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(window_id.id(), windows[0]->window_id.id());
  ASSERT_EQ(2U, windows[0]->tabs.size());

  SessionTab* tab = windows[0]->tabs[0];
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);

  tab = windows[0]->tabs[1];
  helper_.AssertTabEquals(window_id, tab2_id, 1, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav2, tab->navigations[0]);
}

TEST_F(SessionServiceTest, WindowCloseCommittedAfterNavigate) {
  SessionID window2_id;
  SessionID tab_id;
  SessionID tab2_id;
  ASSERT_NE(window2_id.id(), window_id.id());

  service()->SetWindowType(
      window2_id, Browser::TYPE_TABBED, SessionService::TYPE_NORMAL);
  service()->SetWindowBounds(window2_id,
                             window_bounds,
                             ui::SHOW_STATE_NORMAL);

  TabNavigation nav1(0, GURL("http://google.com"), content::Referrer(),
                     ASCIIToUTF16("abc"), "def",
                     content::PAGE_TRANSITION_QUALIFIER_MASK);
  TabNavigation nav2(0, GURL("http://google2.com"), content::Referrer(),
                     ASCIIToUTF16("abcd"), "defg",
                     content::PAGE_TRANSITION_AUTO_BOOKMARK);

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, 0, true);

  helper_.PrepareTabInWindow(window2_id, tab2_id, 0, false);
  UpdateNavigation(window2_id, tab2_id, nav2, 0, true);

  service()->WindowClosing(window2_id);
  service()->TabClosed(window2_id, tab2_id, false);
  service()->WindowClosed(window2_id);

  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));

  ASSERT_EQ(1U, windows->size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(window_id.id(), windows[0]->window_id.id());
  ASSERT_EQ(1U, windows[0]->tabs.size());

  SessionTab* tab = windows[0]->tabs[0];
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
}

// Makes sure we don't track popups.
TEST_F(SessionServiceTest, IgnorePopups) {
  if (browser_defaults::kRestorePopups)
    return;  // This test is only applicable if popups aren't restored.

  SessionID window2_id;
  SessionID tab_id;
  SessionID tab2_id;
  ASSERT_NE(window2_id.id(), window_id.id());

  service()->SetWindowType(
      window2_id, Browser::TYPE_POPUP, SessionService::TYPE_NORMAL);
  service()->SetWindowBounds(window2_id,
                             window_bounds,
                             ui::SHOW_STATE_NORMAL);

  TabNavigation nav1(0, GURL("http://google.com"), content::Referrer(),
                     ASCIIToUTF16("abc"), "def",
                     content::PAGE_TRANSITION_QUALIFIER_MASK);
  TabNavigation nav2(0, GURL("http://google2.com"), content::Referrer(),
                     ASCIIToUTF16("abcd"), "defg",
                     content::PAGE_TRANSITION_AUTO_BOOKMARK);

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, 0, true);

  helper_.PrepareTabInWindow(window2_id, tab2_id, 0, false);
  UpdateNavigation(window2_id, tab2_id, nav2, 0, true);

  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));

  ASSERT_EQ(1U, windows->size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(window_id.id(), windows[0]->window_id.id());
  ASSERT_EQ(1U, windows[0]->tabs.size());

  SessionTab* tab = windows[0]->tabs[0];
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
}

// Makes sure we track popups.
TEST_F(SessionServiceTest, RestorePopup) {
  if (!browser_defaults::kRestorePopups)
    return;  // This test is only applicable if popups are restored.

  SessionID window2_id;
  SessionID tab_id;
  SessionID tab2_id;
  ASSERT_NE(window2_id.id(), window_id.id());

  service()->SetWindowType(
      window2_id, Browser::TYPE_POPUP, SessionService::TYPE_NORMAL);
  service()->SetWindowBounds(window2_id,
                             window_bounds,
                             ui::SHOW_STATE_NORMAL);

  TabNavigation nav1(0, GURL("http://google.com"), content::Referrer(),
                     ASCIIToUTF16("abc"), "def",
                     content::PAGE_TRANSITION_QUALIFIER_MASK);
  TabNavigation nav2(0, GURL("http://google2.com"), content::Referrer(),
                     ASCIIToUTF16("abcd"), "defg",
                     content::PAGE_TRANSITION_AUTO_BOOKMARK);

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, 0, true);

  helper_.PrepareTabInWindow(window2_id, tab2_id, 0, false);
  UpdateNavigation(window2_id, tab2_id, nav2, 0, true);

  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));

  ASSERT_EQ(2U, windows->size());
  int tabbed_index = windows[0]->type == Browser::TYPE_TABBED ?
      0 : 1;
  int popup_index = tabbed_index == 0 ? 1 : 0;
  ASSERT_EQ(0, windows[tabbed_index]->selected_tab_index);
  ASSERT_EQ(window_id.id(), windows[tabbed_index]->window_id.id());
  ASSERT_EQ(1U, windows[tabbed_index]->tabs.size());

  SessionTab* tab = windows[tabbed_index]->tabs[0];
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);

  ASSERT_EQ(0, windows[popup_index]->selected_tab_index);
  ASSERT_EQ(window2_id.id(), windows[popup_index]->window_id.id());
  ASSERT_EQ(1U, windows[popup_index]->tabs.size());

  tab = windows[popup_index]->tabs[0];
  helper_.AssertTabEquals(window2_id, tab2_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav2, tab->navigations[0]);
}

#if defined (USE_AURA)
// Makes sure we track apps. Only applicable on aura.
TEST_F(SessionServiceTest, RestoreApp) {
  SessionID window2_id;
  SessionID tab_id;
  SessionID tab2_id;
  ASSERT_NE(window2_id.id(), window_id.id());

  service()->SetWindowType(
      window2_id, Browser::TYPE_POPUP, SessionService::TYPE_APP);
  service()->SetWindowBounds(window2_id,
                             window_bounds,
                             ui::SHOW_STATE_NORMAL);
  service()->SetWindowAppName(window2_id, "TestApp");

  TabNavigation nav1(0, GURL("http://google.com"), content::Referrer(),
                     ASCIIToUTF16("abc"), "def",
                     content::PAGE_TRANSITION_QUALIFIER_MASK);
  TabNavigation nav2(0, GURL("http://google2.com"), content::Referrer(),
                     ASCIIToUTF16("abcd"), "defg",
                     content::PAGE_TRANSITION_AUTO_BOOKMARK);

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, 0, true);

  helper_.PrepareTabInWindow(window2_id, tab2_id, 0, false);
  UpdateNavigation(window2_id, tab2_id, nav2, 0, true);

  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));

  ASSERT_EQ(2U, windows->size());
  int tabbed_index = windows[0]->type == Browser::TYPE_TABBED ?
      0 : 1;
  int app_index = tabbed_index == 0 ? 1 : 0;
  ASSERT_EQ(0, windows[tabbed_index]->selected_tab_index);
  ASSERT_EQ(window_id.id(), windows[tabbed_index]->window_id.id());
  ASSERT_EQ(1U, windows[tabbed_index]->tabs.size());

  SessionTab* tab = windows[tabbed_index]->tabs[0];
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);

  ASSERT_EQ(0, windows[app_index]->selected_tab_index);
  ASSERT_EQ(window2_id.id(), windows[app_index]->window_id.id());
  ASSERT_EQ(1U, windows[app_index]->tabs.size());
  ASSERT_TRUE(windows[app_index]->type == Browser::TYPE_POPUP);
  ASSERT_EQ("TestApp", windows[app_index]->app_name);

  tab = windows[app_index]->tabs[0];
  helper_.AssertTabEquals(window2_id, tab2_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav2, tab->navigations[0]);
}
#endif

// Tests pruning from the front.
TEST_F(SessionServiceTest, PruneFromFront) {
  const std::string base_url("http://google.com/");
  SessionID tab_id;

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);

  // Add 5 navigations, with the 4th selected.
  for (int i = 0; i < 5; ++i) {
    TabNavigation nav(0, GURL(base_url + base::IntToString(i)),
                      content::Referrer(),
                      ASCIIToUTF16("a"), "b",
                      content::PAGE_TRANSITION_QUALIFIER_MASK);
    UpdateNavigation(window_id, tab_id, nav, i, (i == 3));
  }

  // Prune the first two navigations from the front.
  helper_.service()->TabNavigationPathPrunedFromFront(window_id, tab_id, 2);

  // Read back in.
  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));

  ASSERT_EQ(1U, windows->size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(window_id.id(), windows[0]->window_id.id());
  ASSERT_EQ(1U, windows[0]->tabs.size());

  // There shouldn't be an app id.
  EXPECT_TRUE(windows[0]->tabs[0]->extension_app_id.empty());

  // We should be left with three navigations, the 2nd selected.
  SessionTab* tab = windows[0]->tabs[0];
  ASSERT_EQ(1, tab->current_navigation_index);
  EXPECT_EQ(3U, tab->navigations.size());
  EXPECT_TRUE(GURL(base_url + base::IntToString(2)) ==
              tab->navigations[0].virtual_url());
  EXPECT_TRUE(GURL(base_url + base::IntToString(3)) ==
              tab->navigations[1].virtual_url());
  EXPECT_TRUE(GURL(base_url + base::IntToString(4)) ==
              tab->navigations[2].virtual_url());
}

// Prunes from front so that we have no entries.
TEST_F(SessionServiceTest, PruneToEmpty) {
  const std::string base_url("http://google.com/");
  SessionID tab_id;

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);

  // Add 5 navigations, with the 4th selected.
  for (int i = 0; i < 5; ++i) {
    TabNavigation nav(0, GURL(base_url + base::IntToString(i)),
                      content::Referrer(),
                      ASCIIToUTF16("a"), "b",
                      content::PAGE_TRANSITION_QUALIFIER_MASK);
    UpdateNavigation(window_id, tab_id, nav, i, (i == 3));
  }

  // Prune the first two navigations from the front.
  helper_.service()->TabNavigationPathPrunedFromFront(window_id, tab_id, 5);

  // Read back in.
  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));

  ASSERT_EQ(0U, windows->size());
}

// Don't set the pinned state and make sure the pinned value is false.
TEST_F(SessionServiceTest, PinnedDefaultsToFalse) {
  EXPECT_FALSE(CreateAndWriteSessionWithOneTab(false, false));
}

// Explicitly set the pinned state to false and make sure we get back false.
TEST_F(SessionServiceTest, PinnedFalseWhenSetToFalse) {
  EXPECT_FALSE(CreateAndWriteSessionWithOneTab(false, true));
}

// Explicitly set the pinned state to true and make sure we get back true.
TEST_F(SessionServiceTest, PinnedTrue) {
  EXPECT_TRUE(CreateAndWriteSessionWithOneTab(true, true));
}

// Make sure application extension ids are persisted.
TEST_F(SessionServiceTest, PersistApplicationExtensionID) {
  SessionID tab_id;
  ASSERT_NE(window_id.id(), tab_id.id());
  std::string app_id("foo");

  TabNavigation nav1(0, GURL("http://google.com"), content::Referrer(),
                     ASCIIToUTF16("abc"), std::string(),
                     content::PAGE_TRANSITION_QUALIFIER_MASK);

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, 0, true);
  helper_.SetTabExtensionAppID(window_id, tab_id, app_id);

  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));

  helper_.AssertSingleWindowWithSingleTab(windows.get(), 1);
  EXPECT_TRUE(app_id == windows[0]->tabs[0]->extension_app_id);
}

// Check that user agent overrides are persisted.
TEST_F(SessionServiceTest, PersistUserAgentOverrides) {
  SessionID tab_id;
  ASSERT_NE(window_id.id(), tab_id.id());
  std::string user_agent_override("overridden user agent");

  TabNavigation nav1(0, GURL("http://google.com"), content::Referrer(),
                     ASCIIToUTF16("abc"), std::string(),
                     content::PAGE_TRANSITION_QUALIFIER_MASK);
  nav1.set_is_overriding_user_agent(true);

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, 0, true);
  helper_.SetTabUserAgentOverride(window_id, tab_id, user_agent_override);

  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));
  helper_.AssertSingleWindowWithSingleTab(windows.get(), 1);

  SessionTab* tab = windows[0]->tabs[0];
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
  EXPECT_TRUE(user_agent_override == tab->user_agent_override);
}

// Test that the notification for SESSION_SERVICE_SAVED is working properly.
TEST_F(SessionServiceTest, SavedSessionNotification) {
  content::NotificationRegistrar registrar_;
  registrar_.Add(this, chrome::NOTIFICATION_SESSION_SERVICE_SAVED,
                 content::NotificationService::AllSources());
  service()->Save();
  EXPECT_EQ(sync_save_count_, 1);
}

// Makes sure a tab closed by a user gesture is not restored.
TEST_F(SessionServiceTest, CloseTabUserGesture) {
  SessionID tab_id;
  ASSERT_NE(window_id.id(), tab_id.id());

  TabNavigation nav1(0, GURL("http://google.com"),
                     content::Referrer(GURL("http://www.referrer.com"),
                                       WebKit::WebReferrerPolicyDefault),
                     ASCIIToUTF16("abc"), "def",
                     content::PAGE_TRANSITION_QUALIFIER_MASK);

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, 0, true);
  service()->TabClosed(window_id, tab_id, true);

  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));

  ASSERT_TRUE(windows->empty());
}

// Verifies SetWindowBounds maps SHOW_STATE_DEFAULT to SHOW_STATE_NORMAL.
TEST_F(SessionServiceTest, DontPersistDefault) {
  SessionID tab_id;
  ASSERT_NE(window_id.id(), tab_id.id());
  TabNavigation nav1(0, GURL("http://google.com"),
                     content::Referrer(GURL("http://www.referrer.com"),
                                       WebKit::WebReferrerPolicyDefault),
                     ASCIIToUTF16("abc"), "def",
                     content::PAGE_TRANSITION_QUALIFIER_MASK);
  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, 0, true);
  service()->SetWindowBounds(window_id,
                             window_bounds,
                             ui::SHOW_STATE_DEFAULT);

  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));
  ASSERT_EQ(1U, windows->size());
  EXPECT_EQ(ui::SHOW_STATE_NORMAL, windows[0]->show_state);
}

TEST_F(SessionServiceTest, RemovePostData) {
  helper_.service()->save_post_data_ = false;

  SessionID tab_id;
  ASSERT_NE(window_id.id(), tab_id.id());

  std::string content_state("dummy_content_state");
  // Create a TabNavigation containing content_state and representing a POST
  // request.
  TabNavigation nav1(0, GURL("http://google.com"), content::Referrer(),
                     ASCIIToUTF16("title"), content_state,
                     content::PAGE_TRANSITION_QUALIFIER_MASK);
  nav1.set_type_mask(TabNavigation::HAS_POST_DATA);

  // Create a TabNavigation containing content_state and representing a normal
  // request.
  TabNavigation nav2(0, GURL("http://google.com/nopost"), content::Referrer(),
                     ASCIIToUTF16("title"), content_state,
                     content::PAGE_TRANSITION_QUALIFIER_MASK);

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, 0, true);
  UpdateNavigation(window_id, tab_id, nav2, 1, true);

  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));

  helper_.AssertSingleWindowWithSingleTab(windows.get(), 2);

  // Expected: the HTTP body was removed from the content state of the POST
  // navigation but the content state of the normal navigation is intact.
  EXPECT_NE(content_state, windows[0]->tabs[0]->navigations[0].state());
  helper_.AssertNavigationEquals(nav2, windows[0]->tabs[0]->navigations[1]);
}

TEST_F(SessionServiceTest, KeepPostDataWithoutPasswords) {
  helper_.service()->save_post_data_ = true;

  SessionID tab_id;
  ASSERT_NE(window_id.id(), tab_id.id());

  // Create a content state representing a HTTP body without posted passwords.
  WebKit::WebHTTPBody http_body;
  http_body.initialize();
  const char char_data[] = "data";
  http_body.appendData(WebKit::WebData(char_data, sizeof(char_data)-1));
  WebKit::WebHistoryItem history_item;
  history_item.initialize();
  history_item.setHTTPBody(http_body);
  std::string content_state = webkit_glue::HistoryItemToString(history_item);

  // Create a TabNavigation containing content_state and representing a POST
  // request.
  TabNavigation nav1(0, GURL("http://google.com"), content::Referrer(),
                     ASCIIToUTF16("title"), content_state,
                     content::PAGE_TRANSITION_QUALIFIER_MASK);
  nav1.set_type_mask(TabNavigation::HAS_POST_DATA);

  // Create a TabNavigation containing content_state and representing a normal
  // request.
  TabNavigation nav2(0, GURL("http://google.com/nopost"), content::Referrer(),
                     ASCIIToUTF16("title"), content_state,
                     content::PAGE_TRANSITION_QUALIFIER_MASK);

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, 0, true);
  UpdateNavigation(window_id, tab_id, nav2, 1, true);

  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));

  helper_.AssertSingleWindowWithSingleTab(windows.get(), 2);

  // Expected: the content state of both navigations was saved and restored.
  helper_.AssertNavigationEquals(nav1, windows[0]->tabs[0]->navigations[0]);
  helper_.AssertNavigationEquals(nav2, windows[0]->tabs[0]->navigations[1]);
}

TEST_F(SessionServiceTest, RemovePostDataWithPasswords) {
  helper_.service()->save_post_data_ = true;

  SessionID tab_id;
  ASSERT_NE(window_id.id(), tab_id.id());

  // Create a content state representing a HTTP body with posted passwords.
  WebKit::WebHTTPBody http_body;
  http_body.initialize();
  const char char_data[] = "data";
  http_body.appendData(WebKit::WebData(char_data, sizeof(char_data)-1));
  http_body.setContainsPasswordData(true);
  WebKit::WebHistoryItem history_item;
  history_item.initialize();
  history_item.setHTTPBody(http_body);
  std::string content_state = webkit_glue::HistoryItemToString(history_item);

  // Create a TabNavigation containing content_state and representing a POST
  // request with passwords.
  TabNavigation nav1(0, GURL("http://google.com"), content::Referrer(),
                     ASCIIToUTF16("title"), content_state,
                     content::PAGE_TRANSITION_QUALIFIER_MASK);
  nav1.set_type_mask(TabNavigation::HAS_POST_DATA);
  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, 0, true);

  ScopedVector<SessionWindow> windows;
  ReadWindows(&(windows.get()));

  helper_.AssertSingleWindowWithSingleTab(windows.get(), 1);

  // Expected: the HTTP body was removed from the content state of the POST
  // navigation with passwords.
  EXPECT_NE(content_state, windows[0]->tabs[0]->navigations[0].state());
}

// This test is only applicable to chromeos.
#if defined(OS_CHROMEOS)
// Verifies migration of tab/window closed works.
TEST_F(SessionServiceTest, CanOpenV1TabClosed) {
  FilePath v1_file_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &v1_file_path));
  // v1_session_file contains a tab closed command with the original id. The
  // file was generated from ClosingTabStaysClosed. If we successfully processed
  // the file we'll have one tab.
  v1_file_path =
      v1_file_path.AppendASCII("sessions").AppendASCII("v1_session_file");
  FilePath dest_file_path(path_);
  dest_file_path = dest_file_path.AppendASCII("Current Session");

  // Forces closing the file.
  helper_.set_service(NULL);

  ASSERT_TRUE(file_util::CopyFile(v1_file_path, dest_file_path));

  SessionService* session_service = new SessionService(path_);
  helper_.set_service(session_service);
  ScopedVector<SessionWindow> windows;
  helper_.ReadWindows(&(windows.get()));
  ASSERT_EQ(1u, windows.size());
  EXPECT_EQ(1u, windows[0]->tabs.size());
}
#endif
