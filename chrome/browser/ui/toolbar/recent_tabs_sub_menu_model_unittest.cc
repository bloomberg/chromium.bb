// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/recent_tabs_sub_menu_model.h"

#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sessions/session_types_test_helper.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/glue/synced_session.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/menu_model_test.h"
#include "chrome/test/base/testing_profile.h"
#include "grit/generated_resources.h"
#include "sync/protocol/session_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kBaseSessionTag[] = "session_tag";
const char kBaseSessionName[] = "session_name";
const char kBaseTabUrl[] = "http://foo/?";
const char kTabTitleFormat[] = "session=%d;window=%d;tab=%d";

struct TabTime {
  TabTime(SessionID::id_type session_id,
          SessionID::id_type window_id,
          SessionID::id_type tab_id,
          const base::Time& timestamp)
      : session_id(session_id),
        window_id(window_id),
        tab_id(tab_id),
        timestamp(timestamp) {}

  SessionID::id_type session_id;
  SessionID::id_type window_id;
  SessionID::id_type tab_id;
  base::Time timestamp;
};

bool SortTabTimesByRecency(const TabTime& t1, const TabTime& t2) {
  return t1.timestamp > t2.timestamp;
}

std::string ToSessionTag(SessionID::id_type session_id) {
  return std::string(kBaseSessionTag + base::IntToString(session_id));
}

std::string ToSessionName(SessionID::id_type session_id) {
  return std::string(kBaseSessionName + base::IntToString(session_id));
}

std::string ToTabTitle(SessionID::id_type session_id,
                       SessionID::id_type window_id,
                       SessionID::id_type tab_id) {
  return base::StringPrintf(kTabTitleFormat, session_id, window_id, tab_id);
}

std::string ToTabUrl(SessionID::id_type session_id,
                     SessionID::id_type window_id,
                     SessionID::id_type tab_id) {
  return std::string(kBaseTabUrl + ToTabTitle(session_id, window_id, tab_id));
}

void BuildSessionSpecifics(SessionID::id_type session_id,
                           sync_pb::SessionSpecifics* meta) {
  meta->set_session_tag(ToSessionTag(session_id));
  sync_pb::SessionHeader* header = meta->mutable_header();
  header->set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_CROS);
  header->set_client_name(ToSessionName(session_id));
}

void AddWindowSpecifics(int window_id, int num_tabs, int* tab_id,
                        std::vector<SessionID::id_type>* tab_ids,
                        sync_pb::SessionSpecifics* meta) {
  sync_pb::SessionHeader* header = meta->mutable_header();
  sync_pb::SessionWindow* window = header->add_window();
  window->set_window_id(window_id);
  window->set_selected_tab_index(0);
  window->set_browser_type(sync_pb::SessionWindow_BrowserType_TYPE_TABBED);
  for (int i = 0; i < num_tabs; ++i) {
    SessionID::id_type id = (*tab_id)++;
    window->add_tab(id);
    tab_ids->push_back(id);
  }
}

void BuildTabsSpecifics(SessionID::id_type session_id,
                        SessionID::id_type window_id,
                        const std::vector<SessionID::id_type>& tab_ids,
                        std::vector<sync_pb::SessionSpecifics>* tab_bases) {
  tab_bases->resize(tab_ids.size());
  for (size_t i = 0; i < tab_ids.size(); ++i) {
    SessionID::id_type tab_id = tab_ids[i];
    sync_pb::SessionSpecifics& tab_base = (*tab_bases)[i];
    tab_base.set_session_tag(ToSessionTag(session_id));
    sync_pb::SessionTab* tab = tab_base.mutable_tab();
    tab->set_window_id(window_id);
    tab->set_tab_id(tab_id);
    tab->set_tab_visual_index(1);
    tab->set_current_navigation_index(0);
    tab->set_pinned(true);
    tab->set_extension_app_id("app_id");
    sync_pb::TabNavigation* navigation = tab->add_navigation();
    navigation->set_virtual_url(ToTabUrl(session_id, window_id, tab_id));
    navigation->set_referrer("referrer");
    navigation->set_title(ToTabTitle(session_id, window_id, tab_id));
    navigation->set_page_transition(sync_pb::SyncEnums_PageTransition_TYPED);
  }
}

