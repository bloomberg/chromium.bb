// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"

#include <vector>

#include "base/json/json_writer.h"
#include "base/prefs/public/pref_service_base.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api_constants.h"
#include "chrome/browser/extensions/api/bookmarks/bookmark_api_constants.h"
#include "chrome/browser/extensions/api/bookmarks/bookmark_api_helpers.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/view_type_utils.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/web_ui_util.h"

#if defined(OS_WIN)
#include "win8/util/win8_util.h"
#endif  // OS_WIN

namespace extensions {

namespace bookmark_keys = bookmark_api_constants;
namespace manager_keys = bookmark_manager_api_constants;

using content::WebContents;

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

  const ListValue* ids;
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
  dict->SetString(bookmark_keys::kIdKey, id_string);

  std::string parent_id_string = base::Int64ToString(node.parent()->id());
  dict->SetString(bookmark_keys::kParentIdKey, parent_id_string);

  if (node.is_url())
    dict->SetString(bookmark_keys::kUrlKey, node.url().spec());

  dict->SetString(bookmark_keys::kTitleKey, node.GetTitle());

  ListValue* children = new ListValue();
  for (int i = 0; i < node.child_count(); ++i)
    AddNodeToList(children, *node.GetChild(i));
  dict->Set(bookmark_keys::kChildrenKey, children);

  list->Append(dict);
}

// Recursively adds an element to a list. This is used by
// |BookmarkNodeDataToJSON| when the data comes from a different profile. When
// the data comes from a different profile we do not have any IDs or parent IDs.
void AddElementToList(ListValue* list,
                      const BookmarkNodeData::Element& element) {
  DictionaryValue* dict = new DictionaryValue();

  if (element.is_url)
    dict->SetString(bookmark_keys::kUrlKey, element.url.spec());

  dict->SetString(bookmark_keys::kTitleKey, element.title);

  ListValue* children = new ListValue();
  for (size_t i = 0; i < element.children.size(); ++i)
    AddElementToList(children, element.children[i]);
  dict->Set(bookmark_keys::kChildrenKey, children);

  list->Append(dict);
}

// Builds the JSON structure based on the BookmarksDragData.
void BookmarkNodeDataToJSON(Profile* profile, const BookmarkNodeData& data,
                            ListValue* args) {
  bool same_profile = data.IsFromProfile(profile);
  DictionaryValue* value = new DictionaryValue();
  value->SetBoolean(manager_keys::kSameProfileKey, same_profile);

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
  value->Set(manager_keys::kElementsKey, list);

  args->Append(value);
}

}  // namespace

BookmarkManagerPrivateEventRouter::BookmarkManagerPrivateEventRouter(
    Profile* profile,
    content::WebContents* web_contents)
    : profile_(profile),
      web_contents_(web_contents) {
  BookmarkTabHelper* bookmark_tab_helper =
      BookmarkTabHelper::FromWebContents(web_contents_);
  bookmark_tab_helper->set_bookmark_drag_delegate(this);
}

BookmarkManagerPrivateEventRouter::~BookmarkManagerPrivateEventRouter() {
  BookmarkTabHelper* bookmark_tab_helper =
      BookmarkTabHelper::FromWebContents(web_contents_);
  if (bookmark_tab_helper->bookmark_drag_delegate() == this)
    bookmark_tab_helper->set_bookmark_drag_delegate(NULL);
}

void BookmarkManagerPrivateEventRouter::DispatchEvent(
    const char* event_name,
    scoped_ptr<ListValue> args) {
  if (!ExtensionSystem::Get(profile_)->event_router())
    return;

  scoped_ptr<Event> event(new Event(event_name, args.Pass()));
  ExtensionSystem::Get(profile_)->event_router()->BroadcastEvent(event.Pass());
}

void BookmarkManagerPrivateEventRouter::DispatchDragEvent(
    const BookmarkNodeData& data,
    const char* event_name) {
  if (data.size() == 0)
    return;

  scoped_ptr<ListValue> args(new ListValue());
  BookmarkNodeDataToJSON(profile_, data, args.get());
  DispatchEvent(event_name, args.Pass());
}

