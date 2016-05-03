// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/memory/scoped_vector.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sessions/core/session_command.h"
#include "components/sessions/core/session_types.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/page_state.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::NavigationEntry;
using sessions::SerializedNavigationEntry;
using sessions::SerializedNavigationEntryTestHelper;

class SessionServiceTest : public BrowserWithTestWindowTest,
                           public content::NotificationObserver {
 public:
  SessionServiceTest() : window_bounds(0, 1, 2, 3), sync_save_count_(0) {}

 protected:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());

    std::string b = base::Int64ToString(base::Time::Now().ToInternalValue());
    TestingProfile* profile = profile_manager_->CreateTestingProfile(b);
    SessionService* session_service = new SessionService(profile);
    path_ = profile->GetPath();

    helper_.SetService(session_service);

    service()->SetWindowType(window_id,
                             Browser::TYPE_TABBED,
                             SessionService::TYPE_NORMAL);
    service()->SetWindowBounds(window_id,
                               window_bounds,
                               ui::SHOW_STATE_NORMAL);
    service()->SetWindowWorkspace(window_id, window_workspace);
  }

  // Upon notification, increment the sync_save_count variable
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    ASSERT_EQ(type, chrome::NOTIFICATION_SESSION_SERVICE_SAVED);
    sync_save_count_++;
  }

  void TearDown() override {
    helper_.SetService(NULL);
    BrowserWithTestWindowTest::TearDown();
  }

  void UpdateNavigation(
      const SessionID& window_id,
      const SessionID& tab_id,
      const SerializedNavigationEntry& navigation,
      bool select) {
    service()->UpdateTabNavigation(window_id, tab_id, navigation);
    if (select) {
      service()->SetSelectedNavigationIndex(
          window_id, tab_id, navigation.index());
    }
  }

  void ReadWindows(std::vector<sessions::SessionWindow*>* windows,
                   SessionID::id_type* active_window_id) {
    // Forces closing the file.
    helper_.SetService(NULL);

    SessionService* session_service = new SessionService(path_);
    helper_.SetService(session_service);

    SessionID::id_type* non_null_active_window_id = active_window_id;
    SessionID::id_type dummy_active_window_id = 0;
    if (!non_null_active_window_id)
      non_null_active_window_id = &dummy_active_window_id;
    helper_.ReadWindows(windows, non_null_active_window_id);
  }

  // Configures the session service with one window with one tab and a single
  // navigation. If |pinned_state| is true or |write_always| is true, the
  // pinned state of the tab is updated. The session service is then recreated
  // and the pinned state of the read back tab is returned.
  bool CreateAndWriteSessionWithOneTab(bool pinned_state, bool write_always) {
    SessionID tab_id;
    SerializedNavigationEntry nav1 =
        SerializedNavigationEntryTestHelper::CreateNavigation(
            "http://google.com", "abc");

    helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
    UpdateNavigation(window_id, tab_id, nav1, true);

    if (pinned_state || write_always)
      helper_.service()->SetPinnedState(window_id, tab_id, pinned_state);

    ScopedVector<sessions::SessionWindow> windows;
    ReadWindows(&(windows.get()), NULL);

    EXPECT_EQ(1U, windows.size());
    if (HasFatalFailure())
      return false;
    EXPECT_EQ(1U, windows[0]->tabs.size());
    if (HasFatalFailure())
      return false;

    sessions::SessionTab* tab = windows[0]->tabs[0];
    helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);

    return tab->pinned;
  }

  void CreateAndWriteSessionWithTwoWindows(
      const SessionID& window2_id,
      const SessionID& tab1_id,
      const SessionID& tab2_id,
      SerializedNavigationEntry* nav1,
      SerializedNavigationEntry* nav2) {
    *nav1 = SerializedNavigationEntryTestHelper::CreateNavigation(
        "http://google.com", "abc");
    *nav2 = SerializedNavigationEntryTestHelper::CreateNavigation(
        "http://google2.com", "abcd");

    helper_.PrepareTabInWindow(window_id, tab1_id, 0, true);
    UpdateNavigation(window_id, tab1_id, *nav1, true);

    const gfx::Rect window2_bounds(3, 4, 5, 6);
    service()->SetWindowType(window2_id,
                             Browser::TYPE_TABBED,
                             SessionService::TYPE_NORMAL);
    service()->SetWindowBounds(window2_id,
                               window2_bounds,
                               ui::SHOW_STATE_MAXIMIZED);
    helper_.PrepareTabInWindow(window2_id, tab2_id, 0, true);
    UpdateNavigation(window2_id, tab2_id, *nav2, true);
  }

  SessionService* service() { return helper_.service(); }

  const gfx::Rect window_bounds;

  const std::string window_workspace = "abc";

  SessionID window_id;

  int sync_save_count_;

  // Path used in testing.
  base::ScopedTempDir temp_dir_;
  base::FilePath path_;

  SessionServiceTestHelper helper_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
};

