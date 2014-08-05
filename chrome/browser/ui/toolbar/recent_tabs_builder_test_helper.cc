// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/recent_tabs_builder_test_helper.h"

#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sync/open_tabs_ui_delegate.h"
#include "chrome/browser/sync/sessions/sessions_sync_manager.h"
#include "sync/api/attachments/attachment_id.h"
#include "sync/internal_api/public/attachments/attachment_service_proxy_for_test.h"
#include "sync/protocol/session_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kBaseSessionTag[] = "session_tag";
const char kBaseSessionName[] = "session_name";
const char kBaseTabUrl[] = "http://foo/?";
const char kTabTitleFormat[] = "session=%d;window=%d;tab=%d";

struct TitleTimestampPair {
  base::string16 title;
  base::Time timestamp;
};

bool SortTabTimesByRecency(const TitleTimestampPair& t1,
                           const TitleTimestampPair& t2) {
  return t1.timestamp > t2.timestamp;
}

int CreateUniqueID() {
  static int s_id = 0;
  ++s_id;
  return s_id;
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

}  // namespace

struct RecentTabsBuilderTestHelper::TabInfo {
  TabInfo() : id(0) {}
  SessionID::id_type id;
  base::Time timestamp;
  base::string16 title;
};
struct RecentTabsBuilderTestHelper::WindowInfo {
  WindowInfo() : id(0) {}
  ~WindowInfo() {}
  SessionID::id_type id;
  std::vector<TabInfo> tabs;
};
struct RecentTabsBuilderTestHelper::SessionInfo {
  SessionInfo() : id(0) {}
  ~SessionInfo() {}
  SessionID::id_type id;
  std::vector<WindowInfo> windows;
};

RecentTabsBuilderTestHelper::RecentTabsBuilderTestHelper()
    : max_tab_node_id_(0) {
  start_time_ = base::Time::Now();
}

RecentTabsBuilderTestHelper::~RecentTabsBuilderTestHelper() {
}

void RecentTabsBuilderTestHelper::AddSession() {
  SessionInfo info;
  info.id = CreateUniqueID();
  sessions_.push_back(info);
}

int RecentTabsBuilderTestHelper::GetSessionCount() {
  return sessions_.size();
}

SessionID::id_type RecentTabsBuilderTestHelper::GetSessionID(
    int session_index) {
  return sessions_[session_index].id;
}

base::Time RecentTabsBuilderTestHelper::GetSessionTimestamp(int session_index) {
  std::vector<base::Time> timestamps;
  for (int w = 0; w < GetWindowCount(session_index); ++w) {
    for (int t = 0; t < GetTabCount(session_index, w); ++t)
      timestamps.push_back(GetTabTimestamp(session_index, w, t));
  }

  if (timestamps.empty())
    return base::Time::Now();

  sort(timestamps.begin(), timestamps.end());
  return timestamps[0];
}

void RecentTabsBuilderTestHelper::AddWindow(int session_index) {
  WindowInfo window_info;
  window_info.id = CreateUniqueID();
  sessions_[session_index].windows.push_back(window_info);
}

int RecentTabsBuilderTestHelper::GetWindowCount(int session_index) {
  return sessions_[session_index].windows.size();
}

SessionID::id_type RecentTabsBuilderTestHelper::GetWindowID(int session_index,
                                                            int window_index) {
  return sessions_[session_index].windows[window_index].id;
}

void RecentTabsBuilderTestHelper::AddTab(int session_index, int window_index) {
  base::Time timestamp =
      start_time_ + base::TimeDelta::FromMinutes(base::RandUint64());
  AddTabWithInfo(session_index, window_index, timestamp, base::string16());
}

void RecentTabsBuilderTestHelper::AddTabWithInfo(int session_index,
                                                 int window_index,
                                                 base::Time timestamp,
                                                 const base::string16& title) {
  TabInfo tab_info;
  tab_info.id = CreateUniqueID();
  tab_info.timestamp = timestamp;
  tab_info.title = title;
  sessions_[session_index].windows[window_index].tabs.push_back(tab_info);
}

int RecentTabsBuilderTestHelper::GetTabCount(int session_index,
                                             int window_index) {
  return sessions_[session_index].windows[window_index].tabs.size();
}

SessionID::id_type RecentTabsBuilderTestHelper::GetTabID(int session_index,
                                                         int window_index,
                                                         int tab_index) {
  return sessions_[session_index].windows[window_index].tabs[tab_index].id;
}

base::Time RecentTabsBuilderTestHelper::GetTabTimestamp(int session_index,
                                                        int window_index,
                                                        int tab_index) {
  return sessions_[session_index].windows[window_index]
      .tabs[tab_index].timestamp;
}

base::string16 RecentTabsBuilderTestHelper::GetTabTitle(int session_index,
                                                        int window_index,
                                                        int tab_index) {
  base::string16 title =
      sessions_[session_index].windows[window_index].tabs[tab_index].title;
  if (title.empty()) {
    title = base::UTF8ToUTF16(ToTabTitle(
        GetSessionID(session_index),
        GetWindowID(session_index, window_index),
        GetTabID(session_index, window_index, tab_index)));
  }
  return title;
}

