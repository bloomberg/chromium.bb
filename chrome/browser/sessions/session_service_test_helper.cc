// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service_test_helper.h"

#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/sessions/session_backend.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_types.h"
#include "components/sessions/serialized_navigation_entry_test_helper.h"
#include "components/sessions/session_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;

SessionServiceTestHelper::SessionServiceTestHelper() {}

SessionServiceTestHelper::SessionServiceTestHelper(SessionService* service)
    : service_(service) {}

SessionServiceTestHelper::~SessionServiceTestHelper() {}

void SessionServiceTestHelper::PrepareTabInWindow(const SessionID& window_id,
                                                  const SessionID& tab_id,
                                                  int visual_index,
                                                  bool select) {
  service()->SetTabWindow(window_id, tab_id);
  service()->SetTabIndexInWindow(window_id, tab_id, visual_index);
  if (select)
    service()->SetSelectedTabInWindow(window_id, visual_index);
}

void SessionServiceTestHelper::SetTabExtensionAppID(
    const SessionID& window_id,
    const SessionID& tab_id,
    const std::string& extension_app_id) {
  service()->SetTabExtensionAppID(window_id, tab_id, extension_app_id);
}

void SessionServiceTestHelper::SetTabUserAgentOverride(
    const SessionID& window_id,
    const SessionID& tab_id,
    const std::string& user_agent_override) {
  service()->SetTabUserAgentOverride(window_id, tab_id, user_agent_override);
}

void SessionServiceTestHelper::SetForceBrowserNotAliveWithNoWindows(
    bool force_browser_not_alive_with_no_windows) {
  service()->force_browser_not_alive_with_no_windows_ =
      force_browser_not_alive_with_no_windows;
}

// Be sure and null out service to force closing the file.
void SessionServiceTestHelper::ReadWindows(
    std::vector<SessionWindow*>* windows,
    SessionID::id_type* active_window_id) {
  Time last_time;
  ScopedVector<SessionCommand> read_commands;
  backend()->ReadLastSessionCommandsImpl(&(read_commands.get()));
  service()->RestoreSessionFromCommands(
      read_commands.get(), windows, active_window_id);
}

void SessionServiceTestHelper::AssertTabEquals(const SessionID& window_id,
                                               const SessionID& tab_id,
                                               int visual_index,
                                               int nav_index,
                                               size_t nav_count,
                                               const SessionTab& session_tab) {
  EXPECT_EQ(window_id.id(), session_tab.window_id.id());
  EXPECT_EQ(tab_id.id(), session_tab.tab_id.id());
  AssertTabEquals(visual_index, nav_index, nav_count, session_tab);
}

void SessionServiceTestHelper::AssertTabEquals(
    int visual_index,
    int nav_index,
    size_t nav_count,
    const SessionTab& session_tab) {
  EXPECT_EQ(visual_index, session_tab.tab_visual_index);
  EXPECT_EQ(nav_index, session_tab.current_navigation_index);
  ASSERT_EQ(nav_count, session_tab.navigations.size());
}

// TODO(sky): nuke this and change to call directly into
// SerializedNavigationEntryTestHelper.
void SessionServiceTestHelper::AssertNavigationEquals(
    const sessions::SerializedNavigationEntry& expected,
    const sessions::SerializedNavigationEntry& actual) {
  sessions::SerializedNavigationEntryTestHelper::ExpectNavigationEquals(
      expected, actual);
}

void SessionServiceTestHelper::AssertSingleWindowWithSingleTab(
    const std::vector<SessionWindow*>& windows,
    size_t nav_count) {
  ASSERT_EQ(1U, windows.size());
  EXPECT_EQ(1U, windows[0]->tabs.size());
  EXPECT_EQ(nav_count, windows[0]->tabs[0]->navigations.size());
}

SessionBackend* SessionServiceTestHelper::backend() {
  return service_->backend();
}

void SessionServiceTestHelper::SetService(SessionService* service) {
  service_.reset(service);
  // Execute IO tasks posted by the SessionService.
  content::RunAllBlockingPoolTasksUntilIdle();
}

void SessionServiceTestHelper::RunTaskOnBackendThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  service_->RunTaskOnBackendThread(from_here, task);
}