void BookmarkManagerPrivateEventRouter::OnDragEnter(
    const BookmarkNodeData& data) {
  DispatchDragEvent(data, manager_keys::kOnBookmarkDragEnter);
}

void BookmarkManagerPrivateEventRouter::OnDragOver(
    const BookmarkNodeData& data) {
  // Intentionally empty since these events happens too often and floods the
  // message queue. We do not need this event for the bookmark manager anyway.
}

void BookmarkManagerPrivateEventRouter::OnDragLeave(
    const BookmarkNodeData& data) {
  DispatchDragEvent(data, manager_keys::kOnBookmarkDragLeave);
}

void BookmarkManagerPrivateEventRouter::OnDrop(const BookmarkNodeData& data) {
  DispatchDragEvent(data, manager_keys::kOnBookmarkDrop);

  // Make a copy that is owned by this instance.
  ClearBookmarkNodeData();
  bookmark_drag_data_ = data;
}

const BookmarkNodeData*
BookmarkManagerPrivateEventRouter::GetBookmarkNodeData() {
  if (bookmark_drag_data_.is_valid())
    return &bookmark_drag_data_;
  return NULL;
}

void BookmarkManagerPrivateEventRouter::ClearBookmarkNodeData() {
  bookmark_drag_data_.Clear();
}

bool ClipboardBookmarkManagerFunction::CopyOrCut(bool cut) {
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile());
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
  if (!EditBookmarksEnabled())
    return false;
  return CopyOrCut(true);
}

bool PasteBookmarkManagerFunction::RunImpl() {
  if (!EditBookmarksEnabled())
    return false;
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile());
  const BookmarkNode* parent_node = GetNodeFromArguments(model, args_.get());
  if (!parent_node) {
    error_ = bookmark_keys::kNoParentError;
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
    int this_node_index = parent_node->GetIndexOf(nodes[node]) + 1;
    if (this_node_index > highest_index)
      highest_index = this_node_index;
  }

  bookmark_utils::PasteFromClipboard(model, parent_node, highest_index);
  return true;
}

bool CanPasteBookmarkManagerFunction::RunImpl() {
  if (!EditBookmarksEnabled())
    return false;
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile());
  const BookmarkNode* parent_node = GetNodeFromArguments(model, args_.get());
  if (!parent_node) {
    error_ = bookmark_keys::kNoParentError;
    return false;
  }
  bool can_paste = bookmark_utils::CanPasteFromClipboard(parent_node);
  SetResult(new base::FundamentalValue(can_paste));
  return true;
}

bool SortChildrenBookmarkManagerFunction::RunImpl() {
  if (!EditBookmarksEnabled())
    return false;
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile());
  const BookmarkNode* parent_node = GetNodeFromArguments(model, args_.get());
  if (!parent_node) {
    error_ = bookmark_keys::kNoParentError;
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
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_OPEN_INCOGNITO));
  localized_strings->SetString("open_in_new_tab",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_OPEN_IN_NEW_TAB));
  localized_strings->SetString("open_in_new_window",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_OPEN_IN_NEW_WINDOW));
  localized_strings->SetString("add_new_bookmark",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  localized_strings->SetString("new_folder",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_NEW_FOLDER));
  localized_strings->SetString("open_all",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_OPEN_ALL));
  localized_strings->SetString("open_all_new_window",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  localized_strings->SetString("open_all_incognito",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
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
  localized_strings->SetString("undo_delete",
      l10n_util::GetStringUTF16(IDS_UNDO_DELETE));
  localized_strings->SetString("new_folder_name",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_EDITOR_NEW_FOLDER_NAME));
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
  localized_strings->SetString("bookmark_all_tabs",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_BOOKMARK_ALL_TABS));
  localized_strings->SetString("save",
      l10n_util::GetStringUTF16(IDS_SAVE));
  localized_strings->SetString("cancel",
      l10n_util::GetStringUTF16(IDS_CANCEL));

  webui::SetFontAndTextDirection(localized_strings);

  SetResult(localized_strings);

  // This is needed because unlike the rest of these functions, this class
  // inherits from AsyncFunction directly, rather than BookmarkFunction.
  SendResponse(true);

  return true;
}