TEST_F(SessionServiceTest, Basic) {
  SessionID tab_id;
  ASSERT_NE(window_id.id(), tab_id.id());

  SerializedNavigationEntry nav1 =
      SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://google.com", "abc");
  SerializedNavigationEntryTestHelper::SetOriginalRequestURL(
      GURL("http://original.request.com"), &nav1);

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);

  ScopedVector<sessions::SessionWindow> windows;
  ReadWindows(&(windows.get()), NULL);

  ASSERT_EQ(1U, windows.size());
  ASSERT_TRUE(window_bounds == windows[0]->bounds);
  ASSERT_EQ(window_workspace, windows[0]->workspace);
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(window_id.id(), windows[0]->window_id.id());
  ASSERT_EQ(1U, windows[0]->tabs.size());
  ASSERT_EQ(sessions::SessionWindow::TYPE_TABBED, windows[0]->type);

  sessions::SessionTab* tab = windows[0]->tabs[0];
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);

  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
}

// Make sure we persist post entries.
TEST_F(SessionServiceTest, PersistPostData) {
  SessionID tab_id;
  ASSERT_NE(window_id.id(), tab_id.id());

  SerializedNavigationEntry nav1 =
      SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://google.com", "abc");
  SerializedNavigationEntryTestHelper::SetHasPostData(true, &nav1);

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);

  ScopedVector<sessions::SessionWindow> windows;
  ReadWindows(&(windows.get()), NULL);

  helper_.AssertSingleWindowWithSingleTab(windows.get(), 1);
}

TEST_F(SessionServiceTest, ClosingTabStaysClosed) {
  SessionID tab_id;
  SessionID tab2_id;
  ASSERT_NE(tab_id.id(), tab2_id.id());

  SerializedNavigationEntry nav1 =
      SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://google.com", "abc");
  SerializedNavigationEntry nav2 =
      SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://google2.com", "abcd");

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);

  helper_.PrepareTabInWindow(window_id, tab2_id, 1, false);
  UpdateNavigation(window_id, tab2_id, nav2, true);
  service()->TabClosed(window_id, tab2_id, false);

  ScopedVector<sessions::SessionWindow> windows;
  ReadWindows(&(windows.get()), NULL);

  ASSERT_EQ(1U, windows.size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(window_id.id(), windows[0]->window_id.id());
  ASSERT_EQ(1U, windows[0]->tabs.size());

  sessions::SessionTab* tab = windows[0]->tabs[0];
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);

  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
}

