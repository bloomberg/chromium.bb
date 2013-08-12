// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_SERVICE_TEST_HELPER_H_
#define CHROME_BROWSER_SESSIONS_SESSION_SERVICE_TEST_HELPER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sessions/session_id.h"

class SessionBackend;
class SessionCommand;
class SessionService;
struct SessionTab;
struct SessionWindow;

namespace sessions {
class SerializedNavigationEntry;
}

// A simple class that makes writing SessionService related tests easier.

class SessionServiceTestHelper {
 public:
  explicit SessionServiceTestHelper();
  explicit SessionServiceTestHelper(SessionService* service);
  ~SessionServiceTestHelper();

  void PrepareTabInWindow(const SessionID& window_id,
                          const SessionID& tab_id,
                          int visual_index,
                          bool select);

  void SetTabExtensionAppID(const SessionID& window_id,
                            const SessionID& tab_id,
                            const std::string& extension_app_id);

  void SetTabUserAgentOverride(const SessionID& window_id,
                               const SessionID& tab_id,
                               const std::string& user_agent_override);

  void SetForceBrowserNotAliveWithNoWindows(
      bool force_browser_not_alive_with_no_windows);

  // Reads the contents of the last session.
  void ReadWindows(std::vector<SessionWindow*>* windows,
                   SessionID::id_type* active_window_id);

  void AssertTabEquals(SessionID& window_id,
                       SessionID& tab_id,
                       int visual_index,
                       int nav_index,
                       size_t nav_count,
                       const SessionTab& session_tab);

  void AssertTabEquals(int visual_index,
                       int nav_index,
                       size_t nav_count,
                       const SessionTab& session_tab);

  void AssertNavigationEquals(
      const sessions::SerializedNavigationEntry& expected,
      const sessions::SerializedNavigationEntry& actual);

  void AssertSingleWindowWithSingleTab(
      const std::vector<SessionWindow*>& windows,
      size_t nav_count);

  void SetService(SessionService* service);
  SessionService* ReleaseService() { return service_.release(); }
  SessionService* service() { return service_.get(); }

  SessionBackend* backend();

 private:
  scoped_ptr<SessionService> service_;

  DISALLOW_COPY_AND_ASSIGN(SessionServiceTestHelper);
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_SERVICE_TEST_HELPER_H_