bool StartDragBookmarkManagerFunction::RunImpl() {
  if (!EditBookmarksEnabled())
    return false;
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile());
  std::vector<const BookmarkNode*> nodes;
  EXTENSION_FUNCTION_VALIDATE(
      GetNodesFromArguments(model, args_.get(), 0, &nodes));

  WebContents* web_contents =
      WebContents::FromRenderViewHost(render_view_host_);
  if (chrome::GetViewType(web_contents) == chrome::VIEW_TYPE_TAB_CONTENTS) {
    WebContents* web_contents =
        dispatcher()->delegate()->GetAssociatedWebContents();
    CHECK(web_contents);
    bookmark_utils::DragBookmarks(profile(), nodes,
                                  web_contents->GetNativeView());

    return true;
  } else {
    NOTREACHED();
    return false;
  }
}

bool DropBookmarkManagerFunction::RunImpl() {
  if (!EditBookmarksEnabled())
    return false;

  BookmarkModel* model =BookmarkModelFactory::GetForProfile(profile());

  int64 id;
  std::string id_string;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &id_string));

  if (!base::StringToInt64(id_string, &id)) {
    error_ = bookmark_keys::kInvalidIdError;
    return false;
  }

  const BookmarkNode* drop_parent = model->GetNodeByID(id);
  if (!drop_parent) {
    error_ = bookmark_keys::kNoParentError;
    return false;
  }

  int drop_index;
  if (HasOptionalArgument(1))
    EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(1, &drop_index));
  else
    drop_index = drop_parent->child_count();

  WebContents* web_contents =
      WebContents::FromRenderViewHost(render_view_host_);
  if (chrome::GetViewType(web_contents) == chrome::VIEW_TYPE_TAB_CONTENTS) {
    WebContents* web_contents =
        dispatcher()->delegate()->GetAssociatedWebContents();
    CHECK(web_contents);
    ExtensionWebUI* web_ui =
        static_cast<ExtensionWebUI*>(web_contents->GetWebUI()->GetController());
    CHECK(web_ui);
    BookmarkManagerPrivateEventRouter* router =
        web_ui->bookmark_manager_private_event_router();

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
    return true;
  } else {
    NOTREACHED();
    return false;
  }
}

bool GetSubtreeBookmarkManagerFunction::RunImpl() {
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile());
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
      error_ = bookmark_keys::kInvalidIdError;
      return false;
    }
    node = model->GetNodeByID(id);
  }
  if (!node) {
    error_ = bookmark_keys::kNoNodeError;
    return false;
  }
  scoped_ptr<ListValue> json(new ListValue());
  if (folders_only)
    bookmark_api_helpers::AddNodeFoldersOnly(node, json.get(), true);
  else
    bookmark_api_helpers::AddNode(node, json.get(), true);
  SetResult(json.release());
  return true;
}

bool CanEditBookmarkManagerFunction::RunImpl() {
  PrefServiceBase* prefs = PrefServiceBase::FromBrowserContext(profile_);
  SetResult(new base::FundamentalValue(
      prefs->GetBoolean(prefs::kEditBookmarksEnabled)));
  return true;
}

bool RecordLaunchBookmarkFunction::RunImpl() {
  bookmark_utils::RecordBookmarkLaunch(bookmark_utils::LAUNCH_MANAGER);
  return true;
}

bool CanOpenNewWindowsBookmarkFunction::RunImpl() {
  bool can_open_new_windows = true;

#if defined(OS_WIN)
  if (win8::IsSingleWindowMetroMode())
    can_open_new_windows = false;
#endif  // OS_WIN

  SetResult(new base::FundamentalValue(can_open_new_windows));
  return true;
}

}  // namespace extensions
