// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "extensions/common/manifest_constants.h"

typedef ExtensionApiTest BookmarkOverrideTest;

namespace {
// Bookmark this page keybinding.
#if defined(OS_MACOSX)
const char kBookmarkKeybinding[] = "Command+D";
#else
const char kBookmarkKeybinding[] = "Ctrl+D";
#endif  // defined(OS_MACOSX)
}

// Test that invoking the IDC_BOOKMARK_PAGE command (as done by the wrench menu)
// brings up the bookmark UI, if no extension requests to override ctrl-D and
// the user has assigned it to an extension.
IN_PROC_BROWSER_TEST_F(BookmarkOverrideTest, NonOverrideBookmarkPage) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("keybinding/basics")) << message_;
  const extensions::Extension* extension = GetSingleLoadedExtension();

  extensions::CommandService* command_service =
      extensions::CommandService::Get(browser()->profile());

  // Simulate the user setting the keybinding to Ctrl+D.
  command_service->UpdateKeybindingPrefs(
      extension->id(), extensions::manifest_values::kBrowserActionCommandEvent,
      kBookmarkKeybinding);

  // Check that the BookmarkBubbleView is shown when executing
  // IDC_BOOKMARK_PAGE.
  EXPECT_FALSE(BookmarkBubbleView::IsShowing());
  chrome::ExecuteCommand(browser(), IDC_BOOKMARK_PAGE);
  EXPECT_TRUE(BookmarkBubbleView::IsShowing());
}
