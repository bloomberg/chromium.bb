// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/bookmarks_handler.h"

#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/extensions/extension_bookmark_helpers.h"
#include "chrome/browser/extensions/extension_bookmarks_module_constants.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/common/notification_service.h"

// TODO(csilv):
// Much of this implementation is based on the classes defined in
// extension_bookmarks_module.cc.  Longer term we should consider migrating
// NTP into an embedded extension which would allow us to leverage the same
// bookmark APIs as the bookmark manager.

namespace keys = extension_bookmarks_module_constants;

BookmarksHandler::BookmarksHandler() : dom_ready_(false) {
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
  web_ui_->RegisterMessageCallback("getBookmarksData",
      NewCallback(this, &BookmarksHandler::HandleGetBookmarksData));
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
    const BookmarkNode* old_parent, int old_index,
    const BookmarkNode* new_parent, int new_index) {
  if (!dom_ready_) return;
  const BookmarkNode* node = new_parent->GetChild(new_index);
  StringValue id(base::Int64ToString(node->id()));
  DictionaryValue move_info;
  move_info.SetString(keys::kParentIdKey,
                      base::Int64ToString(new_parent->id()));
  move_info.SetInteger(keys::kIndexKey, new_index);
  move_info.SetString(keys::kOldParentIdKey,
                      base::Int64ToString(old_parent->id()));
  move_info.SetInteger(keys::kOldIndexKey, old_index);

  web_ui_->CallJavascriptFunction("ntp4.bookmarkNodeMoved", id, move_info);
}

void BookmarksHandler::BookmarkNodeAdded(BookmarkModel* model,
    const BookmarkNode* parent, int index) {
  if (!dom_ready_) return;
  const BookmarkNode* node = parent->GetChild(index);
  StringValue id(base::Int64ToString(node->id()));
  scoped_ptr<DictionaryValue> node_info(
      extension_bookmark_helpers::GetNodeDictionary(node, false, false));

  web_ui_->CallJavascriptFunction("ntp4.bookmarkNodeAdded", id, *node_info);
}

void BookmarksHandler::BookmarkNodeRemoved(BookmarkModel* model,
    const BookmarkNode* parent, int index, const BookmarkNode* node) {
  if (!dom_ready_) return;
  StringValue id(base::Int64ToString(node->id()));
  DictionaryValue remove_info;
  remove_info.SetString(keys::kParentIdKey,
                        base::Int64ToString(parent->id()));
  remove_info.SetInteger(keys::kIndexKey, index);

  web_ui_->CallJavascriptFunction("ntp4.bookmarkNodeRemoved", id, remove_info);
}

void BookmarksHandler::BookmarkNodeChanged(BookmarkModel* model,
                                           const BookmarkNode* node) {
  if (!dom_ready_) return;
  StringValue id(base::Int64ToString(node->id()));
  DictionaryValue change_info;
  change_info.SetString(keys::kTitleKey, node->GetTitle());
  if (node->is_url())
    change_info.SetString(keys::kUrlKey, node->url().spec());

  web_ui_->CallJavascriptFunction("ntp4.bookmarkNodeChanged", id, change_info);
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
  StringValue id(base::Int64ToString(node->id()));
  int childCount = node->child_count();
  ListValue* children = new ListValue();
  for (int i = 0; i < childCount; ++i) {
    const BookmarkNode* child = node->GetChild(i);
    Value* child_id = new StringValue(base::Int64ToString(child->id()));
    children->Append(child_id);
  }
  DictionaryValue reorder_info;
  reorder_info.Set(keys::kChildIdsKey, children);

  web_ui_->CallJavascriptFunction("ntp4.bookmarkNodeChildrenReordered", id,
                                  reorder_info);
}

void BookmarksHandler::BookmarkImportBeginning(BookmarkModel* model) {
  if (dom_ready_)
    web_ui_->CallJavascriptFunction("ntp4.bookmarkImportBegan");
}

void BookmarksHandler::BookmarkImportEnding(BookmarkModel* model) {
  if (dom_ready_)
    web_ui_->CallJavascriptFunction("ntp4.bookmarkImportEnded");
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

  base::ListValue* items = new base::ListValue();
  int child_count = node->child_count();
  for (int i = 0; i < child_count; ++i) {
    const BookmarkNode* child = node->GetChild(i);
    extension_bookmark_helpers::AddNode(child, items, false);
  }
  if (node == model->bookmark_bar_node() && model->other_node()->child_count())
    extension_bookmark_helpers::AddNode(model->other_node(), items, false);

  base::ListValue* navigation_items = new base::ListValue();
  while (node) {
    if (node != model->bookmark_bar_node())
      extension_bookmark_helpers::AddNode(node, navigation_items, false);
    node = node->parent();
  }

  base::DictionaryValue bookmarksData;
  bookmarksData.Set("items", items);
  bookmarksData.Set("navigationItems", navigation_items);
  web_ui_->CallJavascriptFunction("ntp4.setBookmarksData", bookmarksData);
}

// static
void BookmarksHandler::RegisterUserPrefs(PrefService* prefs) {
  // Default folder is the root node.
  // TODO(csilv): Should we sync this preference?
  prefs->RegisterInt64Pref(prefs::kNTPShownBookmarksFolder, 0,
                           PrefService::UNSYNCABLE_PREF);
}