// This copies parts of MenuModelTest::Delegate and combines them with the
// RecentTabsSubMenuModel since RecentTabsSubMenuModel is a
// SimpleMenuModel::Delegate and not just derived from SimpleMenuModel.
class TestRecentTabsSubMenuModel : public RecentTabsSubMenuModel {
 public:
  TestRecentTabsSubMenuModel(ui::AcceleratorProvider* provider,
                             Browser* browser,
                             browser_sync::SessionModelAssociator* associator,
                             bool can_restore_tab)
      : RecentTabsSubMenuModel(provider, browser, associator),
        can_restore_tab_(can_restore_tab),
        execute_count_(0),
        enable_count_(0) {
  }

  // Testing overrides to ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE {
    bool val = command_id == IDC_RESTORE_TAB ? can_restore_tab_ :
        RecentTabsSubMenuModel::IsCommandIdEnabled(command_id);
    if (val)
      ++enable_count_;
    return val;
  }

  virtual void ExecuteCommand(int command_id) OVERRIDE {
    ++execute_count_;
  }

  bool can_restore_tab_;
  int execute_count_;
  int mutable enable_count_;  // Mutable because IsCommandIdEnabledAt is const.
};

}  // namespace

class RecentTabsSubMenuModelTest : public BrowserWithTestWindowTest,
                                   public ui::AcceleratorProvider {
 public:
   RecentTabsSubMenuModelTest()
       : sync_service_(&testing_profile_),
         associator_(&sync_service_, true) {
    associator_.SetCurrentMachineTagForTesting("RecentTabsSubMenuModelTest");
  }

  // Don't handle accelerators.
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE {
    return false;
  }

 private:
  TestingProfile testing_profile_;
  testing::NiceMock<ProfileSyncServiceMock> sync_service_;

 protected:
  browser_sync::SessionModelAssociator associator_;
};

// Test disabled "Reopen closed tab" with no foreign tabs.
TEST_F(RecentTabsSubMenuModelTest, NoTabs) {
  TestRecentTabsSubMenuModel model(this, browser(), NULL, false);

  // Expected menu:
  // Menu index  Menu items
  // --------------------------------------
  // 0           Reopen closed tab
  // 1           <separator>
  // 2           No tabs from other Devices

  int num_items = model.GetItemCount();
  EXPECT_EQ(3, num_items);
  EXPECT_FALSE(model.IsEnabledAt(0));
  EXPECT_FALSE(model.IsEnabledAt(2));
  EXPECT_EQ(0, model.enable_count_);
}

// Test enabled "Reopen closed tab" with no foreign tabs.
TEST_F(RecentTabsSubMenuModelTest, ReopenClosedTab) {
  TestRecentTabsSubMenuModel model(this, browser(), NULL, true);
  // Menu contents are identical to NoTabs test.
  int num_items = model.GetItemCount();
  EXPECT_EQ(3, num_items);
  EXPECT_TRUE(model.IsEnabledAt(0));
  model.ActivatedAt(0);
  EXPECT_FALSE(model.IsEnabledAt(2));
  EXPECT_EQ(1, model.enable_count_);
  EXPECT_EQ(1, model.execute_count_);
}

