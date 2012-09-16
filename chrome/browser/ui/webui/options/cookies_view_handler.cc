// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/cookies_view_handler.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/browser/browsing_data/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data/browsing_data_cookie_helper.h"
#include "chrome/browser/browsing_data/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data/browsing_data_file_system_helper.h"
#include "chrome/browser/browsing_data/browsing_data_flash_lso_helper.h"
#include "chrome/browser/browsing_data/browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"
#include "chrome/browser/browsing_data/browsing_data_quota_helper.h"
#include "chrome/browser/browsing_data/browsing_data_server_bound_cert_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/cookies_tree_model_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace options {

CookiesViewHandler::CookiesViewHandler()
  : batch_update_(false),
    app_context_(false),
    model_util_(new CookiesTreeModelUtil) {
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
    { "cookie_app_cache", IDS_COOKIES_APPLICATION_CACHE },
    { "cookie_flash_lso", IDS_COOKIES_FLASH_LSO },
    { "search_cookies", IDS_COOKIES_SEARCH_COOKIES },
    { "remove_cookie", IDS_COOKIES_REMOVE_LABEL },
    { "remove_all_cookie", IDS_COOKIES_REMOVE_ALL_LABEL },
    { "cookie_file_system", IDS_COOKIES_FILE_SYSTEM },
    { "label_file_system_origin", IDS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL },
    { "label_file_system_temporary_usage",
      IDS_COOKIES_FILE_SYSTEM_TEMPORARY_USAGE_LABEL },
    { "label_file_system_persistent_usage",
      IDS_COOKIES_FILE_SYSTEM_PERSISTENT_USAGE_LABEL },
    { "cookie_server_bound_cert", IDS_COOKIES_SERVER_BOUND_CERT },
    { "label_server_bound_cert_server_id",
      IDS_COOKIES_SERVER_BOUND_CERT_ORIGIN_LABEL },
    { "label_server_bound_cert_type",
      IDS_COOKIES_SERVER_BOUND_CERT_TYPE_LABEL },
    { "label_server_bound_cert_created",
      IDS_COOKIES_SERVER_BOUND_CERT_CREATED_LABEL },
    { "label_server_bound_cert_expires",
      IDS_COOKIES_SERVER_BOUND_CERT_EXPIRES_LABEL },
    { "label_protected_by_apps",
      IDS_GEOLOCATION_SET_BY_HOVER },  // TODO(bauerb): Use a better string
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "cookiesViewPage",
                IDS_COOKIES_WEBSITE_PERMISSIONS_WINDOW_TITLE);
  RegisterTitle(localized_strings, "appCookiesViewPage",
                IDS_APP_COOKIES_WEBSITE_PERMISSIONS_WINDOW_TITLE);
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
  web_ui()->RegisterMessageCallback("setViewContext",
      base::Bind(&CookiesViewHandler::SetViewContext,
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

  ListValue* children = new ListValue;
  model_util_->GetChildNodeList(parent_node, start, count,
                                            children);

  ListValue args;
  args.Append(parent == tree_model->GetRoot() ?
      Value::CreateNullValue() :
      Value::CreateStringValue(
          model_util_->GetTreeNodeId(parent_node)));
  args.Append(Value::CreateIntegerValue(start));
  args.Append(children);
  web_ui()->CallJavascriptFunction(
      GetCallback("onTreeItemAdded", tree_model), args);
}

void CookiesViewHandler::TreeNodesRemoved(ui::TreeModel* model,
                                          ui::TreeModelNode* parent,
                                          int start,
                                          int count) {
  // Skip if there is a batch update in progress.
  if (batch_update_)
    return;

  CookiesTreeModel* tree_model = static_cast<CookiesTreeModel*>(model);

  ListValue args;
  args.Append(parent == tree_model->GetRoot() ?
      Value::CreateNullValue() :
      Value::CreateStringValue(model_util_->GetTreeNodeId(
          tree_model->AsNode(parent))));
  args.Append(Value::CreateIntegerValue(start));
  args.Append(Value::CreateIntegerValue(count));
  web_ui()->CallJavascriptFunction(
      GetCallback("onTreeItemRemoved", tree_model), args);
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
  if (!app_context_ && !cookies_tree_model_.get()) {
    Profile* profile = Profile::FromWebUI(web_ui());
    ContainerMap apps_map;
    content::StoragePartition* storage_partition =
        content::BrowserContext::GetDefaultStoragePartition(profile);
    content::IndexedDBContext* indexed_db_context =
        storage_partition->GetIndexedDBContext();
    apps_map[std::string()] = new LocalDataContainer(
        "Site Data", std::string(),
        new BrowsingDataCookieHelper(profile->GetRequestContext()),
        new BrowsingDataDatabaseHelper(profile),
        new BrowsingDataLocalStorageHelper(profile),
        NULL,
        new BrowsingDataAppCacheHelper(profile),
        BrowsingDataIndexedDBHelper::Create(indexed_db_context),
        BrowsingDataFileSystemHelper::Create(profile),
        BrowsingDataQuotaHelper::Create(profile),
        BrowsingDataServerBoundCertHelper::Create(profile),
        BrowsingDataFlashLSOHelper::Create(profile));
    cookies_tree_model_.reset(
        new CookiesTreeModel(apps_map,
                             profile->GetExtensionSpecialStoragePolicy(),
                             false));
    cookies_tree_model_->AddCookiesTreeObserver(this);
  }

  if (app_context_ && !app_cookies_tree_model_.get()) {
    Profile* profile = Profile::FromWebUI(web_ui());
    ContainerMap apps_map;
    const ExtensionService* service = profile->GetExtensionService();
    if (service) {
      const ExtensionSet* extensions = service->extensions();
      for (ExtensionSet::const_iterator it = extensions->begin();
           it != extensions->end(); ++it) {
        if ((*it)->is_storage_isolated()) {
          net::URLRequestContextGetter* context_getter =
              profile->GetRequestContextForIsolatedApp((*it)->id());
          // TODO(nasko): When new types of storage are isolated, add the
          // appropriate browsing data helper objects to the constructor.
          // For now, just cookies are isolated, so other parameters are NULL.
          apps_map[(*it)->id()] = new LocalDataContainer(
              (*it)->name(), (*it)->id(),
              new BrowsingDataCookieHelper(context_getter),
              NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        }
      }
      app_cookies_tree_model_.reset(
          new CookiesTreeModel(apps_map,
                               profile->GetExtensionSpecialStoragePolicy(),
                               false));
      app_cookies_tree_model_->AddCookiesTreeObserver(this);
    }
  }
}

void CookiesViewHandler::UpdateSearchResults(const ListValue* args) {
  string16 query;
  if (!args->GetString(0, &query))
    return;

  EnsureCookiesTreeModelCreated();

  GetTreeModel()->UpdateSearchResults(query);
}

void CookiesViewHandler::RemoveAll(const ListValue* args) {
  EnsureCookiesTreeModelCreated();
  GetTreeModel()->DeleteAllStoredObjects();
}

void CookiesViewHandler::Remove(const ListValue* args) {
  std::string node_path;
  if (!args->GetString(0, &node_path))
    return;

  EnsureCookiesTreeModelCreated();

  const CookieTreeNode* node = model_util_->GetTreeNodeFromPath(
      GetTreeModel()->GetRoot(), node_path);
  if (node)
    GetTreeModel()->DeleteCookieNode(const_cast<CookieTreeNode*>(node));
}

void CookiesViewHandler::LoadChildren(const ListValue* args) {
  std::string node_path;
  if (!args->GetString(0, &node_path))
    return;

  EnsureCookiesTreeModelCreated();

  const CookieTreeNode* node = model_util_->GetTreeNodeFromPath(
      GetTreeModel()->GetRoot(), node_path);
  if (node)
    SendChildren(node);
}

void CookiesViewHandler::SendChildren(const CookieTreeNode* parent) {
  ListValue* children = new ListValue;
  model_util_->GetChildNodeList(parent, 0, parent->child_count(),
      children);

  ListValue args;
  args.Append(parent == GetTreeModel()->GetRoot() ?
      Value::CreateNullValue() :
      Value::CreateStringValue(model_util_->GetTreeNodeId(parent)));
  args.Append(children);

  web_ui()->CallJavascriptFunction(
      GetCallback("loadChildren", GetTreeModel()), args);
}

void CookiesViewHandler::SetViewContext(const base::ListValue* args) {
  bool app_context = false;
  if (args->GetBoolean(0, &app_context))
    app_context_ = app_context;
}

void CookiesViewHandler::ReloadCookies(const base::ListValue* args) {
  cookies_tree_model_.reset();
  app_cookies_tree_model_.reset();

  EnsureCookiesTreeModelCreated();
}

CookiesTreeModel* CookiesViewHandler::GetTreeModel() {
  CookiesTreeModel* model = app_context_ ?
      app_cookies_tree_model_.get() : cookies_tree_model_.get();
  DCHECK(model);
  return model;
}

std::string CookiesViewHandler::GetCallback(
     std::string method, CookiesTreeModel* model) {
  std::string callback("CookiesView");

  if (model == app_cookies_tree_model_)
    callback.append("App");
  return callback.append(".").append(method);
}

}  // namespace options
