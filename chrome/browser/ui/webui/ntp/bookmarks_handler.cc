// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/bookmarks_handler.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_extension_api_constants.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"

// TODO(csilv):
// Much of this implementation is based on the classes defined in
// extension_bookmarks_module.cc.  Longer term we should consider migrating
// NTP into an embedded extension which would allow us to leverage the same
// bookmark APIs as the bookmark manager.

namespace keys = bookmark_extension_api_constants;

BookmarksHandler::BookmarksHandler() : model_(NULL),
                                       dom_ready_(false),
                                       from_current_page_(false) {
}

BookmarksHandler::~BookmarksHandler() {
  if (model_)
    model_->RemoveObserver(this);
}

WebUIMessageHandler* BookmarksHandler::Attach(WebUI* web_ui) {
  WebUIMessageHandler::Attach(web_ui);
  model_ = Profile::FromWebUI(web_ui)->GetBookmarkModel();
  if (model_)
    model_->AddObserver(this);
  return this;
}

void BookmarksHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("createBookmark",
      base::Bind(&BookmarksHandler::HandleCreateBookmark,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("getBookmarksData",
      base::Bind(&BookmarksHandler::HandleGetBookmarksData,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("moveBookmark",
      base::Bind(&BookmarksHandler::HandleMoveBookmark,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("removeBookmark",
      base::Bind(&BookmarksHandler::HandleRemoveBookmark,
                 base::Unretained(this)));
}

void BookmarksHandler::Loaded(BookmarkModel* model, bool ids_reassigned) {
  if (dom_ready_)
    HandleGetBookmarksData(NULL);
}

void BookmarksHandler::BookmarkModelBeingDeleted(BookmarkModel* model) {
  // If this occurs it probably means that this tab will close shortly.
  // Discard our reference to the model so that we won't use it again.
  model_ = NULL;
}

void BookmarksHandler::BookmarkNodeMoved(BookmarkModel* model,
                                         const BookmarkNode* old_parent,
                                         int old_index,
                                         const BookmarkNode* new_parent,
                                         int new_index) {
  if (!dom_ready_) return;
  const BookmarkNode* node = new_parent->GetChild(new_index);
  base::StringValue id(base::Int64ToString(node->id()));
  base::DictionaryValue move_info;
  move_info.SetString(keys::kParentIdKey,
                      base::Int64ToString(new_parent->id()));
  move_info.SetInteger(keys::kIndexKey, new_index);
  move_info.SetString(keys::kOldParentIdKey,
                      base::Int64ToString(old_parent->id()));
  move_info.SetInteger(keys::kOldIndexKey, old_index);
  base::FundamentalValue from_page(from_current_page_);

  web_ui_->CallJavascriptFunction("ntp4.bookmarkNodeMoved", id, move_info,
                                  from_page);
}

void BookmarksHandler::BookmarkNodeAdded(BookmarkModel* model,
                                         const BookmarkNode* parent,
                                         int index) {
  if (!dom_ready_) return;
  const BookmarkNode* node = parent->GetChild(index);
  base::StringValue id(base::Int64ToString(node->id()));
  scoped_ptr<base::DictionaryValue> node_info(GetNodeDictionary(node));
  base::FundamentalValue from_page(from_current_page_);

  web_ui_->CallJavascriptFunction("ntp4.bookmarkNodeAdded", id, *node_info,
                                  from_page);
}

void BookmarksHandler::BookmarkNodeRemoved(BookmarkModel* model,
                                           const BookmarkNode* parent,
                                           int index,
                                           const BookmarkNode* node) {
  if (!dom_ready_) return;

  base::StringValue id(base::Int64ToString(node->id()));
  base::DictionaryValue remove_info;
  remove_info.SetString(keys::kParentIdKey, base::Int64ToString(parent->id()));
  remove_info.SetInteger(keys::kIndexKey, index);
  base::FundamentalValue from_page(from_current_page_);

  web_ui_->CallJavascriptFunction("ntp4.bookmarkNodeRemoved", id, remove_info,
                                  from_page);
}

void BookmarksHandler::BookmarkNodeChanged(BookmarkModel* model,
                                           const BookmarkNode* node) {
  if (!dom_ready_) return;
  base::StringValue id(base::Int64ToString(node->id()));
  base::DictionaryValue change_info;
  change_info.SetString(keys::kTitleKey, node->GetTitle());
  if (node->is_url())
    change_info.SetString(keys::kUrlKey, node->url().spec());
  base::FundamentalValue from_page(from_current_page_);

  web_ui_->CallJavascriptFunction("ntp4.bookmarkNodeChanged", id, change_info,
                                  from_page);
}

void BookmarksHandler::BookmarkNodeFaviconChanged(BookmarkModel* model,
                                                  const BookmarkNode* node) {
  // Favicons are handled by through use of the chrome://favicon protocol, so
  // there's nothing for us to do here (but we need to provide an
  // implementation).
}

void BookmarksHandler::BookmarkNodeChildrenReordered(BookmarkModel* model,
                                                     const BookmarkNode* node) {
  if (!dom_ready_) return;
  base::StringValue id(base::Int64ToString(node->id()));
  base::ListValue* children = new base::ListValue;
  for (int i = 0; i < node->child_count(); ++i) {
    const BookmarkNode* child = node->GetChild(i);
    Value* child_id = new StringValue(base::Int64ToString(child->id()));
    children->Append(child_id);
  }
  base::DictionaryValue reorder_info;
  reorder_info.Set(keys::kChildIdsKey, children);
  base::FundamentalValue from_page(from_current_page_);

  web_ui_->CallJavascriptFunction("ntp4.bookmarkNodeChildrenReordered", id,
                                  reorder_info, from_page);
}

void BookmarksHandler::BookmarkImportBeginning(BookmarkModel* model) {
  if (dom_ready_)
    web_ui_->CallJavascriptFunction("ntp4.bookmarkImportBegan");
}

void BookmarksHandler::BookmarkImportEnding(BookmarkModel* model) {
  if (dom_ready_)
    web_ui_->CallJavascriptFunction("ntp4.bookmarkImportEnded");
}

void BookmarksHandler::HandleCreateBookmark(const ListValue* args) {
  if (!model_) return;

  // This is the only required argument - a stringified int64 parent ID.
  std::string parent_id_string;
  CHECK(args->GetString(0, &parent_id_string));
  int64 parent_id;
  CHECK(base::StringToInt64(parent_id_string, &parent_id));

  const BookmarkNode* parent = model_->GetNodeByID(parent_id);
  if (!parent) return;

  double index;
  if (!args->GetDouble(1, &index) ||
      (index > parent->child_count() || index < 0)) {
    index = parent->child_count();
  }

  // If they didn't pass the argument, just use a blank string.
  string16 title;
  if (!args->GetString(2, &title))
    title = string16();

  // We'll be creating either a bookmark or a folder, so set this for both.
  AutoReset<bool> from_page(&from_current_page_, true);

  // According to the bookmarks API, "If url is NULL or missing, it will be a
  // folder.". Let's just follow the same behavior.
  std::string url_string;
  if (args->GetString(3, &url_string)) {
    GURL url(url_string);
    if (!url.is_empty() && url.is_valid()) {
      // Only valid case for a bookmark as opposed to folder.
      model_->AddURL(parent, static_cast<int>(index), title, url);
      return;
    }
  }

  // We didn't have all the valid parts for a bookmark, just make a folder.
  model_->AddFolder(parent, static_cast<int>(index), title);
}

void BookmarksHandler::HandleGetBookmarksData(const base::ListValue* args) {
  dom_ready_ = true;

  // At startup, Bookmarks may not be fully loaded. If this is the case,
  // we'll wait for the notification to arrive.
  Profile* profile = Profile::FromWebUI(web_ui_);
  BookmarkModel* model = profile->GetBookmarkModel();
  if (!model->IsLoaded()) return;

  int64 id;
  std::string id_string;
  PrefService* prefs = profile->GetPrefs();
  if (args && args->GetString(0, &id_string) &&
      base::StringToInt64(id_string, &id)) {
    // A folder ID was requested, so persist this value.
    prefs->SetInt64(prefs::kNTPShownBookmarksFolder, id);
  } else {
    // No folder ID was requested, so get the default (persisted) value.
    id = prefs->GetInt64(prefs::kNTPShownBookmarksFolder);
  }

  const BookmarkNode* node = model->GetNodeByID(id);
  if (!node)
    node = model->root_node();

  // We wish to merge the root node with the bookmarks bar node.
  if (model->is_root_node(node))
    node = model->bookmark_bar_node();

  base::ListValue* items = new base::ListValue;
  for (int i = 0; i < node->child_count(); ++i) {
    const BookmarkNode* child = node->GetChild(i);
    AddNode(child, items);
  }
  if (node == model->bookmark_bar_node() && model->other_node()->child_count())
    AddNode(model->other_node(), items);

  base::ListValue* navigation_items = new base::ListValue;
  while (node) {
    if (node != model->bookmark_bar_node())
      AddNode(node, navigation_items);
    node = node->parent();
  }

  base::DictionaryValue bookmarksData;
  bookmarksData.Set("items", items);
  bookmarksData.Set("navigationItems", navigation_items);
  web_ui_->CallJavascriptFunction("ntp4.setBookmarksData", bookmarksData);
}

void BookmarksHandler::HandleRemoveBookmark(const ListValue* args) {
  if (!model_) return;

  std::string id_string;
  CHECK(args->GetString(0, &id_string));
  int64 id;
  CHECK(base::StringToInt64(id_string, &id));

  const BookmarkNode* node = model_->GetNodeByID(id);
  if (!node) return;

  AutoReset<bool> from_page(&from_current_page_, true);
  model_->Remove(node->parent(), node->parent()->GetIndexOf(node));
}

void BookmarksHandler::HandleMoveBookmark(const ListValue* args) {
  if (!model_) return;

  std::string id_string;
  CHECK(args->GetString(0, &id_string));
  int64 id;
  CHECK(base::StringToInt64(id_string, &id));

  std::string parent_id_string;
  CHECK(args->GetString(1, &parent_id_string));
  int64 parent_id;
  CHECK(base::StringToInt64(parent_id_string, &parent_id));

  double index;
  args->GetDouble(2, &index);

  const BookmarkNode* parent = model_->GetNodeByID(parent_id);
  if (!parent) return;

  const BookmarkNode* node = model_->GetNodeByID(id);
  if (!node) return;

  AutoReset<bool> from_page(&from_current_page_, true);
  model_->Move(node, parent, static_cast<int>(index));
}

base::DictionaryValue* BookmarksHandler::GetNodeDictionary(
    const BookmarkNode* node) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString(keys::kIdKey, base::Int64ToString(node->id()));

  const BookmarkNode* parent = node->parent();
  if (parent) {
    dict->SetString(keys::kParentIdKey, base::Int64ToString(parent->id()));
    dict->SetInteger(keys::kIndexKey, parent->GetIndexOf(node));
  }

  NewTabUI::SetURLTitleAndDirection(dict, node->GetTitle(), node->url());

  if (!node->is_folder())
    dict->SetString(keys::kUrlKey, node->url().spec());

  return dict;
}

void BookmarksHandler::AddNode(const BookmarkNode* node,
                               base::ListValue* list) {
  list->Append(GetNodeDictionary(node));
}

// static
void BookmarksHandler::RegisterUserPrefs(PrefService* prefs) {
  // Default folder is the root node.
  // TODO(csilv): Should we sync this preference?
  prefs->RegisterInt64Pref(prefs::kNTPShownBookmarksFolder, 0,
                           PrefService::UNSYNCABLE_PREF);
}