// Test enabled "Reopen closed tab" with multiple sessions, multiple windows,
// and multiple enabled tabs from other devices.
TEST_F(RecentTabsSubMenuModelTest, OtherDevices) {
  SessionID::id_type id = 0;
  std::vector<SessionID::id_type> session_ids;
  // Tabs are populated in decreasing timestamp.
  base::Time now = base::Time::Now();

  // Create 1st session : 1 window, 3 tabs
  SessionID::id_type session_id = id++;
  session_ids.push_back(session_id);
  sync_pb::SessionSpecifics meta1;
  BuildSessionSpecifics(session_id, &meta1);
  SessionID::id_type window_id = id++;
  std::vector<SessionID::id_type> tab_ids;
  AddWindowSpecifics(window_id, 3, &id, &tab_ids, &meta1);
  std::vector<sync_pb::SessionSpecifics> tabs;
  BuildTabsSpecifics(session_id, window_id, tab_ids, &tabs);
  base::Time timestamp = now - base::TimeDelta::FromMinutes(window_id * 10);
  // First tab's timestamp is the most recent in session, so use it as session's
  // modified time.
  associator_.AssociateForeignSpecifics(meta1, timestamp);
  std::vector<base::Time> tab_times;
  for (size_t i = 0; i < tabs.size(); ++i)
    tab_times.push_back(timestamp - base::TimeDelta::FromMinutes(i));
  for (size_t i = 0; i < tabs.size(); ++i)
    associator_.AssociateForeignSpecifics(tabs[i], tab_times[i]);

  // Create 2nd session : 2 windows, 1 tab in 1st window, 2 tabs in 2nd window
  session_id = id++;
  session_ids.push_back(session_id);
  sync_pb::SessionSpecifics meta2;
  BuildSessionSpecifics(session_id, &meta2);
  window_id = id++;
  std::vector<SessionID::id_type> tab_ids1;
  AddWindowSpecifics(window_id, 1, &id, &tab_ids1, &meta2);
  std::vector<sync_pb::SessionSpecifics> tabs1;
  BuildTabsSpecifics(session_id, window_id, tab_ids1, &tabs1);
  timestamp = now - base::TimeDelta::FromMinutes(window_id * 10);
  associator_.AssociateForeignSpecifics(meta2, timestamp);
  std::vector<base::Time> tab_times1;
  for (size_t i = 0; i < tabs1.size(); ++i)
    tab_times1.push_back(timestamp - base::TimeDelta::FromMinutes(i));
  for (size_t i = 0; i < tabs1.size(); ++i)
    associator_.AssociateForeignSpecifics(tabs1[i], tab_times1[i]);
  window_id = id++;
  std::vector<SessionID::id_type> tab_ids2;
  AddWindowSpecifics(window_id, 2, &id, &tab_ids2, &meta2);
  std::vector<sync_pb::SessionSpecifics> tabs2;
  BuildTabsSpecifics(session_id, window_id, tab_ids2, &tabs2);
  timestamp = now - base::TimeDelta::FromMinutes(window_id * 10);
  // First tab's timestamp is the most recent in session, so use it as session's
  // modified time.
  associator_.AssociateForeignSpecifics(meta2, timestamp);
  std::vector<base::Time> tab_times2;
  for (size_t i = 0; i < tabs2.size(); ++i)
    tab_times2.push_back(timestamp - base::TimeDelta::FromMinutes(i));
  for (size_t i = 0; i < tabs2.size(); ++i)
    associator_.AssociateForeignSpecifics(tabs2[i], tab_times2[i]);

  // Make sure data is populated correctly in SessionModelAssociator.
  std::vector<const browser_sync::SyncedSession*> sessions;
  ASSERT_TRUE(associator_.GetAllForeignSessions(&sessions));
  ASSERT_EQ(2U, sessions.size());
  std::vector<const SessionWindow*> windows;
  ASSERT_TRUE(associator_.GetForeignSession(ToSessionTag(session_ids[0]),
                                            &windows));
  ASSERT_EQ(1U, windows.size());
  ASSERT_EQ(3U, windows[0]->tabs.size());
  windows.clear();
  ASSERT_TRUE(associator_.GetForeignSession(ToSessionTag(session_ids[1]),
                                            &windows));
  ASSERT_EQ(2U, windows.size());
  ASSERT_EQ(1U, windows[0]->tabs.size());
  ASSERT_EQ(2U, windows[1]->tabs.size());

  // Verify that data is populated correctly in RecentTabsSubMenuModel.
  // Expected menu:
  // - first inserted tab is most recent and hence is top
  // Menu index  Menu items
  // --------------------------------------
  // 0           Reopen closed tab
  // 1           <separator>
  // 2           <section header for 1st session>
  // 3-5         <3 tabs of the only window of session 0>
  // 6           <separator>
  // 7           <section header for 2nd session>
  // 8           <the only tab of window 0 of session 1>
  // 9-10        <2 tabs of window 1 of session 2>

  TestRecentTabsSubMenuModel model(this, browser(), &associator_, true);
  int num_items = model.GetItemCount();
  EXPECT_EQ(11, num_items);
  model.ActivatedAt(0);
  EXPECT_TRUE(model.IsEnabledAt(0));
  model.ActivatedAt(3);
  EXPECT_TRUE(model.IsEnabledAt(3));
  model.ActivatedAt(4);
  EXPECT_TRUE(model.IsEnabledAt(4));
  model.ActivatedAt(5);
  EXPECT_TRUE(model.IsEnabledAt(5));
  model.ActivatedAt(8);
  EXPECT_TRUE(model.IsEnabledAt(8));
  model.ActivatedAt(9);
  EXPECT_TRUE(model.IsEnabledAt(9));
  model.ActivatedAt(10);
  EXPECT_TRUE(model.IsEnabledAt(10));
  EXPECT_EQ(7, model.enable_count_);
  EXPECT_EQ(7, model.execute_count_);
}