TEST_F(SessionServiceTest, Pruning) {
  SessionID tab_id;

  SerializedNavigationEntry nav1 =
      SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://google.com", "abc");
  SerializedNavigationEntry nav2 =
      SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://google2.com", "abcd");

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  for (int i = 0; i < 6; ++i) {
    SerializedNavigationEntry* nav = (i % 2) == 0 ? &nav1 : &nav2;
    nav->set_index(i);
    UpdateNavigation(window_id, tab_id, *nav, true);
  }
  service()->TabNavigationPathPrunedFromBack(window_id, tab_id, 3);

  ScopedVector<sessions::SessionWindow> windows;
  ReadWindows(&(windows.get()), NULL);

  ASSERT_EQ(1U, windows.size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(1U, windows[0]->tabs.size());

  sessions::SessionTab* tab = windows[0]->tabs[0];
  // We left the selected index at 5, then pruned. When rereading the
  // index should get reset to last valid navigation, which is 2.
  helper_.AssertTabEquals(window_id, tab_id, 0, 2, 3, *tab);

  ASSERT_EQ(3u, tab->navigations.size());
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
  helper_.AssertNavigationEquals(nav2, tab->navigations[1]);
  helper_.AssertNavigationEquals(nav1, tab->navigations[2]);
}

TEST_F(SessionServiceTest, TwoWindows) {
  SessionID window2_id;
  SessionID tab1_id;
  SessionID tab2_id;
  SerializedNavigationEntry nav1;
  SerializedNavigationEntry nav2;

  CreateAndWriteSessionWithTwoWindows(
      window2_id, tab1_id, tab2_id, &nav1, &nav2);

  ScopedVector<sessions::SessionWindow> windows;
  ReadWindows(&(windows.get()), NULL);

  ASSERT_EQ(2U, windows.size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(0, windows[1]->selected_tab_index);
  ASSERT_EQ(1U, windows[0]->tabs.size());
  ASSERT_EQ(1U, windows[1]->tabs.size());

  sessions::SessionTab* rt1;
  sessions::SessionTab* rt2;
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
  sessions::SessionTab* tab = rt1;
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

  SerializedNavigationEntry nav1 =
      SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://google.com", "abc");

  helper_.PrepareTabInWindow(window_id, tab1_id, 0, true);
  UpdateNavigation(window_id, tab1_id, nav1, true);

  const gfx::Rect window2_bounds(3, 4, 5, 6);
  service()->SetWindowType(window2_id,
                           Browser::TYPE_TABBED,
                           SessionService::TYPE_NORMAL);
  service()->SetWindowBounds(window2_id,
                             window2_bounds,
                             ui::SHOW_STATE_NORMAL);
  helper_.PrepareTabInWindow(window2_id, tab2_id, 0, true);

  ScopedVector<sessions::SessionWindow> windows;
  ReadWindows(&(windows.get()), NULL);

  ASSERT_EQ(1U, windows.size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(1U, windows[0]->tabs.size());
  ASSERT_EQ(window_id.id(), windows[0]->window_id.id());

  sessions::SessionTab* tab = windows[0]->tabs[0];
  helper_.AssertTabEquals(window_id, tab1_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
}

TEST_F(SessionServiceTest, ClosingWindowDoesntCloseTabs) {
  SessionID tab_id;
  SessionID tab2_id;
  ASSERT_NE(tab_id.id(), tab2_id.id());

  SerializedNavigationEntry nav1 =
      SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://google.com", "abc");
  SerializedNavigationEntry nav2 =
      SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://google2.com", "abcd");

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);

  helper_.PrepareTabInWindow(window_id, tab2_id, 1, false);
  UpdateNavigation(window_id, tab2_id, nav2, true);

  service()->WindowClosing(window_id);

  ScopedVector<sessions::SessionWindow> windows;
  ReadWindows(&(windows.get()), NULL);

  ASSERT_EQ(1U, windows.size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(window_id.id(), windows[0]->window_id.id());
  ASSERT_EQ(2U, windows[0]->tabs.size());

  sessions::SessionTab* tab = windows[0]->tabs[0];
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);

  tab = windows[0]->tabs[1];
  helper_.AssertTabEquals(window_id, tab2_id, 1, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav2, tab->navigations[0]);
}

TEST_F(SessionServiceTest, LockingWindowRemembersAll) {
  SessionID window2_id;
  SessionID tab1_id;
  SessionID tab2_id;
  SerializedNavigationEntry nav1;
  SerializedNavigationEntry nav2;

  CreateAndWriteSessionWithTwoWindows(
      window2_id, tab1_id, tab2_id, &nav1, &nav2);

  ASSERT_TRUE(service()->profile());
  ProfileManager* manager = g_browser_process->profile_manager();
  ASSERT_TRUE(manager);
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(manager->GetProfileAttributesStorage().
      GetProfileAttributesWithPath(service()->profile()->GetPath(), &entry));
  entry->SetIsSigninRequired(true);

  service()->WindowClosing(window_id);
  service()->WindowClosed(window_id);
  service()->WindowClosing(window2_id);
  service()->WindowClosed(window2_id);

  ScopedVector<sessions::SessionWindow> windows;
  ReadWindows(&(windows.get()), NULL);

  ASSERT_EQ(2U, windows.size());
  ASSERT_EQ(1U, windows[0]->tabs.size());
  ASSERT_EQ(1U, windows[1]->tabs.size());
}

TEST_F(SessionServiceTest, WindowCloseCommittedAfterNavigate) {
  SessionID window2_id;
  SessionID tab_id;
  SessionID tab2_id;
  ASSERT_NE(window2_id.id(), window_id.id());

  service()->SetWindowType(window2_id,
                           Browser::TYPE_TABBED,
                           SessionService::TYPE_NORMAL);
  service()->SetWindowBounds(window2_id,
                             window_bounds,
                             ui::SHOW_STATE_NORMAL);

  SerializedNavigationEntry nav1 =
      SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://google.com", "abc");
  SerializedNavigationEntry nav2 =
      SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://google2.com", "abcd");

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);

  helper_.PrepareTabInWindow(window2_id, tab2_id, 0, false);
  UpdateNavigation(window2_id, tab2_id, nav2, true);

  service()->WindowClosing(window2_id);
  service()->TabClosed(window2_id, tab2_id, false);
  service()->WindowClosed(window2_id);

  ScopedVector<sessions::SessionWindow> windows;
  ReadWindows(&(windows.get()), NULL);

  ASSERT_EQ(1U, windows.size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(window_id.id(), windows[0]->window_id.id());
  ASSERT_EQ(1U, windows[0]->tabs.size());

  sessions::SessionTab* tab = windows[0]->tabs[0];
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
}

// Makes sure we don't track popups.
TEST_F(SessionServiceTest, IgnorePopups) {
  SessionID window2_id;
  SessionID tab_id;
  SessionID tab2_id;
  ASSERT_NE(window2_id.id(), window_id.id());

  service()->SetWindowType(window2_id,
                           Browser::TYPE_POPUP,
                           SessionService::TYPE_NORMAL);
  service()->SetWindowBounds(window2_id,
                             window_bounds,
                             ui::SHOW_STATE_NORMAL);
  service()->SetWindowWorkspace(window2_id, window_workspace);

  SerializedNavigationEntry nav1 =
      SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://google.com", "abc");
  SerializedNavigationEntry nav2 =
      SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://google2.com", "abcd");

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);

  helper_.PrepareTabInWindow(window2_id, tab2_id, 0, false);
  UpdateNavigation(window2_id, tab2_id, nav2, true);

  ScopedVector<sessions::SessionWindow> windows;
  ReadWindows(&(windows.get()), NULL);

  ASSERT_EQ(1U, windows.size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(window_id.id(), windows[0]->window_id.id());
  ASSERT_EQ(1U, windows[0]->tabs.size());

  sessions::SessionTab* tab = windows[0]->tabs[0];
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
}

TEST_F(SessionServiceTest, RemoveUnusedRestoreWindowsTest) {
  ScopedVector<sessions::SessionWindow> windows_list;
  windows_list.push_back(new sessions::SessionWindow());
  windows_list.back()->type = sessions::SessionWindow::TYPE_TABBED;
  windows_list.push_back(new sessions::SessionWindow());
  windows_list.back()->type = sessions::SessionWindow::TYPE_POPUP;

  service()->RemoveUnusedRestoreWindows(&(windows_list.get()));
  ASSERT_EQ(1U, windows_list.size());
  EXPECT_EQ(sessions::SessionWindow::TYPE_TABBED, windows_list[0]->type);
}

#if defined (OS_CHROMEOS)
// Makes sure we track apps. Only applicable on chromeos.
TEST_F(SessionServiceTest, RestoreApp) {
  SessionID window2_id;
  SessionID tab_id;
  SessionID tab2_id;
  ASSERT_NE(window2_id.id(), window_id.id());

  service()->SetWindowType(window2_id,
                           Browser::TYPE_POPUP,
                           SessionService::TYPE_APP);
  service()->SetWindowBounds(window2_id,
                             window_bounds,
                             ui::SHOW_STATE_NORMAL);
  service()->SetWindowAppName(window2_id, "TestApp");

  SerializedNavigationEntry nav1 =
      SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://google.com", "abc");
  SerializedNavigationEntry nav2 =
      SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://google2.com", "abcd");

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);

  helper_.PrepareTabInWindow(window2_id, tab2_id, 0, false);
  UpdateNavigation(window2_id, tab2_id, nav2, true);

  ScopedVector<sessions::SessionWindow> windows;
  ReadWindows(&(windows.get()), NULL);

  ASSERT_EQ(2U, windows.size());
  int tabbed_index = windows[0]->type == sessions::SessionWindow::TYPE_TABBED ?
      0 : 1;
  int app_index = tabbed_index == 0 ? 1 : 0;
  ASSERT_EQ(0, windows[tabbed_index]->selected_tab_index);
  ASSERT_EQ(window_id.id(), windows[tabbed_index]->window_id.id());
  ASSERT_EQ(1U, windows[tabbed_index]->tabs.size());

  sessions::SessionTab* tab = windows[tabbed_index]->tabs[0];
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);

  ASSERT_EQ(0, windows[app_index]->selected_tab_index);
  ASSERT_EQ(window2_id.id(), windows[app_index]->window_id.id());
  ASSERT_EQ(1U, windows[app_index]->tabs.size());
  ASSERT_TRUE(windows[app_index]->type == sessions::SessionWindow::TYPE_POPUP);
  ASSERT_EQ("TestApp", windows[app_index]->app_name);

  tab = windows[app_index]->tabs[0];
  helper_.AssertTabEquals(window2_id, tab2_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav2, tab->navigations[0]);
}
#endif  // defined (OS_CHROMEOS)

// Tests pruning from the front.
TEST_F(SessionServiceTest, PruneFromFront) {
  const std::string base_url("http://google.com/");
  SessionID tab_id;

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);

  // Add 5 navigations, with the 4th selected.
  for (int i = 0; i < 5; ++i) {
    SerializedNavigationEntry nav =
        SerializedNavigationEntryTestHelper::CreateNavigation(
            base_url + base::IntToString(i), "a");
    nav.set_index(i);
    UpdateNavigation(window_id, tab_id, nav, (i == 3));
  }

  // Prune the first two navigations from the front.
  helper_.service()->TabNavigationPathPrunedFromFront(window_id, tab_id, 2);

  // Read back in.
  ScopedVector<sessions::SessionWindow> windows;
  ReadWindows(&(windows.get()), NULL);

  ASSERT_EQ(1U, windows.size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(window_id.id(), windows[0]->window_id.id());
  ASSERT_EQ(1U, windows[0]->tabs.size());

  // There shouldn't be an app id.
  EXPECT_TRUE(windows[0]->tabs[0]->extension_app_id.empty());

  // We should be left with three navigations, the 2nd selected.
  sessions::SessionTab* tab = windows[0]->tabs[0];
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
    SerializedNavigationEntry nav =
        SerializedNavigationEntryTestHelper::CreateNavigation(
            base_url + base::IntToString(i), "a");
    nav.set_index(i);
    UpdateNavigation(window_id, tab_id, nav, (i == 3));
  }

  // Prune the first two navigations from the front.
  helper_.service()->TabNavigationPathPrunedFromFront(window_id, tab_id, 5);

  // Read back in.
  ScopedVector<sessions::SessionWindow> windows;
  ReadWindows(&(windows.get()), NULL);

  ASSERT_EQ(0U, windows.size());
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

  SerializedNavigationEntry nav1 =
      SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://google.com", "abc");

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);
  helper_.SetTabExtensionAppID(window_id, tab_id, app_id);

  ScopedVector<sessions::SessionWindow> windows;
  ReadWindows(&(windows.get()), NULL);

  helper_.AssertSingleWindowWithSingleTab(windows.get(), 1);
  EXPECT_TRUE(app_id == windows[0]->tabs[0]->extension_app_id);
}

