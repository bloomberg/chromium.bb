// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/cookies_view_handler.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browsing_data/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data/browsing_data_channel_id_helper.h"
#include "chrome/browser/browsing_data/browsing_data_cookie_helper.h"
#include "chrome/browser/browsing_data/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data/browsing_data_file_system_helper.h"
#include "chrome/browser/browsing_data/browsing_data_flash_lso_helper.h"
#include "chrome/browser/browsing_data/browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"
#include "chrome/browser/browsing_data/browsing_data_quota_helper.h"
#include "chrome/browser/browsing_data/browsing_data_service_worker_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/cookies_tree_model_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"

namespace storage {
class FileSystemContext;
}

namespace options {

CookiesViewHandler::CookiesViewHandler()
  : batch_update_(false),
    model_util_(new CookiesTreeModelUtil) {
}

CookiesViewHandler::~CookiesViewHandler() {
}

void CookiesViewHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
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
    { "label_service_worker_origin", IDS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL },
    { "label_service_worker_scopes", IDS_COOKIES_SERVICE_WORKER_SCOPES_LABEL },
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
    { "cookie_app_cache", IDS_COOKIES_APPLICATION_CACHE },
    { "cookie_service_worker", IDS_COOKIES_SERVICE_WORKER },
    { "cookie_flash_lso", IDS_COOKIES_FLASH_LSO },
    { "search_cookies", IDS_COOKIES_SEARCH_COOKIES },
    { "remove_cookie", IDS_COOKIES_REMOVE_LABEL },
    { "remove_all_cookie", IDS_COOKIES_REMOVE_ALL_LABEL },
    { "remove_all_shown_cookie", IDS_COOKIES_REMOVE_ALL_SHOWN_LABEL },
    { "cookie_file_system", IDS_COOKIES_FILE_SYSTEM },
    { "label_file_system_origin", IDS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL },
    { "label_file_system_temporary_usage",
      IDS_COOKIES_FILE_SYSTEM_TEMPORARY_USAGE_LABEL },
    { "label_file_system_persistent_usage",
      IDS_COOKIES_FILE_SYSTEM_PERSISTENT_USAGE_LABEL },
    { "cookie_channel_id", IDS_COOKIES_CHANNEL_ID },
    { "label_channel_id_server_id",
      IDS_COOKIES_CHANNEL_ID_ORIGIN_LABEL },
    { "label_channel_id_type",
      IDS_COOKIES_CHANNEL_ID_TYPE_LABEL },
    { "label_channel_id_created",
      IDS_COOKIES_CHANNEL_ID_CREATED_LABEL },
    { "label_channel_id_expires",
      IDS_COOKIES_CHANNEL_ID_EXPIRES_LABEL },
    { "label_protected_by_apps",
      IDS_GEOLOCATION_SET_BY_HOVER },  // TODO(bauerb): Use a better string
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "cookiesViewPage",
                IDS_COOKIES_WEBSITE_PERMISSIONS_WINDOW_TITLE);
}

void CookiesViewHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("updateCookieSearchResults",
      base::Bind(&CookiesViewHandler::UpdateSearchResults,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("removeAllCookies",
      base::Bind(&CookiesViewHandler::RemoveAll,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("removeCookie",
      base::Bind(&CookiesViewHandler::Remove,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("loadCookie",
      base::Bind(&CookiesViewHandler::LoadChildren,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("reloadCookies",
      base::Bind(&CookiesViewHandler::ReloadCookies,
                 base::Unretained(this)));
}

void CookiesViewHandler::TreeNodesAdded(ui::TreeModel* model,
                                        ui::TreeModelNode* parent,
                                        int start,
                                        int count) {
  // Skip if there is a batch update in progress.
  if (batch_update_)
    return;

  CookiesTreeModel* tree_model = static_cast<CookiesTreeModel*>(model);
  CookieTreeNode* parent_node = tree_model->AsNode(parent);

  base::ListValue* children = new base::ListValue;
  model_util_->GetChildNodeList(parent_node, start, count, children);

  base::ListValue args;
  args.Append(parent == tree_model->GetRoot() ?
      base::Value::CreateNullValue() :
      new base::StringValue(model_util_->GetTreeNodeId(parent_node)));
  args.Append(new base::FundamentalValue(start));
  args.Append(children);
  web_ui()->CallJavascriptFunction("CookiesView.onTreeItemAdded", args);
}

void CookiesViewHandler::TreeNodesRemoved(ui::TreeModel* model,
                                          ui::TreeModelNode* parent,
                                          int start,
                                          int count) {
  // Skip if there is a batch update in progress.
  if (batch_update_)
    return;

  CookiesTreeModel* tree_model = static_cast<CookiesTreeModel*>(model);

  base::ListValue args;
  args.Append(parent == tree_model->GetRoot() ?
      base::Value::CreateNullValue() :
      new base::StringValue(model_util_->GetTreeNodeId(
          tree_model->AsNode(parent))));
  args.Append(new base::FundamentalValue(start));
  args.Append(new base::FundamentalValue(count));
  web_ui()->CallJavascriptFunction("CookiesView.onTreeItemRemoved", args);
}

void CookiesViewHandler::TreeModelBeginBatch(CookiesTreeModel* model) {
  DCHECK(!batch_update_);  // There should be no nested batch begin.
  batch_update_ = true;
}

void CookiesViewHandler::TreeModelEndBatch(CookiesTreeModel* model) {
  DCHECK(batch_update_);
  batch_update_ = false;

  SendChildren(model->GetRoot());
}

void CookiesViewHandler::EnsureCookiesTreeModelCreated() {
  if (!cookies_tree_model_.get()) {
    Profile* profile = Profile::FromWebUI(web_ui());
    content::StoragePartition* storage_partition =
        content::BrowserContext::GetDefaultStoragePartition(profile);
    content::IndexedDBContext* indexed_db_context =
        storage_partition->GetIndexedDBContext();
    content::ServiceWorkerContext* service_worker_context =
        storage_partition->GetServiceWorkerContext();
    storage::FileSystemContext* file_system_context =
        storage_partition->GetFileSystemContext();
    LocalDataContainer* container = new LocalDataContainer(
        new BrowsingDataCookieHelper(profile->GetRequestContext()),
        new BrowsingDataDatabaseHelper(profile),
        new BrowsingDataLocalStorageHelper(profile),
        NULL,
        new BrowsingDataAppCacheHelper(profile),
        new BrowsingDataIndexedDBHelper(indexed_db_context),
        BrowsingDataFileSystemHelper::Create(file_system_context),
        BrowsingDataQuotaHelper::Create(profile),
        BrowsingDataChannelIDHelper::Create(profile),
        new BrowsingDataServiceWorkerHelper(service_worker_context),
        BrowsingDataFlashLSOHelper::Create(profile));
    cookies_tree_model_.reset(
        new CookiesTreeModel(container,
                             profile->GetExtensionSpecialStoragePolicy(),
                             false));
    cookies_tree_model_->AddCookiesTreeObserver(this);
  }
}

void CookiesViewHandler::UpdateSearchResults(const base::ListValue* args) {
  base::string16 query;
  if (!args->GetString(0, &query))
    return;

  EnsureCookiesTreeModelCreated();

  cookies_tree_model_->UpdateSearchResults(query);
}

void CookiesViewHandler::RemoveAll(const base::ListValue* args) {
  EnsureCookiesTreeModelCreated();
  cookies_tree_model_->DeleteAllStoredObjects();
}

void CookiesViewHandler::Remove(const base::ListValue* args) {
  std::string node_path;
  if (!args->GetString(0, &node_path))
    return;

  EnsureCookiesTreeModelCreated();

  const CookieTreeNode* node = model_util_->GetTreeNodeFromPath(
      cookies_tree_model_->GetRoot(), node_path);
  if (node)
    cookies_tree_model_->DeleteCookieNode(const_cast<CookieTreeNode*>(node));
}

void CookiesViewHandler::LoadChildren(const base::ListValue* args) {
  std::string node_path;
  if (!args->GetString(0, &node_path))
    return;

  EnsureCookiesTreeModelCreated();

  const CookieTreeNode* node = model_util_->GetTreeNodeFromPath(
      cookies_tree_model_->GetRoot(), node_path);
  if (node)
    SendChildren(node);
}

void CookiesViewHandler::SendChildren(const CookieTreeNode* parent) {
  base::ListValue* children = new base::ListValue;
  model_util_->GetChildNodeList(parent, 0, parent->child_count(),
      children);

  base::ListValue args;
  args.Append(parent == cookies_tree_model_->GetRoot() ?
      base::Value::CreateNullValue() :
      new base::StringValue(model_util_->GetTreeNodeId(parent)));
  args.Append(children);

  web_ui()->CallJavascriptFunction("CookiesView.loadChildren", args);
}

void CookiesViewHandler::ReloadCookies(const base::ListValue* args) {
  cookies_tree_model_.reset();

  EnsureCookiesTreeModelCreated();
}

}  // namespace options
