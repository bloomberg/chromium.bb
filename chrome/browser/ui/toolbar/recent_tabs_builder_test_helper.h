// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_RECENT_TABS_BUILDER_TEST_HELPER_H_
#define CHROME_BROWSER_UI_TOOLBAR_RECENT_TABS_BUILDER_TEST_HELPER_H_

#include <vector>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/sessions/session_id.h"

namespace browser_sync {
class OpenTabsUIDelegate;
class SessionsSyncManager;
}
namespace sync_pb {
class SessionSpecifics;
}

// Utility class to help add recent tabs for testing.
class RecentTabsBuilderTestHelper {
 public:
  RecentTabsBuilderTestHelper();
  ~RecentTabsBuilderTestHelper();

  void AddSession();
  int GetSessionCount();
  SessionID::id_type GetSessionID(int session_index);
  base::Time GetSessionTimestamp(int session_index);

  void AddWindow(int session_index);
  int GetWindowCount(int session_index);
  SessionID::id_type GetWindowID(int session_index, int window_index);

  void AddTab(int session_index, int window_index);
  void AddTabWithInfo(int session_index,
                      int window_index,
                      base::Time timestamp,
                      const base::string16& title);
  int GetTabCount(int session_index, int window_index);
  SessionID::id_type GetTabID(int session_index,
                              int window_index,
                              int tab_index);
  base::Time GetTabTimestamp(int session_index,
                             int window_index,
                             int tab_index);
  base::string16 GetTabTitle(int session_index,
                       int window_index,
                       int tab_index);

  void ExportToSessionsSyncManager(
      browser_sync::SessionsSyncManager* manager);

  std::vector<base::string16> GetTabTitlesSortedByRecency();

 private:
  void BuildSessionSpecifics(int session_index,
                             sync_pb::SessionSpecifics* meta);
  void BuildWindowSpecifics(int session_index,
                            int window_index,
                            sync_pb::SessionSpecifics* meta);
  void BuildTabSpecifics(int session_index,
                         int window_index,
                         int tab_index,
                         sync_pb::SessionSpecifics* tab_base);
  void VerifyExport(browser_sync::OpenTabsUIDelegate* delegate);

  struct TabInfo;
  struct WindowInfo;
  struct SessionInfo;

  std::vector<SessionInfo> sessions_;
  base::Time start_time_;

  int max_tab_node_id_;

  DISALLOW_COPY_AND_ASSIGN(RecentTabsBuilderTestHelper);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_RECENT_TABS_BUILDER_TEST_HELPER_H_