// Check that user agent overrides are persisted.
TEST_F(SessionServiceTest, PersistUserAgentOverrides) {
  SessionID tab_id;
  ASSERT_NE(window_id.id(), tab_id.id());
  std::string user_agent_override = "Mozilla/5.0 (X11; Linux x86_64) "
      "AppleWebKit/535.19 (KHTML, like Gecko) Chrome/18.0.1025.45 "
      "Safari/535.19";

  SerializedNavigationEntry nav1 =
      SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://google.com", "abc");
  SerializedNavigationEntryTestHelper::SetIsOverridingUserAgent(true, &nav1);

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);
  helper_.SetTabUserAgentOverride(window_id, tab_id, user_agent_override);

  ScopedVector<sessions::SessionWindow> windows;
  ReadWindows(&(windows.get()), NULL);
  helper_.AssertSingleWindowWithSingleTab(windows.get(), 1);

  sessions::SessionTab* tab = windows[0]->tabs[0];
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
  EXPECT_TRUE(user_agent_override == tab->user_agent_override);
}

// Test that the notification for SESSION_SERVICE_SAVED is working properly.
TEST_F(SessionServiceTest, SavedSessionNotification) {
  content::NotificationRegistrar registrar_;
  registrar_.Add(this, chrome::NOTIFICATION_SESSION_SERVICE_SAVED,
                 content::NotificationService::AllSources());
  service()->GetBaseSessionServiceForTest()->Save();
  EXPECT_EQ(sync_save_count_, 1);
}

