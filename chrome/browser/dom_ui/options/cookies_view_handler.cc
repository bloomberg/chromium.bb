// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/options/cookies_view_handler.h"

#include "base/i18n/time_formatting.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "grit/generated_resources.h"
#include "net/base/cookie_monster.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

static const char kKeyId[]          = "id";
static const char kKeyTitle[]       = "title";
static const char kKeyIcon[]        = "icon";
static const char kKeyType[]        = "type";
static const char kKeyHasChildren[] = "hasChildren";

static const char kKeyName[]        = "name";
static const char kKeyContent[]     = "content";
static const char kKeyDomain[]      = "domain";
static const char kKeyPath[]        = "path";
static const char kKeySendFor[]     = "sendfor";
static const char kKeyDesc[]        = "desc";
static const char kKeySize[]        = "size";
static const char kKeyOrigin[]      = "origin";
static const char kKeyManifest[]    = "manifest";

static const char kKeyAccessed[]    = "accessed";
static const char kKeyCreated[]     = "created";
static const char kKeyExpires[]     = "expires";
static const char kKeyModified[]    = "modified";

// Encodes a pointer value into a hex string.
std::string PointerToHexString(const void* pointer) {
  return base::HexEncode(&pointer, sizeof(pointer));
}

// Decodes a pointer from a hex string.
void* HexStringToPointer(const std::string& str) {
  std::vector<uint8> buffer;
  if (!base::HexStringToBytes(str, &buffer) ||
      buffer.size() != sizeof(void*)) {
    return NULL;
  }

  return *reinterpret_cast<void**>(&buffer[0]);
}

