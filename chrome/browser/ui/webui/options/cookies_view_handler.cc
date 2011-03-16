// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/cookies_view_handler.h"

#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/cookies_tree_model_util.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

CookiesViewHandler::CookiesViewHandler() : batch_update_(false) {
}

CookiesViewHandler::~CookiesViewHandler() {
}

void CookiesViewHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "label_cookie_name", IDS_COOKIES_COOKIE_NAME_LABEL },
    { "label_cookie_content", IDS_COOKIES_COOKIE_CONTENT_LABEL },
    { "label_cookie_domain", IDS_COOKIES_COOKIE_DOMAIN_LABEL },
    { "label_cookie_path", IDS_COOKIES_COOKIE_PATH_LABEL },
    { "label_cookie_send_for", IDS_COOKIES_COOKIE_SENDFOR_LABEL },
    { "label_cookie_accessible_to_script",
      IDS_COOKIES_COOKIE_ACCESSIBLE_TO_SCRIPT_LABEL },
    { "label_cookie_created", IDS_COOKIES_COOKIE_CREATED_LABEL },
    { "label_cookie_expires", IDS_COOKIES_COOKIE_EXPIRES_LABEL },
    { "label_webdb_desc", IDS_COOKIES_WEB_DATABASE_DESCRIPTION_LABEL },
    { "label_local_storage_size",
      IDS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL },
    { "label_local_storage_last_modified",
      IDS_COOKIES_LOCAL_STORAGE_LAST_MODIFIED_LABEL },
    { "label_local_storage_origin", IDS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL },
    { "label_indexed_db_size", IDS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL },
    { "label_indexed_db_last_modified",
      IDS_COOKIES_LOCAL_STORAGE_LAST_MODIFIED_LABEL },
    { "label_indexed_db_origin", IDS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL },
    { "label_app_cache_manifest",
      IDS_COOKIES_APPLICATION_CACHE_MANIFEST_LABEL },
    { "label_cookie_last_accessed", IDS_COOKIES_LAST_ACCESSED_LABEL },
    { "cookie_domain", IDS_COOKIES_DOMAIN_COLUMN_HEADER },
    { "cookie_local_data", IDS_COOKIES_DATA_COLUMN_HEADER },
    { "cookie_singular", IDS_COOKIES_SINGLE_COOKIE },
    { "cookie_plural", IDS_COOKIES_PLURAL_COOKIES },
    { "cookie_database_storage", IDS_COOKIES_DATABASE_STORAGE },
    { "cookie_indexed_db", IDS_COOKIES_INDEXED_DB },
    { "cookie_local_storage", IDS_COOKIES_LOCAL_STORAGE },
    { "cookie_session_storage", IDS_COOKIES_SESSION_STORAGE },
    { "remove_cookie", IDS_COOKIES_REMOVE_LABEL },
    { "remove_all_cookie", IDS_COOKIES_REMOVE_ALL_LABEL },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "cookiesViewPage",
                IDS_COOKIES_WEBSITE_PERMISSIONS_WINDOW_TITLE);
}

void CookiesViewHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("updateCookieSearchResults",
      NewCallback(this, &CookiesViewHandler::UpdateSearchResults));
  web_ui_->RegisterMessageCallback("removeAllCookies",
      NewCallback(this, &CookiesViewHandler::RemoveAll));
  web_ui_->RegisterMessageCallback("removeCookie",
      NewCallback(this, &CookiesViewHandler::Remove));
  web_ui_->RegisterMessageCallback("loadCookie",
      NewCallback(this, &CookiesViewHandler::LoadChildren));
}