// Makes sure a tab closed by a user gesture is not restored.
TEST_F(SessionServiceTest, CloseTabUserGesture) {
  SessionID tab_id;
  ASSERT_NE(window_id.id(), tab_id.id());

  SerializedNavigationEntry nav1 =
      SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://google.com", "abc");

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);
  service()->TabClosed(window_id, tab_id, true);

  ScopedVector<sessions::SessionWindow> windows;
  ReadWindows(&(windows.get()), NULL);

  ASSERT_TRUE(windows.empty());
}

// Verifies SetWindowBounds maps SHOW_STATE_DEFAULT to SHOW_STATE_NORMAL.
TEST_F(SessionServiceTest, DontPersistDefault) {
  SessionID tab_id;
  ASSERT_NE(window_id.id(), tab_id.id());
  SerializedNavigationEntry nav1 =
      SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://google.com", "abc");
  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);
  service()->SetWindowBounds(window_id,
                             window_bounds,
                             ui::SHOW_STATE_DEFAULT);

  ScopedVector<sessions::SessionWindow> windows;
  ReadWindows(&(windows.get()), NULL);
  ASSERT_EQ(1U, windows.size());
  EXPECT_EQ(ui::SHOW_STATE_NORMAL, windows[0]->show_state);
}

TEST_F(SessionServiceTest, KeepPostDataWithoutPasswords) {
  SessionID tab_id;
  ASSERT_NE(window_id.id(), tab_id.id());

  // Create a page state representing a HTTP body without posted passwords.
  content::PageState page_state =
      content::PageState::CreateForTesting(GURL(), false, "data", NULL);

  // Create a TabNavigation containing page_state and representing a POST
  // request.
  SerializedNavigationEntry nav1 =
      SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://google.com", "title");
  SerializedNavigationEntryTestHelper::SetEncodedPageState(
      page_state.ToEncodedData(), &nav1);
  SerializedNavigationEntryTestHelper::SetHasPostData(true, &nav1);

  // Create a TabNavigation containing page_state and representing a normal
  // request.
  SerializedNavigationEntry nav2 =
      SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://google.com/nopost", "title");
  SerializedNavigationEntryTestHelper::SetEncodedPageState(
      page_state.ToEncodedData(), &nav2);
  nav2.set_index(1);

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);
  UpdateNavigation(window_id, tab_id, nav2, true);

  ScopedVector<sessions::SessionWindow> windows;
  ReadWindows(&(windows.get()), NULL);

  helper_.AssertSingleWindowWithSingleTab(windows.get(), 2);

  // Expected: the page state of both navigations was saved and restored.
  ASSERT_EQ(2u, windows[0]->tabs[0]->navigations.size());
  helper_.AssertNavigationEquals(nav1, windows[0]->tabs[0]->navigations[0]);
  helper_.AssertNavigationEquals(nav2, windows[0]->tabs[0]->navigations[1]);
}

