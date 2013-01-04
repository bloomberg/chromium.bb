// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/avatar_menu_button.h"

#include "base/path_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/avatar_menu_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"

namespace {

void CreateTestingProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  EXPECT_EQ(1u, profile_manager->GetNumberOfProfiles());

  FilePath path;
  PathService::Get(chrome::DIR_USER_DATA, &path);
  path = path.AppendASCII("test_profile");
  if (!file_util::PathExists(path))
    CHECK(file_util::CreateDirectory(path));
  Profile* profile =
      Profile::CreateProfile(path, NULL, Profile::CREATE_MODE_SYNCHRONOUS);
  profile_manager->RegisterTestingProfile(profile, true);

  EXPECT_EQ(2u, profile_manager->GetNumberOfProfiles());
}

typedef InProcessBrowserTest AvatarMenuButtonTest;

IN_PROC_BROWSER_TEST_F(AvatarMenuButtonTest, HideOnSecondClick) {
  if (!ProfileManager::IsMultipleProfilesEnabled())
    return;

  CreateTestingProfile();

  BrowserView* browser_view = reinterpret_cast<BrowserView*>(
      browser()->window());
  AvatarMenuButton* button = browser_view->frame()->GetAvatarMenuButton();
  ASSERT_TRUE(button);

  // Verify that clicking once shows the avatar bubble.
  static_cast<views::MenuButtonListener*>(button)->OnMenuButtonClicked(
      NULL, gfx::Point());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(AvatarMenuBubbleView::IsShowing());

  // Verify that clicking again doesn't reshow it.
  static_cast<views::MenuButtonListener*>(button)->OnMenuButtonClicked(
      NULL, gfx::Point());
  // Hide the bubble manually. In the browser this would normally happen during
  // the event processing.
  AvatarMenuBubbleView::Hide();
  MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(AvatarMenuBubbleView::IsShowing());
}

}  // namespace
