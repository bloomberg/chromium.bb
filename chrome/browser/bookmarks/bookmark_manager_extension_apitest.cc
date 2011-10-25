// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_manager_extension_api.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, BookmarkManager) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("bookmark_manager/standard")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, BookmarkManagerEditDisabled) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  Profile* profile = browser()->profile();

  // Provide some testing data here, since bookmark editing will be disabled
  // within the extension.
  BookmarkModel* model = profile->GetBookmarkModel();
  ui_test_utils::WaitForBookmarkModelToLoad(model);
  const BookmarkNode* bar = model->bookmark_bar_node();
  const BookmarkNode* folder = model->AddFolder(bar, 0, ASCIIToUTF16("Folder"));
  model->AddURL(bar, 1, ASCIIToUTF16("AAA"), GURL("http://aaa.example.com"));
  model->AddURL(folder, 0, ASCIIToUTF16("BBB"), GURL("http://bbb.example.com"));

  profile->GetPrefs()->SetBoolean(prefs::kEditBookmarksEnabled, false);

  ASSERT_TRUE(RunExtensionTest("bookmark_manager/edit_disabled")) << message_;
}