TEST_F(SessionServiceTest, RemovePostDataWithPasswords) {
  SessionID tab_id;
  ASSERT_NE(window_id.id(), tab_id.id());

  // Create a page state representing a HTTP body with posted passwords.
  content::PageState page_state =
      content::PageState::CreateForTesting(GURL(), true, "data", NULL);

  // Create a TabNavigation containing page_state and representing a POST
  // request with passwords.
  SerializedNavigationEntry nav1 =
      SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://google.com", "title");
  SerializedNavigationEntryTestHelper::SetEncodedPageState(
      page_state.ToEncodedData(), &nav1);
  SerializedNavigationEntryTestHelper::SetHasPostData(true, &nav1);
  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);

  ScopedVector<sessions::SessionWindow> windows;
  ReadWindows(&(windows.get()), NULL);

  helper_.AssertSingleWindowWithSingleTab(windows.get(), 1);

  // Expected: the HTTP body was removed from the page state of the POST
  // navigation with passwords.
  EXPECT_NE(page_state.ToEncodedData(),
            windows[0]->tabs[0]->navigations[0].encoded_page_state());
}

TEST_F(SessionServiceTest, ReplacePendingNavigation) {
  const std::string base_url("http://google.com/");
  SessionID tab_id;

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);

  // Add 5 navigations, some with the same index
  for (int i = 0; i < 5; ++i) {
    SerializedNavigationEntry nav =
        SerializedNavigationEntryTestHelper::CreateNavigation(
            base_url + base::IntToString(i), "a");
    nav.set_index(i / 2);
    UpdateNavigation(window_id, tab_id, nav, true);
  }

  // Read back in.
  ScopedVector<sessions::SessionWindow> windows;
  ReadWindows(&(windows.get()), NULL);

  // The ones with index 0, and 2 should have been replaced by 1 and 3.
  ASSERT_EQ(1U, windows.size());
  ASSERT_EQ(1U, windows[0]->tabs.size());
  EXPECT_EQ(3U, windows[0]->tabs[0]->navigations.size());
  EXPECT_EQ(GURL(base_url + base::IntToString(1)),
            windows[0]->tabs[0]->navigations[0].virtual_url());
  EXPECT_EQ(GURL(base_url + base::IntToString(3)),
            windows[0]->tabs[0]->navigations[1].virtual_url());
  EXPECT_EQ(GURL(base_url + base::IntToString(4)),
            windows[0]->tabs[0]->navigations[2].virtual_url());
}

