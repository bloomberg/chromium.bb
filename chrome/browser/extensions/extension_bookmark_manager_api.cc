// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_bookmark_manager_api.h"

#include "app/l10n_util.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_html_writer.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/extensions/extension_bookmarks_module_constants.h"
#include "chrome/browser/importer/importer.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"

#include <vector>

namespace keys = extension_bookmarks_module_constants;

namespace {

// Returns a single bookmark node from the argument ID.
// This returns NULL in case of failure.
const BookmarkNode* GetNodeFromArguments(BookmarkModel* model,
    const Value* args) {
  std::string id_string;
  if (!args->GetAsString(&id_string))
    return NULL;
  int64 id;
  if (!StringToInt64(id_string, &id))
    return NULL;
  return model->GetNodeByID(id);
}

// Gets a vector of bookmark nodes from the argument list of IDs.
// This returns false in the case of failure.
bool GetNodesFromArguments(BookmarkModel* model, const ListValue* ids,
    std::vector<const BookmarkNode*>* nodes) {
  size_t count = ids->GetSize();
  if (count == 0)
    return false;

  for (size_t i = 0; i < count; ++i) {
    std::string id_string;
    if (!ids->GetString(i, &id_string))
      return false;
    int64 id;
    if (!StringToInt64(id_string, &id))
      return false;
    const BookmarkNode* node = model->GetNodeByID(id);
    if (!node)
      return false;
    nodes->push_back(node);
  }

  return true;
}

}

bool ClipboardBookmarkManagerFunction::CopyOrCut(bool cut) {
  BookmarkModel* model = profile()->GetBookmarkModel();
  std::vector<const BookmarkNode*> nodes;
  EXTENSION_FUNCTION_VALIDATE(GetNodesFromArguments(model, args_as_list(),
                                                    &nodes));
  bookmark_utils::CopyToClipboard(model, nodes, cut);
  return true;
}

bool CopyBookmarkManagerFunction::RunImpl() {
  return CopyOrCut(false);
}

bool CutBookmarkManagerFunction::RunImpl() {
  return CopyOrCut(true);
}

bool PasteBookmarkManagerFunction::RunImpl() {
  BookmarkModel* model = profile()->GetBookmarkModel();
  const BookmarkNode* parent_node = GetNodeFromArguments(model, args_.get());
  EXTENSION_FUNCTION_VALIDATE(parent_node);
  bool can_paste = bookmark_utils::CanPasteFromClipboard(parent_node);
  EXTENSION_FUNCTION_VALIDATE(can_paste);
  bookmark_utils::PasteFromClipboard(model, parent_node, -1);
  return true;
}

bool CanPasteBookmarkManagerFunction::RunImpl() {
  BookmarkModel* model = profile()->GetBookmarkModel();
  const BookmarkNode* parent_node = GetNodeFromArguments(model, args_.get());
  EXTENSION_FUNCTION_VALIDATE(parent_node);
  bool can_paste = bookmark_utils::CanPasteFromClipboard(parent_node);
  result_.reset(Value::CreateBooleanValue(can_paste));
  SendResponse(true);
  return true;
}

void BookmarkManagerIOFunction::SelectFile(SelectFileDialog::Type type) {
  // Balanced in one of the three callbacks of SelectFileDialog:
  // either FileSelectionCanceled, MultiFilesSelected, or FileSelected
  AddRef();
  select_file_dialog_ = SelectFileDialog::Create(this);
  SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("html"));

  select_file_dialog_->SelectFile(type,
                                  string16(),
                                  FilePath(),
                                  &file_type_info,
                                  0,
                                  FILE_PATH_LITERAL(""),
                                  NULL,
                                  NULL);
}

void BookmarkManagerIOFunction::FileSelectionCanceled(void* params) {
  Release();  // Balanced in BookmarkManagerIOFunction::SelectFile()
}

void BookmarkManagerIOFunction::MultiFilesSelected(
    const std::vector<FilePath>& files, void* params) {
  Release();  // Balanced in BookmarkManagerIOFunction::SelectFile()
  NOTREACHED() << "Should not be able to select multiple files";
}

bool ImportBookmarksFunction::RunImpl() {
  SelectFile(SelectFileDialog::SELECT_OPEN_FILE);
  return true;
}

void ImportBookmarksFunction::FileSelected(const FilePath& path,
                                           int index,
                                           void* params) {
  ImporterHost* host = new ImporterHost();
  ProfileInfo profile_info;
  profile_info.browser_type = BOOKMARKS_HTML;
  profile_info.source_path = path.ToWStringHack();
  host->StartImportSettings(profile_info,
                            profile(),
                            FAVORITES,
                            new ProfileWriter(profile()),
                            true);
  Release();  // Balanced in BookmarkManagerIOFunction::SelectFile()
}

bool ExportBookmarksFunction::RunImpl() {
  SelectFile(SelectFileDialog::SELECT_SAVEAS_FILE);
  return true;
}

void ExportBookmarksFunction::FileSelected(const FilePath& path,
                                                int index,
                                                void* params) {
  bookmark_html_writer::WriteBookmarks(profile()->GetBookmarkModel(), path);
  Release();  // Balanced in BookmarkManagerIOFunction::SelectFile()
}

bool SortChildrenBookmarkManagerFunction::RunImpl() {
  BookmarkModel* model = profile()->GetBookmarkModel();
  const BookmarkNode* parent_node = GetNodeFromArguments(model, args_.get());
  EXTENSION_FUNCTION_VALIDATE(parent_node);
  model->SortChildren(parent_node);
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
