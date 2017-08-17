// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller_test.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::WebContents;
using ui::PAGE_TRANSITION_TYPED;

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest, MouseLockOnFileURL) {
  static const base::FilePath::CharType* kEmptyFile =
      FILE_PATH_LITERAL("empty.html");
  GURL file_url(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kEmptyFile)));
  AddTabAtIndex(0, file_url, PAGE_TRANSITION_TYPED);
  RequestToLockMouse(true, false);
  ASSERT_TRUE(IsFullscreenBubbleDisplayed());
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest, FullscreenOnFileURL) {
  static const base::FilePath::CharType* kEmptyFile =
      FILE_PATH_LITERAL("empty.html");
  GURL file_url(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kEmptyFile)));
  AddTabAtIndex(0, file_url, PAGE_TRANSITION_TYPED);
  GetFullscreenController()->EnterFullscreenModeForTab(
      browser()->tab_strip_model()->GetActiveWebContents(),
      file_url.GetOrigin());
  ASSERT_TRUE(IsFullscreenBubbleDisplayed());
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       MouseLockBubbleHideCallbackReject) {
  SetWebContentsGrantedSilentMouseLockPermission();
  mouse_lock_bubble_hide_reason_recorder_.clear();
  RequestToLockMouse(false, false);

  EXPECT_EQ(0ul, mouse_lock_bubble_hide_reason_recorder_.size());
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       MouseLockBubbleHideCallbackSilentLock) {
  SetWebContentsGrantedSilentMouseLockPermission();
  mouse_lock_bubble_hide_reason_recorder_.clear();
  RequestToLockMouse(false, true);

  EXPECT_EQ(1ul, mouse_lock_bubble_hide_reason_recorder_.size());
  EXPECT_EQ(ExclusiveAccessBubbleHideReason::kNotShown,
            mouse_lock_bubble_hide_reason_recorder_[0]);
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       MouseLockBubbleHideCallbackUnlock) {
  SetWebContentsGrantedSilentMouseLockPermission();
  mouse_lock_bubble_hide_reason_recorder_.clear();
  RequestToLockMouse(true, false);
  EXPECT_EQ(0ul, mouse_lock_bubble_hide_reason_recorder_.size());

  LostMouseLock();
  EXPECT_EQ(1ul, mouse_lock_bubble_hide_reason_recorder_.size());
  EXPECT_EQ(ExclusiveAccessBubbleHideReason::kInterrupted,
            mouse_lock_bubble_hide_reason_recorder_[0]);
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       MouseLockBubbleHideCallbackLockThenFullscreen) {
  SetWebContentsGrantedSilentMouseLockPermission();
  mouse_lock_bubble_hide_reason_recorder_.clear();
  RequestToLockMouse(true, false);
  EXPECT_EQ(0ul, mouse_lock_bubble_hide_reason_recorder_.size());

  EnterActiveTabFullscreen();
  EXPECT_EQ(1ul, mouse_lock_bubble_hide_reason_recorder_.size());
  EXPECT_EQ(ExclusiveAccessBubbleHideReason::kInterrupted,
            mouse_lock_bubble_hide_reason_recorder_[0]);
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       MouseLockBubbleHideCallbackTimeout) {
  SetWebContentsGrantedSilentMouseLockPermission();
  base::ScopedMockTimeMessageLoopTaskRunner mock_time_task_runner;

  mouse_lock_bubble_hide_reason_recorder_.clear();
  RequestToLockMouse(true, false);
  EXPECT_EQ(0ul, mouse_lock_bubble_hide_reason_recorder_.size());

  EXPECT_TRUE(mock_time_task_runner->HasPendingTask());
  // Must fast forward at least |ExclusiveAccessBubble::kInitialDelayMs|.
  mock_time_task_runner->FastForwardBy(
      base::TimeDelta::FromMilliseconds(InitialBubbleDelayMs() + 20));
  EXPECT_EQ(1ul, mouse_lock_bubble_hide_reason_recorder_.size());
  EXPECT_EQ(ExclusiveAccessBubbleHideReason::kTimeout,
            mouse_lock_bubble_hide_reason_recorder_[0]);
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest, FastMouseLockUnlockRelock) {
  base::ScopedMockTimeMessageLoopTaskRunner mock_time_task_runner;

  RequestToLockMouse(true, false);
  // Shorter than |ExclusiveAccessBubble::kInitialDelayMs|.
  mock_time_task_runner->FastForwardBy(
      base::TimeDelta::FromMilliseconds(InitialBubbleDelayMs() / 2));
  LostMouseLock();
  RequestToLockMouse(true, true);

  EXPECT_TRUE(
      GetExclusiveAccessManager()->mouse_lock_controller()->IsMouseLocked());
  EXPECT_FALSE(GetExclusiveAccessManager()
                   ->mouse_lock_controller()
                   ->IsMouseLockedSilently());
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest, SlowMouseLockUnlockRelock) {
  base::ScopedMockTimeMessageLoopTaskRunner mock_time_task_runner;

  RequestToLockMouse(true, false);
  // Longer than |ExclusiveAccessBubble::kInitialDelayMs|.
  mock_time_task_runner->FastForwardBy(
      base::TimeDelta::FromMilliseconds(InitialBubbleDelayMs() + 20));
  LostMouseLock();
  RequestToLockMouse(true, true);

  EXPECT_TRUE(
      GetExclusiveAccessManager()->mouse_lock_controller()->IsMouseLocked());
  EXPECT_TRUE(GetExclusiveAccessManager()
                  ->mouse_lock_controller()
                  ->IsMouseLockedSilently());
}