TEST_F(SessionServiceTest, ReplacePendingNavigationAndPrune) {
  const std::string base_url("http://google.com/");
  SessionID tab_id;

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);

  for (int i = 0; i < 5; ++i) {
    SerializedNavigationEntry nav =
        SerializedNavigationEntryTestHelper::CreateNavigation(
            base_url + base::IntToString(i), "a");
    nav.set_index(i);
    UpdateNavigation(window_id, tab_id, nav, true);
  }

  // Prune all those navigations.
  helper_.service()->TabNavigationPathPrunedFromFront(window_id, tab_id, 5);

  // Add another navigation to replace the last one.
  SerializedNavigationEntry nav =
      SerializedNavigationEntryTestHelper::CreateNavigation(
        base_url + base::IntToString(5), "a");
  nav.set_index(4);
  UpdateNavigation(window_id, tab_id, nav, true);

  // Read back in.
  ScopedVector<sessions::SessionWindow> windows;
  ReadWindows(&(windows.get()), NULL);

  // We should still have that last navigation at the end,
  // even though it replaced one that was set before the prune.
  ASSERT_EQ(1U, windows.size());
  ASSERT_EQ(1U, windows[0]->tabs.size());
  ASSERT_EQ(1U, windows[0]->tabs[0]->navigations.size());
  EXPECT_EQ(GURL(base_url + base::IntToString(5)),
            windows[0]->tabs[0]->navigations[0].virtual_url());
}

TEST_F(SessionServiceTest, RestoreActivation1) {
  SessionID window2_id;
  SessionID tab1_id;
  SessionID tab2_id;
  SerializedNavigationEntry nav1;
  SerializedNavigationEntry nav2;

  CreateAndWriteSessionWithTwoWindows(
      window2_id, tab1_id, tab2_id, &nav1, &nav2);

  service()->ScheduleCommand(
      sessions::CreateSetActiveWindowCommand(window2_id));
  service()->ScheduleCommand(sessions::CreateSetActiveWindowCommand(window_id));

  ScopedVector<sessions::SessionWindow> windows;
  SessionID::id_type active_window_id = 0;
  ReadWindows(&(windows.get()), &active_window_id);
  EXPECT_EQ(window_id.id(), active_window_id);
}

