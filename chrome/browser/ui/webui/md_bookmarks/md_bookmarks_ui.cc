// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/md_bookmarks/md_bookmarks_ui.h"

#include <algorithm>

#include "base/strings/string16.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

void AddLocalizedString(content::WebUIDataSource* source,
                        const std::string& message,
                        int id) {
  base::string16 str = l10n_util::GetStringUTF16(id);
  str.erase(std::remove(str.begin(), str.end(), '&'), str.end());
  source->AddString(message, str);
}

content::WebUIDataSource* CreateMdBookmarksUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIBookmarksHost);

  // Localized strings (alphabetical order).
  AddLocalizedString(source, "cancelEdit", IDS_CANCEL);
  AddLocalizedString(source, "clearSearch",
                     IDS_MD_BOOKMARK_MANAGER_CLEAR_SEARCH);
  AddLocalizedString(source, "editBookmarkTitle", IDS_BOOKMARK_EDITOR_TITLE);
  AddLocalizedString(source, "editDialogNameInput",
                     IDS_BOOKMARK_MANAGER_NAME_INPUT_PLACE_HOLDER);
  AddLocalizedString(source, "editDialogUrlInput",
                             IDS_BOOKMARK_MANAGER_URL_INPUT_PLACE_HOLDER);
  AddLocalizedString(source, "emptyList",
                     IDS_MD_BOOKMARK_MANAGER_EMPTY_LIST);
  AddLocalizedString(source, "menuAddBookmark",
                     IDS_MD_BOOKMARK_MANAGER_MENU_ADD_BOOKMARK);
  AddLocalizedString(source, "menuAddFolder",
                     IDS_MD_BOOKMARK_MANAGER_MENU_ADD_FOLDER);
  AddLocalizedString(source, "menuBulkEdit",
                     IDS_MD_BOOKMARK_MANAGER_MENU_BULK_EDIT);
  AddLocalizedString(source, "menuCopyURL",
                     IDS_MD_BOOKMARK_MANAGER_MENU_COPY_URL);
  AddLocalizedString(source, "menuDelete", IDS_DELETE);
  AddLocalizedString(source, "menuEdit", IDS_EDIT);
  AddLocalizedString(source, "menuExport", IDS_MD_BOOKMARK_MANAGER_MENU_EXPORT);
  AddLocalizedString(source, "menuImport", IDS_MD_BOOKMARK_MANAGER_MENU_IMPORT);
  AddLocalizedString(source, "menuRename", IDS_MD_BOOKMARK_MANAGER_MENU_RENAME);
  AddLocalizedString(source, "menuSort", IDS_MD_BOOKMARK_MANAGER_MENU_SORT);
  AddLocalizedString(source, "noSearchResults",
                     IDS_MD_BOOKMARK_MANAGER_NO_SEARCH_RESULTS);
  AddLocalizedString(source, "renameFolderTitle",
                     IDS_MD_BOOKMARK_MANAGER_FOLDER_RENAME_TITLE);
  AddLocalizedString(source, "searchPrompt",
                     IDS_BOOKMARK_MANAGER_SEARCH_BUTTON);
  AddLocalizedString(source, "saveEdit", IDS_SAVE);
  AddLocalizedString(source, "title", IDS_MD_BOOKMARK_MANAGER_TITLE);

  // Resources.
  source->AddResourcePath("app.html", IDR_MD_BOOKMARKS_APP_HTML);
  source->AddResourcePath("app.js", IDR_MD_BOOKMARKS_APP_JS);
  source->AddResourcePath("folder_node.html",
                          IDR_MD_BOOKMARKS_FOLDER_NODE_HTML);
  source->AddResourcePath("folder_node.js",
                          IDR_MD_BOOKMARKS_FOLDER_NODE_JS);
  source->AddResourcePath("icons.html", IDR_MD_BOOKMARKS_ICONS_HTML);
  source->AddResourcePath("item.html", IDR_MD_BOOKMARKS_ITEM_HTML);
  source->AddResourcePath("item.js", IDR_MD_BOOKMARKS_ITEM_JS);
  source->AddResourcePath("list.html", IDR_MD_BOOKMARKS_LIST_HTML);
  source->AddResourcePath("list.js", IDR_MD_BOOKMARKS_LIST_JS);
  source->AddResourcePath("router.html", IDR_MD_BOOKMARKS_ROUTER_HTML);
  source->AddResourcePath("router.js", IDR_MD_BOOKMARKS_ROUTER_JS);
  source->AddResourcePath("shared_style.html",
                          IDR_MD_BOOKMARKS_SHARED_STYLE_HTML);
  source->AddResourcePath("shared_vars.html",
                          IDR_MD_BOOKMARKS_SHARED_VARS_HTML);
  source->AddResourcePath("sidebar.html", IDR_MD_BOOKMARKS_SIDEBAR_HTML);
  source->AddResourcePath("sidebar.js", IDR_MD_BOOKMARKS_SIDEBAR_JS);
  source->AddResourcePath("store.html", IDR_MD_BOOKMARKS_STORE_HTML);
  source->AddResourcePath("store.js", IDR_MD_BOOKMARKS_STORE_JS);
  source->AddResourcePath("toolbar.html", IDR_MD_BOOKMARKS_TOOLBAR_HTML);
  source->AddResourcePath("toolbar.js", IDR_MD_BOOKMARKS_TOOLBAR_JS);
  source->SetDefaultResource(IDR_MD_BOOKMARKS_BOOKMARKS_HTML);
  source->SetJsonPath("strings.js");

  return source;
}

}  // namespace

MdBookmarksUI::MdBookmarksUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  // Set up the chrome://bookmarks/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile,
                                CreateMdBookmarksUIHTMLSource(profile));
}

// static
bool MdBookmarksUI::IsEnabled() {
  return base::FeatureList::IsEnabled(features::kMaterialDesignBookmarks);
}