void RecentTabsBuilderTestHelper::ExportToSessionsSyncManager(
    browser_sync::SessionsSyncManager* manager) {
  syncer::SyncChangeList changes;
  for (int s = 0; s < GetSessionCount(); ++s) {
    sync_pb::EntitySpecifics session_entity;
    sync_pb::SessionSpecifics* meta = session_entity.mutable_session();
    BuildSessionSpecifics(s, meta);
    for (int w = 0; w < GetWindowCount(s); ++w) {
      BuildWindowSpecifics(s, w, meta);
      for (int t = 0; t < GetTabCount(s, w); ++t) {
        sync_pb::EntitySpecifics entity;
        sync_pb::SessionSpecifics* tab_base = entity.mutable_session();
        BuildTabSpecifics(s, w, t, tab_base);
        changes.push_back(syncer::SyncChange(
            FROM_HERE,
            syncer::SyncChange::ACTION_ADD,
            syncer::SyncData::CreateRemoteData(
                tab_base->tab_node_id(),
                entity,
                GetTabTimestamp(s, w, t),
                syncer::AttachmentIdList(),
                syncer::AttachmentServiceProxyForTest::Create())));
      }
    }
    changes.push_back(syncer::SyncChange(
        FROM_HERE,
        syncer::SyncChange::ACTION_ADD,
        syncer::SyncData::CreateRemoteData(
            1,
            session_entity,
            GetSessionTimestamp(s),
            syncer::AttachmentIdList(),
            syncer::AttachmentServiceProxyForTest::Create())));
  }
  manager->ProcessSyncChanges(FROM_HERE, changes);
  VerifyExport(manager);
}

void RecentTabsBuilderTestHelper::VerifyExport(
    browser_sync::OpenTabsUIDelegate* delegate) {
  // Make sure data is populated correctly in SessionModelAssociator.
  std::vector<const browser_sync::SyncedSession*> sessions;
  ASSERT_TRUE(delegate->GetAllForeignSessions(&sessions));
  ASSERT_EQ(GetSessionCount(), static_cast<int>(sessions.size()));
  for (int s = 0; s < GetSessionCount(); ++s) {
    std::vector<const SessionWindow*> windows;
    ASSERT_TRUE(delegate->GetForeignSession(ToSessionTag(GetSessionID(s)),
                                            &windows));
    ASSERT_EQ(GetWindowCount(s), static_cast<int>(windows.size()));
    for (int w = 0; w < GetWindowCount(s); ++w)
      ASSERT_EQ(GetTabCount(s, w), static_cast<int>(windows[w]->tabs.size()));
  }
}

std::vector<base::string16>
RecentTabsBuilderTestHelper::GetTabTitlesSortedByRecency() {
  std::vector<TitleTimestampPair> tabs;
  for (int s = 0; s < GetSessionCount(); ++s) {
    for (int w = 0; w < GetWindowCount(s); ++w) {
      for (int t = 0; t < GetTabCount(s, w); ++t) {
        TitleTimestampPair pair;
        pair.title = GetTabTitle(s, w, t);
        pair.timestamp = GetTabTimestamp(s, w, t);
        tabs.push_back(pair);
      }
    }
  }
  sort(tabs.begin(), tabs.end(), SortTabTimesByRecency);

  std::vector<base::string16> titles;
  for (size_t i = 0; i < tabs.size(); ++i)
    titles.push_back(tabs[i].title);
  return titles;
}

void RecentTabsBuilderTestHelper::BuildSessionSpecifics(
    int session_index,
    sync_pb::SessionSpecifics* meta) {
  SessionID::id_type session_id = GetSessionID(session_index);
  meta->set_session_tag(ToSessionTag(session_id));
  sync_pb::SessionHeader* header = meta->mutable_header();
  header->set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_CROS);
  header->set_client_name(ToSessionName(session_id));
}

void RecentTabsBuilderTestHelper::BuildWindowSpecifics(
    int session_index,
    int window_index,
    sync_pb::SessionSpecifics* meta) {
  sync_pb::SessionHeader* header = meta->mutable_header();
  sync_pb::SessionWindow* window = header->add_window();
  SessionID::id_type window_id = GetWindowID(session_index, window_index);
  window->set_window_id(window_id);
  window->set_selected_tab_index(0);
  window->set_browser_type(sync_pb::SessionWindow_BrowserType_TYPE_TABBED);
  for (int i = 0; i < GetTabCount(session_index, window_index); ++i)
    window->add_tab(GetTabID(session_index, window_index, i));
}

void RecentTabsBuilderTestHelper::BuildTabSpecifics(
    int session_index,
    int window_index,
    int tab_index,
    sync_pb::SessionSpecifics* tab_base) {
  SessionID::id_type session_id = GetSessionID(session_index);
  SessionID::id_type window_id = GetWindowID(session_index, window_index);
  SessionID::id_type tab_id = GetTabID(session_index, window_index, tab_index);

  tab_base->set_session_tag(ToSessionTag(session_id));
  tab_base->set_tab_node_id(++max_tab_node_id_);
  sync_pb::SessionTab* tab = tab_base->mutable_tab();
  tab->set_window_id(window_id);
  tab->set_tab_id(tab_id);
  tab->set_tab_visual_index(1);
  tab->set_current_navigation_index(0);
  tab->set_pinned(true);
  tab->set_extension_app_id("app_id");
  sync_pb::TabNavigation* navigation = tab->add_navigation();
  navigation->set_virtual_url(ToTabUrl(session_id, window_id, tab_id));
  navigation->set_referrer("referrer");
  navigation->set_title(base::UTF16ToUTF8(GetTabTitle(
      session_index, window_index, tab_index)));
  navigation->set_page_transition(sync_pb::SyncEnums_PageTransition_TYPED);
}
