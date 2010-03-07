// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_bookmark_manager_api.h"

#include <vector>

#include "app/l10n_util.h"
#include "base/json/json_writer.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_drag_data.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_html_writer.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/extensions/extension_bookmarks_module_constants.h"
#include "chrome/browser/extensions/extension_dom_ui.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/importer/importer.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"

namespace keys = extension_bookmarks_module_constants;

namespace {

typedef RenderViewHostDelegate::BookmarkDrag::DragData DragData;

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

// Recursively adds a node to a list. This is by used |BookmarkDragDataToJSON|
// when the data comes from the current profile. In this case we have a
// BookmarkNode since we got the data from the current profile.
void AddNodeToList(ListValue* list, const BookmarkNode& node) {
  DictionaryValue* dict = new DictionaryValue();

  // Add id and parentId so we can associate the data with existing nodes on the
  // client side.
  std::string id_string = Int64ToString(node.id());
  dict->SetString(keys::kIdKey, id_string);

  std::string parent_id_string = Int64ToString(node.GetParent()->id());
  dict->SetString(keys::kParentIdKey, parent_id_string);

  if (node.is_url())
    dict->SetString(keys::kUrlKey, node.GetURL().spec());

  dict->SetString(keys::kTitleKey, node.GetTitle());

  ListValue* children = new ListValue();
  for (int i = 0; i < node.GetChildCount(); ++i)
    AddNodeToList(children, *node.GetChild(i));
  dict->Set(keys::kChildrenKey, children);

  list->Append(dict);
}

// Recursively adds an element to a list. This is by used
// |BookmarkDragDataToJSON| when the data comes from a different profile. When
// the data comes from a different profile we do not have any IDs or parent IDs.
void AddElementToList(ListValue* list,
                      const BookmarkDragData::Element& element) {
  DictionaryValue* dict = new DictionaryValue();

  if (element.is_url)
    dict->SetString(keys::kUrlKey, element.url.spec());

  dict->SetString(keys::kTitleKey, UTF16ToWide(element.title));

  ListValue* children = new ListValue();
  for (size_t i = 0; i < element.children.size(); ++i)
    AddElementToList(children, element.children[i]);
  dict->Set(keys::kChildrenKey, children);

  list->Append(dict);
}

// Builds the JSON structure based on the BookmarksDragData.
void BookmarkDragDataToJSON(Profile* profile, const BookmarkDragData& data,
                            ListValue* args) {
  bool same_profile = data.IsFromProfile(profile);
  DictionaryValue* value = new DictionaryValue();
  value->SetBoolean(keys::kSameProfileKey, same_profile);

  ListValue* list = new ListValue();
  if (same_profile) {
    std::vector<const BookmarkNode*> nodes = data.GetNodes(profile);
    for (size_t i = 0; i < nodes.size(); ++i)
      AddNodeToList(list, *nodes[i]);
  } else {
    // We do not have an node IDs when the data comes from a different profile.
    std::vector<BookmarkDragData::Element> elements = data.elements;
    for (size_t i = 0; i < elements.size(); ++i)
      AddElementToList(list, elements[i]);
  }
  value->Set(keys::kElementsKey, list);

  args->Append(value);
}

// This is the platform specific function that takes the drag data and creates
// the BookmarkDragData as needed.
bool GetBookmarkDragData(const DragData* data,
                         BookmarkDragData* bookmark_drag_data) {
#if defined(TOOLKIT_VIEWS)
  // On TOOLKIT_VIEWS DragData is OSExchangeData.
  return bookmark_drag_data->Read(*data);
#elif defined(OS_MACOSX)
  return bookmark_drag_data->ReadFromDragClipboard();
#else
  NOTIMPLEMENTED();
  return false;
#endif
}

}  // namespace

ExtensionBookmarkManagerEventRouter::ExtensionBookmarkManagerEventRouter(
    Profile* profile, TabContents* tab_contents)
    : profile_(profile),
    tab_contents_(tab_contents) {
  tab_contents_->SetBookmarkDragDelegate(this);
}

ExtensionBookmarkManagerEventRouter::~ExtensionBookmarkManagerEventRouter() {
  if (tab_contents_->GetBookmarkDragDelegate() == this)
    tab_contents_->SetBookmarkDragDelegate(NULL);
}

void ExtensionBookmarkManagerEventRouter::DispatchEvent(const char* event_name,
                                                        const ListValue* args) {
  if (!profile_->GetExtensionMessageService())
    return;

  std::string json_args;
  base::JSONWriter::Write(args, false, &json_args);
  profile_->GetExtensionMessageService()->DispatchEventToRenderers(
      event_name, json_args, profile_->IsOffTheRecord());
}

void ExtensionBookmarkManagerEventRouter::DispatchDragEvent(
    const DragData* data, const char* event_name) {
  BookmarkDragData bookmark_drag_data;
  if (::GetBookmarkDragData(data, &bookmark_drag_data)) {
    ListValue args;
    BookmarkDragDataToJSON(profile_, bookmark_drag_data, &args);
    DispatchEvent(event_name, &args);
  }
}