TEST_F(RecentTabsSubMenuModelTest, MaxSessionsAndRecency) {
  SessionID::id_type id = 0;
  std::vector<SessionID::id_type> session_ids;
  base::Time now = base::Time::Now();

  // Create 4 sessions : each session has 1 window with 1 tab each.
  std::vector<TabTime> tab_times;
  for (int i = 0; i < 4; ++i) {
    SessionID::id_type session_id = id++;
    session_ids.push_back(session_id);
    sync_pb::SessionSpecifics meta;
    BuildSessionSpecifics(session_id, &meta);
    SessionID::id_type window_id = id++;
    std::vector<SessionID::id_type> tab_ids;
    AddWindowSpecifics(window_id, 1, &id, &tab_ids, &meta);
    std::vector<sync_pb::SessionSpecifics> tabs;
    BuildTabsSpecifics(session_id, window_id, tab_ids, &tabs);
    base::Time timestamp =
        now + base::TimeDelta::FromMinutes(base::RandUint64());
    tab_times.push_back(TabTime(session_id, window_id, tab_ids[0], timestamp));
    // Use tab's timestamp as session's modified time.
    associator_.AssociateForeignSpecifics(meta, timestamp);
    associator_.AssociateForeignSpecifics(tabs[0], timestamp);
  }

  // Make sure data is populated correctly in SessionModelAssociator.
  std::vector<const browser_sync::SyncedSession*> sessions;
  ASSERT_TRUE(associator_.GetAllForeignSessions(&sessions));
  ASSERT_EQ(4U, sessions.size());
  std::vector<const SessionWindow*> windows;
  for (size_t i = 0; i < sessions.size(); ++i) {
    ASSERT_TRUE(associator_.GetForeignSession(ToSessionTag(session_ids[i]),
                                              &windows));
    ASSERT_EQ(1U, windows.size());
    ASSERT_EQ(1U, windows[0]->tabs.size());
  }

  // Verify that data is populated correctly in RecentTabsSubMenuModel.
  // Expected menu:
  // - max sessions is 3, so only 3 most-recent sessions will show
  // Menu index  Menu items
  // --------------------------------------
  // 0           Reopen closed tab
  // 1           <separator>
  // 2           <section header for 1st session>
  // 3           <the only tab of the only window of session 3>
  // 4           <separator>
  // 5           <section header for 2nd session>
  // 6           <the only tab of the only window of session 2>
  // 7           <separator>
  // 8           <section header for 3rd session>
  // 9           <the only tab of the only window of session 1>

  TestRecentTabsSubMenuModel model(this, browser(), &associator_, true);
  int num_items = model.GetItemCount();
  EXPECT_EQ(10, num_items);

  sort(tab_times.begin(), tab_times.end(), SortTabTimesByRecency);
  EXPECT_EQ(UTF8ToUTF16(ToTabTitle(tab_times[0].session_id,
                                   tab_times[0].window_id,
                                   tab_times[0].tab_id)),
            model.GetLabelAt(3));
  EXPECT_EQ(UTF8ToUTF16(ToTabTitle(tab_times[1].session_id,
                                   tab_times[1].window_id,
                                   tab_times[1].tab_id)),
            model.GetLabelAt(6));
  EXPECT_EQ(UTF8ToUTF16(ToTabTitle(tab_times[2].session_id,
                                   tab_times[2].window_id,
                                   tab_times[2].tab_id)),
            model.GetLabelAt(9));
}

