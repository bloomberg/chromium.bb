// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/avatar_menu_model.h"

#include "base/path_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/test_utils.h"

namespace {

typedef InProcessBrowserTest AvatarMenuModelTest;

IN_PROC_BROWSER_TEST_F(AvatarMenuModelTest, SignOut) {
  if (!ProfileManager::IsMultipleProfilesEnabled())
    return;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* current_profile = browser()->profile();
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  size_t index = cache.GetIndexOfProfileWithPath(current_profile->GetPath());

  AvatarMenuModel model(&cache, NULL, browser());

  BrowserList* browser_list =
      BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_NATIVE);
  EXPECT_EQ(1U, browser_list->size());
  content::WindowedNotificationObserver window_close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(browser()));

  EXPECT_FALSE(cache.ProfileIsSigninRequiredAtIndex(index));
  model.BeginSignOut("about:blank");
  EXPECT_TRUE(cache.ProfileIsSigninRequiredAtIndex(index));

  window_close_observer.Wait();  // rely on test time-out for failure indication
  EXPECT_EQ(0U, browser_list->size());
}

}  // namespace