void ExtensionBookmarkManagerEventRouter::OnDragEnter(const DragData* data) {
  DispatchDragEvent(data, keys::kOnBookmarkDragEnter);
}

void ExtensionBookmarkManagerEventRouter::OnDragOver(const DragData* data) {
  // Intentionally empty since these events happens too often and floods the
  // message queue. We do not need this event for the bookmark manager anyway.
}

void ExtensionBookmarkManagerEventRouter::OnDragLeave(const DragData* data) {
  DispatchDragEvent(data, keys::kOnBookmarkDragLeave);
}

void ExtensionBookmarkManagerEventRouter::OnDrop(const DragData* data) {
  DispatchDragEvent(data, keys::kOnBookmarkDrop);

  // Make a copy that is owned by this instance.
  ClearBookmarkDragData();
  ::GetBookmarkDragData(data, &bookmark_drag_data_);
}

const BookmarkDragData*
ExtensionBookmarkManagerEventRouter::GetBookmarkDragData() {
  if (bookmark_drag_data_.is_valid())
    return &bookmark_drag_data_;
  return NULL;
}

void ExtensionBookmarkManagerEventRouter::ClearBookmarkDragData() {
  bookmark_drag_data_.Clear();
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
  if (!parent_node) {
    error_ = keys::kNoParentError;
    return false;
  }
  bool can_paste = bookmark_utils::CanPasteFromClipboard(parent_node);
  if (!can_paste)
    return false;
  bookmark_utils::PasteFromClipboard(model, parent_node, -1);
  return true;
}

bool CanPasteBookmarkManagerFunction::RunImpl() {
  BookmarkModel* model = profile()->GetBookmarkModel();
  const BookmarkNode* parent_node = GetNodeFromArguments(model, args_.get());
  if (!parent_node) {
    error_ = keys::kNoParentError;
    return false;
  }
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
  bookmark_html_writer::WriteBookmarks(profile(), path, NULL);
  Release();  // Balanced in BookmarkManagerIOFunction::SelectFile()
}

bool SortChildrenBookmarkManagerFunction::RunImpl() {
  BookmarkModel* model = profile()->GetBookmarkModel();
  const BookmarkNode* parent_node = GetNodeFromArguments(model, args_.get());
  if (!parent_node) {
    error_ = keys::kNoParentError;
    return false;
  }
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
  localized_strings->SetString(L"new_folder_name",
      l10n_util::GetString(IDS_BOOMARK_EDITOR_NEW_FOLDER_NAME));
  localized_strings->SetString(L"name_input_placeholder",
      l10n_util::GetString(IDS_BOOKMARK_MANAGER_NAME_INPUT_PLACE_HOLDER));
  localized_strings->SetString(L"url_input_placeholder",
      l10n_util::GetString(IDS_BOOKMARK_MANAGER_URL_INPUT_PLACE_HOLDER));
  localized_strings->SetString(L"invalid_url",
      l10n_util::GetString(IDS_BOOKMARK_MANAGER_INVALID_URL));

  ChromeURLDataManager::DataSource::SetFontAndTextDirection(localized_strings);

  result_.reset(localized_strings);
  SendResponse(true);
  return true;
}

bool StartDragBookmarkManagerFunction::RunImpl() {
  BookmarkModel* model = profile()->GetBookmarkModel();
  std::vector<const BookmarkNode*> nodes;
  EXTENSION_FUNCTION_VALIDATE(
      GetNodesFromArguments(model, args_as_list(), &nodes));

  bookmark_utils::DragBookmarks(profile(), nodes,
      dispatcher()->GetExtensionDOMUI()->tab_contents()->GetNativeView());

  return true;
}

bool DropBookmarkManagerFunction::RunImpl() {
  BookmarkModel* model = profile()->GetBookmarkModel();

  // TODO(arv): The arguments change between a list and a value depending on the
  // parameters. See http://crbug.com/36301
  int64 id;
  std::string id_string;
  if (args_->IsType(Value::TYPE_STRING)) {
    EXTENSION_FUNCTION_VALIDATE(args_->GetAsString(&id_string));
  } else {
    EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_LIST));
    EXTENSION_FUNCTION_VALIDATE(args_as_list()->GetString(0, &id_string));
  }
  if (!StringToInt64(id_string, &id)) {
    error_ = keys::kInvalidIdError;
    return false;
  }

  const BookmarkNode* drop_parent = model->GetNodeByID(id);
  if (!drop_parent) {
    error_ = keys::kNoParentError;
    return false;
  }

  int drop_index;
  if (args_as_list()->GetSize() == 2)
    EXTENSION_FUNCTION_VALIDATE(args_as_list()->GetInteger(1, &drop_index));
  else
    drop_index = drop_parent->GetChildCount();

  ExtensionBookmarkManagerEventRouter* router = dispatcher()->
      GetExtensionDOMUI()->extension_bookmark_manager_event_router();

  DCHECK(router);

  bookmark_utils::PerformBookmarkDrop(profile(), *router->GetBookmarkDragData(),
                                      drop_parent, drop_index);

  router->ClearBookmarkDragData();
  SendResponse(true);
  return true;
}
