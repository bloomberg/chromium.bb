// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Bookmarks) {
  // Add test managed bookmarks to verify that the bookmarks API can read them
  // and can't modify them.
  Profile* profile = browser()->profile();
  ChromeBookmarkClient* client =
      BookmarkModelFactory::GetChromeBookmarkClientForProfile(profile);
  BookmarkModel* model = client->model();
  test::WaitForBookmarkModelToLoad(model);

  base::ListValue list;
  base::DictionaryValue* node = new base::DictionaryValue();
  node->SetString("name", "Managed Bookmark");
  node->SetString("url", "http://www.chromium.org");
  list.Append(node);
  node = new base::DictionaryValue();
  node->SetString("name", "Managed Folder");
  node->Set("children", new base::ListValue());
  list.Append(node);
  profile->GetPrefs()->Set(prefs::kManagedBookmarks, list);
  ASSERT_EQ(2, client->managed_node()->child_count());

  ASSERT_TRUE(RunExtensionTest("bookmarks")) << message_;
}