TEST_F(RecentTabsSubMenuModelTest, MaxTabsPerSessionAndRecency) {
  SessionID::id_type id = 0;
  base::Time now = base::Time::Now();

  // Create a session: 2 windows with 5 tabs each.
  std::vector<TabTime> tab_times;
  SessionID::id_type session_id = id++;
  sync_pb::SessionSpecifics meta;
  BuildSessionSpecifics(session_id, &meta);
  for (int i = 0; i < 2; ++i) {
    SessionID::id_type window_id = id++;
    std::vector<SessionID::id_type> tab_ids;
    AddWindowSpecifics(window_id, 5, &id, &tab_ids, &meta);
    std::vector<sync_pb::SessionSpecifics> tabs;
    BuildTabsSpecifics(session_id, window_id, tab_ids, &tabs);
    // There's no session recency sorting with only one session, so just use now
    // as session's modified time.
    associator_.AssociateForeignSpecifics(meta, now);
    for (size_t i = 0; i < tabs.size(); ++i) {
      base::Time timestamp = now +
                             base::TimeDelta::FromMinutes(base::RandUint64());
      tab_times.push_back(TabTime(session_id, window_id, tab_ids[i],
                                  timestamp));
      associator_.AssociateForeignSpecifics(tabs[i], timestamp);
    }
  }

  // Make sure data is populated correctly in SessionModelAssociator.
  std::vector<const browser_sync::SyncedSession*> sessions;
  ASSERT_TRUE(associator_.GetAllForeignSessions(&sessions));
  ASSERT_EQ(1U, sessions.size());
  std::vector<const SessionWindow*> windows;
  ASSERT_TRUE(associator_.GetForeignSession(ToSessionTag(session_id),
                                            &windows));
  ASSERT_EQ(2U, windows.size());
  ASSERT_EQ(5U, windows[0]->tabs.size());
  ASSERT_EQ(5U, windows[1]->tabs.size());

  // Verify that data is populated correctly in RecentTabsSubMenuModel.
  // Expected menu:
  // - max tabs per session  is 4, so only 4 most-recent tabs will show,
  //   independent of which window they came from
  // Menu index  Menu items
  // --------------------------------------
  // 0           Reopen closed tab
  // 1           <separator>
  // 2           <section header for session>
  // 3-6         <4 most-recent tabs of session>

  TestRecentTabsSubMenuModel model(this, browser(), &associator_, true);
  int num_items = model.GetItemCount();
  EXPECT_EQ(7, num_items);

  sort(tab_times.begin(), tab_times.end(), SortTabTimesByRecency);
  EXPECT_EQ(UTF8ToUTF16(ToTabTitle(tab_times[0].session_id,
                                   tab_times[0].window_id,
                                   tab_times[0].tab_id)),
            model.GetLabelAt(3));
  EXPECT_EQ(UTF8ToUTF16(ToTabTitle(tab_times[1].session_id,
                                   tab_times[1].window_id,
                                   tab_times[1].tab_id)),
            model.GetLabelAt(4));
  EXPECT_EQ(UTF8ToUTF16(ToTabTitle(tab_times[2].session_id,
                                   tab_times[2].window_id,
                                   tab_times[2].tab_id)),
            model.GetLabelAt(5));
  EXPECT_EQ(UTF8ToUTF16(ToTabTitle(tab_times[3].session_id,
                                   tab_times[3].window_id,
                                   tab_times[3].tab_id)),
            model.GetLabelAt(6));
}