// Populate given |dict| with cookie tree node properties.
void GetCookieTreeNodeDictionary(const CookieTreeNode& node,
                                 DictionaryValue* dict) {
  // Use node's address as an id for DOMUI to look it up.
  dict->SetString(kKeyId, PointerToHexString(&node));
  dict->SetString(kKeyTitle, node.GetTitle());
  dict->SetBoolean(kKeyHasChildren, !!node.GetChildCount());

  switch (node.GetDetailedInfo().node_type) {
    case CookieTreeNode::DetailedInfo::TYPE_ORIGIN: {
      dict->SetString(kKeyType, "origin");
#if defined(OS_MACOSX)
      dict->SetString(kKeyIcon, "chrome://theme/IDR_BOOKMARK_BAR_FOLDER");
#endif
      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_COOKIE: {
      dict->SetString(kKeyType, "cookie");
      dict->SetString(kKeyIcon, "chrome://theme/IDR_COOKIE_ICON");

      const net::CookieMonster::CanonicalCookie& cookie =
          *node.GetDetailedInfo().cookie;

      dict->SetString(kKeyName, cookie.Name());
      dict->SetString(kKeyContent, cookie.Value());
      dict->SetString(kKeyDomain, cookie.Domain());
      dict->SetString(kKeyPath, cookie.Path());
      dict->SetString(kKeySendFor, cookie.IsSecure() ?
          l10n_util::GetStringUTF8(IDS_COOKIES_COOKIE_SENDFOR_SECURE) :
          l10n_util::GetStringUTF8(IDS_COOKIES_COOKIE_SENDFOR_ANY));
      dict->SetString(kKeyCreated, UTF16ToUTF8(
          base::TimeFormatFriendlyDateAndTime(cookie.CreationDate())));
      dict->SetString(kKeyExpires, cookie.DoesExpire() ? UTF16ToUTF8(
          base::TimeFormatFriendlyDateAndTime(cookie.ExpiryDate())) :
          l10n_util::GetStringUTF8(IDS_COOKIES_COOKIE_EXPIRES_SESSION));

      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_DATABASE: {
      dict->SetString(kKeyType, "database");
      dict->SetString(kKeyIcon, "chrome://theme/IDR_COOKIE_STORAGE_ICON");

      const BrowsingDataDatabaseHelper::DatabaseInfo& database_info =
          *node.GetDetailedInfo().database_info;

      dict->SetString(kKeyName, database_info.database_name.empty() ?
          l10n_util::GetStringUTF8(IDS_COOKIES_WEB_DATABASE_UNNAMED_NAME) :
          database_info.database_name);
      dict->SetString(kKeyDesc, database_info.description);
      dict->SetString(kKeySize,
          FormatBytes(database_info.size,
                      GetByteDisplayUnits(database_info.size),
                      true));
      dict->SetString(kKeyModified, UTF16ToUTF8(
          base::TimeFormatFriendlyDateAndTime(database_info.last_modified)));

      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE: {
      dict->SetString(kKeyType, "local_storage");
      dict->SetString(kKeyIcon, "chrome://theme/IDR_COOKIE_STORAGE_ICON");

      const BrowsingDataLocalStorageHelper::LocalStorageInfo&
         local_storage_info = *node.GetDetailedInfo().local_storage_info;

      dict->SetString(kKeyOrigin, local_storage_info.origin);
      dict->SetString(kKeySize,
          FormatBytes(local_storage_info.size,
                      GetByteDisplayUnits(local_storage_info.size),
                      true));
      dict->SetString(kKeyModified, UTF16ToUTF8(
          base::TimeFormatFriendlyDateAndTime(
              local_storage_info.last_modified)));

      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_APPCACHE: {
      dict->SetString(kKeyType, "app_cache");
      dict->SetString(kKeyIcon, "chrome://theme/IDR_COOKIE_STORAGE_ICON");

      const appcache::AppCacheInfo& appcache_info =
          *node.GetDetailedInfo().appcache_info;

      dict->SetString(kKeyManifest, appcache_info.manifest_url.spec());
      dict->SetString(kKeySize,
          FormatBytes(appcache_info.size,
                      GetByteDisplayUnits(appcache_info.size),
                      true));
      dict->SetString(kKeyCreated, UTF16ToUTF8(
          base::TimeFormatFriendlyDateAndTime(appcache_info.creation_time)));
      dict->SetString(kKeyAccessed, UTF16ToUTF8(
          base::TimeFormatFriendlyDateAndTime(appcache_info.last_access_time)));

      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_INDEXED_DB: {
      dict->SetString(kKeyType, "indexed_db");
      dict->SetString(kKeyIcon, "chrome://theme/IDR_COOKIE_STORAGE_ICON");

      const BrowsingDataIndexedDBHelper::IndexedDBInfo& indexed_db_info =
          *node.GetDetailedInfo().indexed_db_info;

      dict->SetString(kKeyOrigin, indexed_db_info.origin);
      dict->SetString(kKeySize,
          FormatBytes(indexed_db_info.size,
                      GetByteDisplayUnits(indexed_db_info.size),
                      true));
      dict->SetString(kKeyModified, UTF16ToUTF8(
          base::TimeFormatFriendlyDateAndTime(indexed_db_info.last_modified)));

      break;
    }
    default:
#if defined(OS_MACOSX)
      dict->SetString(kKeyIcon, "chrome://theme/IDR_BOOKMARK_BAR_FOLDER");
#endif
      break;
  }
}

// Append the children nodes of |parent| in specified range to |nodes| list.
void GetChildNodeList(CookieTreeNode* parent, int start, int count,
                      ListValue* nodes) {
  for (int i = 0; i < count; ++i) {
    DictionaryValue* dict = new DictionaryValue;
    CookieTreeNode* child = parent->GetChild(start + i);
    GetCookieTreeNodeDictionary(*child, dict);
    nodes->Append(dict);
  }
}

// TODO(xiyuan): Remove this function when strings are updated.
// Remove "&" in button label for DOMUI.
string16 CleanButtonLabel(const string16& text) {
  string16 out(text);
  ReplaceFirstSubstringAfterOffset(&out, 0, ASCIIToUTF16("&"), string16());
  return out;
}

}  // namespace

CookiesViewHandler::CookiesViewHandler() : batch_update_(false) {
}

CookiesViewHandler::~CookiesViewHandler() {
}

void CookiesViewHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  localized_strings->SetString("cookiesViewPage",
      l10n_util::GetStringUTF16(IDS_COOKIES_WEBSITE_PERMISSIONS_WINDOW_TITLE));

  localized_strings->SetString("label_cookie_search",
      l10n_util::GetStringUTF16(IDS_COOKIES_SEARCH_LABEL));

  localized_strings->SetString("label_cookie_name",
      l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_NAME_LABEL));
  localized_strings->SetString("label_cookie_content",
      l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_CONTENT_LABEL));
  localized_strings->SetString("label_cookie_domain",
      l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_DOMAIN_LABEL));
  localized_strings->SetString("label_cookie_path",
      l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_PATH_LABEL));
  localized_strings->SetString("label_cookie_send_for",
      l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_SENDFOR_LABEL));
  localized_strings->SetString("label_cookie_created",
      l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_CREATED_LABEL));
  localized_strings->SetString("label_cookie_expires",
      l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_EXPIRES_LABEL));
  localized_strings->SetString("label_webdb_desc",
      l10n_util::GetStringUTF16(IDS_COOKIES_WEB_DATABASE_DESCRIPTION_LABEL));
  localized_strings->SetString("label_local_storage_size",
      l10n_util::GetStringUTF16(IDS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL));
  localized_strings->SetString("label_local_storage_last_modified",
      l10n_util::GetStringUTF16(IDS_COOKIES_LOCAL_STORAGE_LAST_MODIFIED_LABEL));
  localized_strings->SetString("label_local_storage_origin",
      l10n_util::GetStringUTF16(IDS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL));
  localized_strings->SetString("label_indexed_db_size",
      l10n_util::GetStringUTF16(IDS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL));
  localized_strings->SetString("label_indexed_db_last_modified",
      l10n_util::GetStringUTF16(IDS_COOKIES_LOCAL_STORAGE_LAST_MODIFIED_LABEL));
  localized_strings->SetString("label_indexed_db_origin",
      l10n_util::GetStringUTF16(IDS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL));
  localized_strings->SetString("label_app_cache_manifest",
      l10n_util::GetStringUTF16(IDS_COOKIES_APPLICATION_CACHE_MANIFEST_LABEL));
  localized_strings->SetString("label_cookie_last_accessed",
      l10n_util::GetStringUTF16(IDS_COOKIES_LAST_ACCESSED_LABEL));

  localized_strings->SetString("no_cookie",
      l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_NONESELECTED));
  localized_strings->SetString("unnamed",
      l10n_util::GetStringUTF16(IDS_COOKIES_WEB_DATABASE_UNNAMED_NAME));

  localized_strings->SetString("label_cookie_clear_search", CleanButtonLabel(
      l10n_util::GetStringUTF16(IDS_COOKIES_CLEAR_SEARCH_LABEL)));
  localized_strings->SetString("remove_cookie", CleanButtonLabel(
      l10n_util::GetStringUTF16(IDS_COOKIES_REMOVE_LABEL)));
  localized_strings->SetString("remove_all_cookie", CleanButtonLabel(
      l10n_util::GetStringUTF16(IDS_COOKIES_REMOVE_ALL_LABEL)));
}

void CookiesViewHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("updateCookieSearchResults",
      NewCallback(this, &CookiesViewHandler::UpdateSearchResults));
  dom_ui_->RegisterMessageCallback("removeAllCookies",
      NewCallback(this, &CookiesViewHandler::RemoveAll));
  dom_ui_->RegisterMessageCallback("removeCookie",
      NewCallback(this, &CookiesViewHandler::Remove));
  dom_ui_->RegisterMessageCallback("loadCookie",
      NewCallback(this, &CookiesViewHandler::LoadChildren));
}

void CookiesViewHandler::TreeNodesAdded(ui::TreeModel* model,
                                        ui::TreeModelNode* parent,
                                        int start,
                                        int count) {
  // Skip if there is a batch update in progress.
  if (batch_update_)
    return;

  ListValue* children = new ListValue;
  GetChildNodeList(cookies_tree_model_->AsNode(parent), start, count, children);

  ListValue args;
  args.Append(parent == cookies_tree_model_->GetRoot() ?
      Value::CreateNullValue() :
      Value::CreateStringValue(PointerToHexString(parent)));
  args.Append(Value::CreateIntegerValue(start));
  args.Append(children);
  dom_ui_->CallJavascriptFunction(L"CookiesView.onTreeItemAdded", args);
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
      Value::CreateStringValue(PointerToHexString(parent)));
  args.Append(Value::CreateIntegerValue(start));
  args.Append(Value::CreateIntegerValue(count));
  dom_ui_->CallJavascriptFunction(L"CookiesView.onTreeItemRemoved", args);
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
    Profile* profile = dom_ui_->GetProfile();
    cookies_tree_model_.reset(new CookiesTreeModel(
        profile->GetRequestContext()->GetCookieStore()->GetCookieMonster(),
        new BrowsingDataDatabaseHelper(profile),
        new BrowsingDataLocalStorageHelper(profile),
        NULL,
        new BrowsingDataAppCacheHelper(profile),
        BrowsingDataIndexedDBHelper::Create(profile)));
    cookies_tree_model_->AddObserver(this);
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

  CookieTreeNode* node = GetTreeNodeFromPath(node_path);
  if (node)
    cookies_tree_model_->DeleteCookieNode(node);
}

void CookiesViewHandler::LoadChildren(const ListValue* args) {
  std::string node_path;
  if (!args->GetString(0, &node_path)){
    return;
  }

  CookieTreeNode* node = GetTreeNodeFromPath(node_path);
  if (node)
    SendChildren(node);
}

CookieTreeNode* CookiesViewHandler::GetTreeNodeFromPath(
    const std::string& path) {
  std::vector<std::string> node_ids;
  base::SplitString(path, ',', &node_ids);

  CookieTreeNode* child = NULL;
  CookieTreeNode* parent = cookies_tree_model_->GetRoot();
  int child_index = -1;

  // Validate the tree path and get the node pointer.
  for (size_t i = 0; i < node_ids.size(); ++i) {
    child = reinterpret_cast<CookieTreeNode*>(
        HexStringToPointer(node_ids[i]));

    child_index = parent->IndexOfChild(child);
    if (child_index == -1)
      break;

    parent = child;
  }

  return child_index >= 0 ? child : NULL;
}

void CookiesViewHandler::SendChildren(CookieTreeNode* parent) {
  ListValue* children = new ListValue;
  GetChildNodeList(parent, 0, parent->GetChildCount(), children);

  ListValue args;
  args.Append(parent == cookies_tree_model_->GetRoot() ?
      Value::CreateNullValue() :
      Value::CreateStringValue(PointerToHexString(parent)));
  args.Append(children);

  dom_ui_->CallJavascriptFunction(L"CookiesView.loadChildren", args);
}
