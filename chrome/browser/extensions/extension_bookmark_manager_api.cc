// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_bookmark_manager_api.h"

#include <vector>

#include "base/json/json_writer.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/extensions/extension_bookmark_helpers.h"
#include "chrome/browser/extensions/extension_bookmarks_module_constants.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace keys = extension_bookmarks_module_constants;

namespace {

// Returns a single bookmark node from the argument ID.
// This returns NULL in case of failure.
const BookmarkNode* GetNodeFromArguments(BookmarkModel* model,
    const ListValue* args) {
  std::string id_string;
  if (!args->GetString(0, &id_string))
    return NULL;
  int64 id;
  if (!base::StringToInt64(id_string, &id))
    return NULL;
  return model->GetNodeByID(id);
}

// Gets a vector of bookmark nodes from the argument list of IDs.
// This returns false in the case of failure.
bool GetNodesFromArguments(BookmarkModel* model, const ListValue* args,
    size_t args_index, std::vector<const BookmarkNode*>* nodes) {

  ListValue* ids;
  if (!args->GetList(args_index, &ids))
    return false;

  size_t count = ids->GetSize();
  if (count == 0)
    return false;

  for (size_t i = 0; i < count; ++i) {
    std::string id_string;
    if (!ids->GetString(i, &id_string))
      return false;
    int64 id;
    if (!base::StringToInt64(id_string, &id))
      return false;
    const BookmarkNode* node = model->GetNodeByID(id);
    if (!node)
      return false;
    nodes->push_back(node);
  }

  return true;
}

// Recursively adds a node to a list. This is by used |BookmarkNodeDataToJSON|
// when the data comes from the current profile. In this case we have a
// BookmarkNode since we got the data from the current profile.
void AddNodeToList(ListValue* list, const BookmarkNode& node) {
  DictionaryValue* dict = new DictionaryValue();

  // Add id and parentId so we can associate the data with existing nodes on the
  // client side.
  std::string id_string = base::Int64ToString(node.id());
  dict->SetString(keys::kIdKey, id_string);

  std::string parent_id_string = base::Int64ToString(node.GetParent()->id());
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

// Recursively adds an element to a list. This is used by
// |BookmarkNodeDataToJSON| when the data comes from a different profile. When
// the data comes from a different profile we do not have any IDs or parent IDs.
void AddElementToList(ListValue* list,
                      const BookmarkNodeData::Element& element) {
  DictionaryValue* dict = new DictionaryValue();

  if (element.is_url)
    dict->SetString(keys::kUrlKey, element.url.spec());

  dict->SetString(keys::kTitleKey, element.title);

  ListValue* children = new ListValue();
  for (size_t i = 0; i < element.children.size(); ++i)
    AddElementToList(children, element.children[i]);
  dict->Set(keys::kChildrenKey, children);

  list->Append(dict);
}

// Builds the JSON structure based on the BookmarksDragData.
void BookmarkNodeDataToJSON(Profile* profile, const BookmarkNodeData& data,
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
    std::vector<BookmarkNodeData::Element> elements = data.elements;
    for (size_t i = 0; i < elements.size(); ++i)
      AddElementToList(list, elements[i]);
  }
  value->Set(keys::kElementsKey, list);

  args->Append(value);
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
  if (!profile_->GetExtensionEventRouter())
    return;

  std::string json_args;
  base::JSONWriter::Write(args, false, &json_args);
  profile_->GetExtensionEventRouter()->DispatchEventToRenderers(
      event_name, json_args, NULL, GURL());
}

void ExtensionBookmarkManagerEventRouter::DispatchDragEvent(
    const BookmarkNodeData& data, const char* event_name) {
  if (data.size() == 0)
    return;

  ListValue args;
  BookmarkNodeDataToJSON(profile_, data, &args);
  DispatchEvent(event_name, &args);
}

void ExtensionBookmarkManagerEventRouter::OnDragEnter(
    const BookmarkNodeData& data) {
  DispatchDragEvent(data, keys::kOnBookmarkDragEnter);
}

void ExtensionBookmarkManagerEventRouter::OnDragOver(
    const BookmarkNodeData& data) {
  // Intentionally empty since these events happens too often and floods the
  // message queue. We do not need this event for the bookmark manager anyway.
}

void ExtensionBookmarkManagerEventRouter::OnDragLeave(
    const BookmarkNodeData& data) {
  DispatchDragEvent(data, keys::kOnBookmarkDragLeave);
}

void ExtensionBookmarkManagerEventRouter::OnDrop(
    const BookmarkNodeData& data) {
  DispatchDragEvent(data, keys::kOnBookmarkDrop);

  // Make a copy that is owned by this instance.
  ClearBookmarkNodeData();
  bookmark_drag_data_ = data;
}

const BookmarkNodeData*
ExtensionBookmarkManagerEventRouter::GetBookmarkNodeData() {
  if (bookmark_drag_data_.is_valid())
    return &bookmark_drag_data_;
  return NULL;
}

void ExtensionBookmarkManagerEventRouter::ClearBookmarkNodeData() {
  bookmark_drag_data_.Clear();
}

bool ClipboardBookmarkManagerFunction::CopyOrCut(bool cut) {
  BookmarkModel* model = profile()->GetBookmarkModel();
  std::vector<const BookmarkNode*> nodes;
  EXTENSION_FUNCTION_VALIDATE(GetNodesFromArguments(model, args_.get(),
                                                    0, &nodes));
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

  // We want to use the highest index of the selected nodes as a destination.
  std::vector<const BookmarkNode*> nodes;
  // No need to test return value, if we got an empty list, we insert at end.
  GetNodesFromArguments(model, args_.get(), 1, &nodes);
  int highest_index = -1;  // -1 means insert at end of list.
  for (size_t node = 0; node < nodes.size(); ++node) {
    // + 1 so that we insert after the selection.
    int this_node_index = parent_node->IndexOfChild(nodes[node]) + 1;
    if (this_node_index > highest_index)
      highest_index = this_node_index;
  }

  bookmark_utils::PasteFromClipboard(model, parent_node, highest_index);
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

  localized_strings->SetString("title",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_TITLE));
  localized_strings->SetString("search_button",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_SEARCH_BUTTON));
  localized_strings->SetString("show_in_folder",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_SHOW_IN_FOLDER));
  localized_strings->SetString("sort",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_SORT));
  localized_strings->SetString("organize_menu",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_ORGANIZE_MENU));
  localized_strings->SetString("tools_menu",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_TOOLS_MENU));
  localized_strings->SetString("import_menu",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_IMPORT_MENU));
  localized_strings->SetString("export_menu",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_EXPORT_MENU));
  localized_strings->SetString("rename_folder",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_RENAME_FOLDER));
  localized_strings->SetString("edit",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_EDIT));
  localized_strings->SetString("should_open_all",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_SHOULD_OPEN_ALL));
  localized_strings->SetString("open_incognito",
      l10n_util::GetStringUTF16(IDS_BOOMARK_BAR_OPEN_INCOGNITO));
  localized_strings->SetString("open_in_new_tab",
      l10n_util::GetStringUTF16(IDS_BOOMARK_BAR_OPEN_IN_NEW_TAB));
  localized_strings->SetString("open_in_new_window",
      l10n_util::GetStringUTF16(IDS_BOOMARK_BAR_OPEN_IN_NEW_WINDOW));
  localized_strings->SetString("add_new_bookmark",
      l10n_util::GetStringUTF16(IDS_BOOMARK_BAR_ADD_NEW_BOOKMARK));
  localized_strings->SetString("new_folder",
      l10n_util::GetStringUTF16(IDS_BOOMARK_BAR_NEW_FOLDER));
  localized_strings->SetString("open_all",
      l10n_util::GetStringUTF16(IDS_BOOMARK_BAR_OPEN_ALL));
  localized_strings->SetString("open_all_new_window",
      l10n_util::GetStringUTF16(IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW));
  localized_strings->SetString("open_all_incognito",
      l10n_util::GetStringUTF16(IDS_BOOMARK_BAR_OPEN_ALL_INCOGNITO));
  localized_strings->SetString("remove",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_REMOVE));
  localized_strings->SetString("copy",
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_COPY));
  localized_strings->SetString("cut",
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_CUT));
  localized_strings->SetString("paste",
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_PASTE));
  localized_strings->SetString("delete",
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_DELETE));
  localized_strings->SetString("new_folder_name",
      l10n_util::GetStringUTF16(IDS_BOOMARK_EDITOR_NEW_FOLDER_NAME));
  localized_strings->SetString("name_input_placeholder",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_NAME_INPUT_PLACE_HOLDER));
  localized_strings->SetString("url_input_placeholder",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_URL_INPUT_PLACE_HOLDER));
  localized_strings->SetString("invalid_url",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_INVALID_URL));
  localized_strings->SetString("recent",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_RECENT));
  localized_strings->SetString("search",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_SEARCH));

  ChromeURLDataManager::DataSource::SetFontAndTextDirection(localized_strings);

  result_.reset(localized_strings);
  SendResponse(true);
  return true;
}