void CookiesViewHandler::TreeNodesAdded(ui::TreeModel* model,
                                        ui::TreeModelNode* parent,
                                        int start,
                                        int count) {
  // Skip if there is a batch update in progress.
  if (batch_update_)
    return;

  CookieTreeNode* parent_node = cookies_tree_model_->AsNode(parent);

  ListValue* children = new ListValue;
  cookies_tree_model_util::GetChildNodeList(parent_node, start, count,
                                            children);

  ListValue args;
  args.Append(parent == cookies_tree_model_->GetRoot() ?
      Value::CreateNullValue() :
      Value::CreateStringValue(
          cookies_tree_model_util::GetTreeNodeId(parent_node)));
  args.Append(Value::CreateIntegerValue(start));
  args.Append(children);
  web_ui_->CallJavascriptFunction("CookiesView.onTreeItemAdded", args);
}

void CookiesViewHandler::TreeNodesRemoved(ui::TreeModel* model,
                                          ui::TreeModelNode* parent,
                                          int start,
                                          int count) {
  // Skip if there is a batch update in progress.
  if (batch_update_)
    return;

  ListValue args;
  args.Append(parent == cookies_tree_model_->GetRoot() ?
      Value::CreateNullValue() :
      Value::CreateStringValue(cookies_tree_model_util::GetTreeNodeId(
          cookies_tree_model_->AsNode(parent))));
  args.Append(Value::CreateIntegerValue(start));
  args.Append(Value::CreateIntegerValue(count));
  web_ui_->CallJavascriptFunction("CookiesView.onTreeItemRemoved", args);
}

void CookiesViewHandler::TreeModelBeginBatch(CookiesTreeModel* model) {
  DCHECK(!batch_update_);  // There should be no nested batch begin.
  batch_update_ = true;
}

void CookiesViewHandler::TreeModelEndBatch(CookiesTreeModel* model) {
  DCHECK(batch_update_);
  batch_update_ = false;

  SendChildren(cookies_tree_model_->GetRoot());
}

void CookiesViewHandler::UpdateSearchResults(const ListValue* args) {
  std::string query;
  if (!args->GetString(0, &query)){
    return;
  }

  if (!cookies_tree_model_.get()) {
    Profile* profile = web_ui_->GetProfile();
    cookies_tree_model_.reset(new CookiesTreeModel(
        profile->GetRequestContext()->GetCookieStore()->GetCookieMonster(),
        new BrowsingDataDatabaseHelper(profile),
        new BrowsingDataLocalStorageHelper(profile),
        NULL,
        new BrowsingDataAppCacheHelper(profile),
        BrowsingDataIndexedDBHelper::Create(profile)));
    cookies_tree_model_->AddCookiesTreeObserver(this);
  }

  cookies_tree_model_->UpdateSearchResults(UTF8ToWide(query));
}

void CookiesViewHandler::RemoveAll(const ListValue* args) {
  cookies_tree_model_->DeleteAllStoredObjects();
}

void CookiesViewHandler::Remove(const ListValue* args) {
  std::string node_path;
  if (!args->GetString(0, &node_path)){
    return;
  }

  CookieTreeNode* node = cookies_tree_model_util::GetTreeNodeFromPath(
      cookies_tree_model_->GetRoot(), node_path);
  if (node)
    cookies_tree_model_->DeleteCookieNode(node);
}

void CookiesViewHandler::LoadChildren(const ListValue* args) {
  std::string node_path;
  if (!args->GetString(0, &node_path)){
    return;
  }

  CookieTreeNode* node = cookies_tree_model_util::GetTreeNodeFromPath(
      cookies_tree_model_->GetRoot(), node_path);
  if (node)
    SendChildren(node);
}

void CookiesViewHandler::SendChildren(CookieTreeNode* parent) {
  ListValue* children = new ListValue;
  cookies_tree_model_util::GetChildNodeList(parent, 0, parent->child_count(),
      children);

  ListValue args;
  args.Append(parent == cookies_tree_model_->GetRoot() ?
      Value::CreateNullValue() :
      Value::CreateStringValue(cookies_tree_model_util::GetTreeNodeId(parent)));
  args.Append(children);

  web_ui_->CallJavascriptFunction("CookiesView.loadChildren", args);
}
