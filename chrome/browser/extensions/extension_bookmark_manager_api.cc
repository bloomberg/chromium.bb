// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_bookmark_manager_api.h"

#include "app/l10n_util.h"
#include "base/values.h"
#include "grit/generated_resources.h"

bool CopyBookmarkManagerFunction::RunImpl() {
  NOTIMPLEMENTED();
  return true;
}


bool CutBookmarkManagerFunction::RunImpl() {
  NOTIMPLEMENTED();
  return true;
}


bool PasteBookmarkManagerFunction::RunImpl() {
  NOTIMPLEMENTED();
  return true;
}


bool BookmarkManagerGetStringsFunction::RunImpl() {
  DictionaryValue* localized_strings = new DictionaryValue();

  localized_strings->SetString(L"title",
      l10n_util::GetString(IDS_BOOKMARK_MANAGER_TITLE));
  localized_strings->SetString(L"search_button",
      l10n_util::GetString(IDS_BOOKMARK_MANAGER_SEARCH_BUTTON));
  localized_strings->SetString(L"show_in_folder",
      l10n_util::GetString(IDS_BOOKMARK_MANAGER_SHOW_IN_FOLDER));
  localized_strings->SetString(L"sort",
      l10n_util::GetString(IDS_BOOKMARK_MANAGER_SORT));
  localized_strings->SetString(L"organize_menu",
      l10n_util::GetString(IDS_BOOKMARK_MANAGER_ORGANIZE_MENU));
  localized_strings->SetString(L"tools_menu",
      l10n_util::GetString(IDS_BOOKMARK_MANAGER_TOOLS_MENU));
  localized_strings->SetString(L"import_menu",
      l10n_util::GetString(IDS_BOOKMARK_MANAGER_IMPORT_MENU));
  localized_strings->SetString(L"export_menu",
      l10n_util::GetString(IDS_BOOKMARK_MANAGER_EXPORT_MENU));

  localized_strings->SetString(L"rename_folder",
      l10n_util::GetString(IDS_BOOKMARK_BAR_RENAME_FOLDER));
  localized_strings->SetString(L"edit",
      l10n_util::GetString(IDS_BOOKMARK_BAR_EDIT));
  localized_strings->SetString(L"should_open_all",
      l10n_util::GetString(IDS_BOOKMARK_BAR_SHOULD_OPEN_ALL));
  localized_strings->SetString(L"open_incognito",
      l10n_util::GetString(IDS_BOOMARK_BAR_OPEN_INCOGNITO));
  localized_strings->SetString(L"open_in_new_tab",
      l10n_util::GetString(IDS_BOOMARK_BAR_OPEN_IN_NEW_TAB));
  localized_strings->SetString(L"open_in_new_window",
      l10n_util::GetString(IDS_BOOMARK_BAR_OPEN_IN_NEW_WINDOW));
  localized_strings->SetString(L"add_new_bookmark",
      l10n_util::GetString(IDS_BOOMARK_BAR_ADD_NEW_BOOKMARK));
  localized_strings->SetString(L"new_folder",
      l10n_util::GetString(IDS_BOOMARK_BAR_NEW_FOLDER));
  localized_strings->SetString(L"open_all",
      l10n_util::GetString(IDS_BOOMARK_BAR_OPEN_ALL));
  localized_strings->SetString(L"open_all_new_window",
      l10n_util::GetString(IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW));
  localized_strings->SetString(L"open_all_incognito",
      l10n_util::GetString(IDS_BOOMARK_BAR_OPEN_ALL_INCOGNITO));
  localized_strings->SetString(L"remove",
      l10n_util::GetString(IDS_BOOKMARK_BAR_REMOVE));

  localized_strings->SetString(L"copy",
      l10n_util::GetString(IDS_CONTENT_CONTEXT_COPY));
  localized_strings->SetString(L"cut",
      l10n_util::GetString(IDS_CONTENT_CONTEXT_CUT));
  localized_strings->SetString(L"paste",
      l10n_util::GetString(IDS_CONTENT_CONTEXT_PASTE));
  localized_strings->SetString(L"delete",
      l10n_util::GetString(IDS_CONTENT_CONTEXT_DELETE));

  result_.reset(localized_strings);
  SendResponse(true);
  return true;
}