// It's easier to have two separate tests with setup/teardown than to manualy
// reset the state for the different flavors of the test.
TEST_F(SessionServiceTest, RestoreActivation2) {
  SessionID window2_id;
  SessionID tab1_id;
  SessionID tab2_id;
  SerializedNavigationEntry nav1;
  SerializedNavigationEntry nav2;

  CreateAndWriteSessionWithTwoWindows(
      window2_id, tab1_id, tab2_id, &nav1, &nav2);

  service()->ScheduleCommand(
      sessions::CreateSetActiveWindowCommand(window2_id));
  service()->ScheduleCommand(sessions::CreateSetActiveWindowCommand(window_id));
  service()->ScheduleCommand(
      sessions::CreateSetActiveWindowCommand(window2_id));

  ScopedVector<sessions::SessionWindow> windows;
  SessionID::id_type active_window_id = 0;
  ReadWindows(&(windows.get()), &active_window_id);
  EXPECT_EQ(window2_id.id(), active_window_id);
}

// Makes sure we don't track blacklisted URLs.
TEST_F(SessionServiceTest, IgnoreBlacklistedUrls) {
  SessionID tab_id;

  SerializedNavigationEntry nav1 =
      SerializedNavigationEntryTestHelper::CreateNavigation(
          "http://google.com", "abc");
  SerializedNavigationEntry nav2 =
      SerializedNavigationEntryTestHelper::CreateNavigation(
          chrome::kChromeUIQuitURL, "quit");
  SerializedNavigationEntry nav3 =
      SerializedNavigationEntryTestHelper::CreateNavigation(
          chrome::kChromeUIRestartURL, "restart");

  helper_.PrepareTabInWindow(window_id, tab_id, 0, true);
  UpdateNavigation(window_id, tab_id, nav1, true);
  UpdateNavigation(window_id, tab_id, nav2, true);
  UpdateNavigation(window_id, tab_id, nav3, true);

  ScopedVector<sessions::SessionWindow> windows;
  ReadWindows(&(windows.get()), NULL);

  ASSERT_EQ(1U, windows.size());
  ASSERT_EQ(0, windows[0]->selected_tab_index);
  ASSERT_EQ(window_id.id(), windows[0]->window_id.id());
  ASSERT_EQ(1U, windows[0]->tabs.size());

  sessions::SessionTab* tab = windows[0]->tabs[0];
  helper_.AssertTabEquals(window_id, tab_id, 0, 0, 1, *tab);
  helper_.AssertNavigationEquals(nav1, tab->navigations[0]);
}

// Functions used by GetSessionsAndDestroy.
namespace {

void OnGotPreviousSession(ScopedVector<sessions::SessionWindow> windows,
                          SessionID::id_type ignored_active_window) {
  FAIL() << "SessionService was destroyed, this shouldn't be reached.";
}

void PostBackToThread(base::MessageLoop* message_loop,
                      base::RunLoop* run_loop) {
  message_loop->task_runner()->PostTask(
      FROM_HERE, base::Bind(&base::RunLoop::Quit, base::Unretained(run_loop)));
}

}  // namespace

// Verifies that SessionService::GetLastSession() works correctly if the
// SessionService is deleted during processing. To verify the problematic case
// does the following:
// 1. Sends a task to the background thread that blocks.
// 2. Asks SessionService for the last session commands. This is blocked by 1.
// 3. Posts another task to the background thread, this too is blocked by 1.
// 4. Deletes SessionService.
// 5. Signals the semaphore that 2 and 3 are waiting on, allowing
//    GetLastSession() to continue.
// 6. runs the message loop, this is quit when the task scheduled in 3 posts
//    back to the ui thread to quit the run loop.
// The call to get the previous session should never be invoked because the
// SessionService was destroyed before SessionService could process the results.
TEST_F(SessionServiceTest, GetSessionsAndDestroy) {
  base::CancelableTaskTracker cancelable_task_tracker;
  base::RunLoop run_loop;
  base::WaitableEvent event(true, false);
  helper_.RunTaskOnBackendThread(FROM_HERE,
                                 base::Bind(&base::WaitableEvent::Wait,
                                            base::Unretained(&event)));
  service()->GetLastSession(base::Bind(&OnGotPreviousSession),
                            &cancelable_task_tracker);
  helper_.RunTaskOnBackendThread(
      FROM_HERE,
      base::Bind(&PostBackToThread,
                 base::Unretained(base::MessageLoop::current()),
                 base::Unretained(&run_loop)));
  delete helper_.ReleaseService();
  event.Signal();
  run_loop.Run();
}