bool StartDragBookmarkManagerFunction::RunImpl() {
  BookmarkModel* model = profile()->GetBookmarkModel();
  std::vector<const BookmarkNode*> nodes;
  EXTENSION_FUNCTION_VALIDATE(
      GetNodesFromArguments(model, args_.get(), 0, &nodes));

  if (dispatcher()->render_view_host()->delegate()->GetRenderViewType() ==
      ViewType::TAB_CONTENTS) {
    ExtensionWebUI* web_ui =
        static_cast<ExtensionWebUI*>(dispatcher()->delegate());
    bookmark_utils::DragBookmarks(
        profile(), nodes, web_ui->tab_contents()->GetNativeView());

    return true;
  } else {
    NOTREACHED();
    return false;
  }
}

bool DropBookmarkManagerFunction::RunImpl() {
  BookmarkModel* model = profile()->GetBookmarkModel();

  int64 id;
  std::string id_string;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &id_string));

  if (!base::StringToInt64(id_string, &id)) {
    error_ = keys::kInvalidIdError;
    return false;
  }

  const BookmarkNode* drop_parent = model->GetNodeByID(id);
  if (!drop_parent) {
    error_ = keys::kNoParentError;
    return false;
  }

  int drop_index;
  if (args_->GetSize() == 2)
    EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(1, &drop_index));
  else
    drop_index = drop_parent->GetChildCount();

  if (dispatcher()->render_view_host()->delegate()->GetRenderViewType() ==
      ViewType::TAB_CONTENTS) {
    ExtensionWebUI* web_ui =
        static_cast<ExtensionWebUI*>(dispatcher()->delegate());
    ExtensionBookmarkManagerEventRouter* router =
        web_ui->extension_bookmark_manager_event_router();

    DCHECK(router);
    const BookmarkNodeData* drag_data = router->GetBookmarkNodeData();
    if (drag_data == NULL) {
      NOTREACHED() <<"Somehow we're dropping null bookmark data";
      return false;
    }
    bookmark_utils::PerformBookmarkDrop(profile(),
                                        *drag_data,
                                        drop_parent, drop_index);

    router->ClearBookmarkNodeData();
    SendResponse(true);
    return true;
  } else {
    NOTREACHED();
    return false;
  }
}

bool GetSubtreeBookmarkManagerFunction::RunImpl() {
  BookmarkModel* model = profile()->GetBookmarkModel();
  const BookmarkNode* node;
  int64 id;
  std::string id_string;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &id_string));
  bool folders_only;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(1, &folders_only));
  if (id_string == "") {
    node = model->root_node();
  } else {
     if (!base::StringToInt64(id_string, &id)) {
      error_ = keys::kInvalidIdError;
      return false;
    }
    node = model->GetNodeByID(id);
  }
  if (!node) {
    error_ = keys::kNoNodeError;
    return false;
  }
  scoped_ptr<ListValue> json(new ListValue());
  if (folders_only) {
    extension_bookmark_helpers::AddNodeFoldersOnly(node,
                                                   json.get(),
                                                   true);
  } else {
    extension_bookmark_helpers::AddNode(node, json.get(), true);
  }
  result_.reset(json.release());
  return true;
}
