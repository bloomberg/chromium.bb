// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jamescook): Why does this test run on all Aura platforms, instead of
// only Chrome OS or Ash?
#if defined(USE_AURA)

#include "components/sessions/core/persistent_tab_restore_service.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef sessions::TabRestoreService::Window Window;

typedef InProcessBrowserTest PersistentTabRestoreServiceBrowserTest;

IN_PROC_BROWSER_TEST_F(PersistentTabRestoreServiceBrowserTest, RestoreApp) {
  Profile* profile = browser()->profile();
  sessions::TabRestoreService* trs =
      TabRestoreServiceFactory::GetForProfile(profile);
  const char* app_name = "TestApp";

  Browser* app_browser = CreateBrowserForApp(app_name, profile);
  app_browser->window()->Close();
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(app_browser));
  observer.Wait();

  // One entry should be created.
  ASSERT_EQ(1U, trs->entries().size());
  const sessions::TabRestoreService::Entry* restored_entry =
      trs->entries().front().get();

  // It should be a window with an app.
  ASSERT_EQ(sessions::TabRestoreService::WINDOW, restored_entry->type);
  const Window* restored_window = static_cast<const Window*>(restored_entry);
  EXPECT_EQ(app_name, restored_window->app_name);
}

#endif  // defined(USE_AURA)
