// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cookies_tree_model.h"

#include <algorithm>
#include <functional>
#include <vector>

#include "base/bind.h"
#include "base/memory/linked_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browsing_data_cookie_helper.h"
#include "chrome/browser/browsing_data_server_bound_cert_helper.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/extensions/extension_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "net/base/registry_controlled_domain.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

static const char kFileOriginNodeName[] = "file://";

///////////////////////////////////////////////////////////////////////////////
// CookieTreeNode, public:

void CookieTreeNode::DeleteStoredObjects() {
  std::for_each(children().begin(),
                children().end(),
                std::mem_fun(&CookieTreeNode::DeleteStoredObjects));
}

CookiesTreeModel* CookieTreeNode::GetModel() const {
  if (parent())
    return parent()->GetModel();
  else
    return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeCookieNode, public:

CookieTreeCookieNode::CookieTreeCookieNode(
    std::list<net::CookieMonster::CanonicalCookie>::iterator cookie)
    : CookieTreeNode(UTF8ToUTF16(cookie->Name())),
      cookie_(cookie) {
}

CookieTreeCookieNode::~CookieTreeCookieNode() {}

void CookieTreeCookieNode::DeleteStoredObjects() {
  // notify CookieMonster that we should delete this cookie
  GetModel()->cookie_helper_->DeleteCookie(*cookie_);
  GetModel()->cookie_list_.erase(cookie_);
}

CookieTreeNode::DetailedInfo CookieTreeCookieNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->parent()->GetTitle()).InitCookie(&*cookie_);
}

namespace {
// comparison functor, for use in CookieTreeRootNode
class OriginNodeComparator {
 public:
  bool operator() (const CookieTreeNode* lhs,
                   const CookieTreeNode* rhs) {
    // We want to order by registry controlled domain, so we would get
    // google.com, ad.google.com, www.google.com,
    // microsoft.com, ad.microsoft.com. CanonicalizeHost transforms the origins
    // into a form like google.com.www so that string comparisons work.
    return (CanonicalizeHost(lhs->GetTitle()) <
            CanonicalizeHost(rhs->GetTitle()));
  }

 private:
  static std::string CanonicalizeHost(const string16& host16) {
    // The canonicalized representation makes the registry controlled domain
    // come first, and then adds subdomains in reverse order, e.g.
    // 1.mail.google.com would become google.com.mail.1, and then a standard
    // string comparison works to order hosts by registry controlled domain
    // first. Leading dots are ignored, ".google.com" is the same as
    // "google.com".

    std::string host = UTF16ToUTF8(host16);
    std::string retval = net::RegistryControlledDomainService::
        GetDomainAndRegistry(host);
    if (!retval.length())  // Is an IP address or other special origin.
      return host;

    std::string::size_type position = host.rfind(retval);

    // The host may be the registry controlled domain, in which case fail fast.
    if (position == 0 || position == std::string::npos)
      return host;

    // If host is www.google.com, retval will contain google.com at this point.
    // Start operating to the left of the registry controlled domain, e.g. in
    // the www.google.com example, start at index 3.
    --position;

    // If position == 0, that means it's a dot; this will be ignored to treat
    // ".google.com" the same as "google.com".
    while (position > 0) {
      retval += std::string(".");
      // Copy up to the next dot. host[position] is a dot so start after it.
      std::string::size_type next_dot = host.rfind(".", position - 1);
      if (next_dot == std::string::npos) {
        retval += host.substr(0, position);
        break;
      }
      retval += host.substr(next_dot + 1, position - (next_dot + 1));
      position = next_dot;
    }
    return retval;
  }
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// CookieTreeAppCacheNode, public:

CookieTreeAppCacheNode::CookieTreeAppCacheNode(
    const GURL& origin_url,
    std::list<appcache::AppCacheInfo>::iterator appcache_info)
    : CookieTreeNode(UTF8ToUTF16(appcache_info->manifest_url.spec())),
      origin_url_(origin_url),
      appcache_info_(appcache_info) {
}

CookieTreeAppCacheNode::~CookieTreeAppCacheNode() {
}

void CookieTreeAppCacheNode::DeleteStoredObjects() {
  DCHECK(GetModel()->appcache_helper_);
  GetModel()->appcache_helper_->DeleteAppCacheGroup(
      appcache_info_->manifest_url);
  GetModel()->appcache_info_[origin_url_].erase(appcache_info_);
}

CookieTreeNode::DetailedInfo CookieTreeAppCacheNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->parent()->GetTitle()).InitAppCache(
      &*appcache_info_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeDatabaseNode, public:

CookieTreeDatabaseNode::CookieTreeDatabaseNode(
    std::list<BrowsingDataDatabaseHelper::DatabaseInfo>::iterator database_info)
    : CookieTreeNode(database_info->database_name.empty() ?
          l10n_util::GetStringUTF16(IDS_COOKIES_WEB_DATABASE_UNNAMED_NAME) :
          UTF8ToUTF16(database_info->database_name)),
      database_info_(database_info) {
}

CookieTreeDatabaseNode::~CookieTreeDatabaseNode() {}

void CookieTreeDatabaseNode::DeleteStoredObjects() {
  GetModel()->database_helper_->DeleteDatabase(
      database_info_->origin_identifier, database_info_->database_name);
  GetModel()->database_info_list_.erase(database_info_);
}

CookieTreeNode::DetailedInfo CookieTreeDatabaseNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->parent()->GetTitle()).InitDatabase(
      &*database_info_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeLocalStorageNode, public:

CookieTreeLocalStorageNode::CookieTreeLocalStorageNode(
    std::list<BrowsingDataLocalStorageHelper::LocalStorageInfo>::iterator
        local_storage_info)
    : CookieTreeNode(UTF8ToUTF16(local_storage_info->origin_url.spec())),
      local_storage_info_(local_storage_info) {
}

CookieTreeLocalStorageNode::~CookieTreeLocalStorageNode() {}

void CookieTreeLocalStorageNode::DeleteStoredObjects() {
  GetModel()->local_storage_helper_->DeleteOrigin(
      local_storage_info_->origin_url);
  GetModel()->local_storage_info_list_.erase(local_storage_info_);
}

CookieTreeNode::DetailedInfo
CookieTreeLocalStorageNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->parent()->GetTitle()).InitLocalStorage(
      &*local_storage_info_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeSessionStorageNode, public:

CookieTreeSessionStorageNode::CookieTreeSessionStorageNode(
    std::list<BrowsingDataLocalStorageHelper::LocalStorageInfo>::iterator
        session_storage_info)
    : CookieTreeNode(UTF8ToUTF16(session_storage_info->origin_url.spec())),
      session_storage_info_(session_storage_info) {
}

CookieTreeSessionStorageNode::~CookieTreeSessionStorageNode() {}

void CookieTreeSessionStorageNode::DeleteStoredObjects() {
  GetModel()->session_storage_info_list_.erase(session_storage_info_);
}

CookieTreeNode::DetailedInfo
CookieTreeSessionStorageNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->parent()->GetTitle()).InitSessionStorage(
      &*session_storage_info_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeIndexedDBNode, public:

CookieTreeIndexedDBNode::CookieTreeIndexedDBNode(
    std::list<BrowsingDataIndexedDBHelper::IndexedDBInfo>::iterator
        indexed_db_info)
    : CookieTreeNode(UTF8ToUTF16(
          indexed_db_info->origin.spec())),
      indexed_db_info_(indexed_db_info) {
}

CookieTreeIndexedDBNode::~CookieTreeIndexedDBNode() {}

void CookieTreeIndexedDBNode::DeleteStoredObjects() {
  GetModel()->indexed_db_helper_->DeleteIndexedDB(
      indexed_db_info_->origin);
  GetModel()->indexed_db_info_list_.erase(indexed_db_info_);
}

CookieTreeNode::DetailedInfo CookieTreeIndexedDBNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->parent()->GetTitle()).InitIndexedDB(
      &*indexed_db_info_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeFileSystemNode, public:

CookieTreeFileSystemNode::CookieTreeFileSystemNode(
    std::list<BrowsingDataFileSystemHelper::FileSystemInfo>::iterator
        file_system_info)
    : CookieTreeNode(UTF8ToUTF16(
          file_system_info->origin.spec())),
      file_system_info_(file_system_info) {
}

CookieTreeFileSystemNode::~CookieTreeFileSystemNode() {}

void CookieTreeFileSystemNode::DeleteStoredObjects() {
  GetModel()->file_system_helper_->DeleteFileSystemOrigin(
      file_system_info_->origin);
  GetModel()->file_system_info_list_.erase(file_system_info_);
}

CookieTreeNode::DetailedInfo CookieTreeFileSystemNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->parent()->GetTitle()).InitFileSystem(
      &*file_system_info_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeQuotaNode, public:

CookieTreeQuotaNode::CookieTreeQuotaNode(
    std::list<BrowsingDataQuotaHelper::QuotaInfo>::iterator quota_info)
    : CookieTreeNode(UTF8ToUTF16(quota_info->host)),
      quota_info_(quota_info) {
}

CookieTreeQuotaNode::~CookieTreeQuotaNode() {}

void CookieTreeQuotaNode::DeleteStoredObjects() {
  // Calling this function may cause unexpected over-quota state of origin.
  // However, it'll caused no problem, just prevent usage growth of the origin.
  GetModel()->quota_helper_->RevokeHostQuota(quota_info_->host);
  GetModel()->quota_info_list_.erase(quota_info_);
}

CookieTreeNode::DetailedInfo CookieTreeQuotaNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->parent()->GetTitle()).InitQuota(
      &*quota_info_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeServerBoundCertNode, public:

CookieTreeServerBoundCertNode::CookieTreeServerBoundCertNode(
      net::ServerBoundCertStore::ServerBoundCertList::iterator cert)
    : CookieTreeNode(ASCIIToUTF16(cert->server_identifier())),
      server_bound_cert_(cert) {
}

CookieTreeServerBoundCertNode::~CookieTreeServerBoundCertNode() {}

void CookieTreeServerBoundCertNode::DeleteStoredObjects() {
  GetModel()->server_bound_cert_helper_->DeleteServerBoundCert(
      server_bound_cert_->server_identifier());
  GetModel()->server_bound_cert_list_.erase(server_bound_cert_);
}

CookieTreeNode::DetailedInfo
CookieTreeServerBoundCertNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->parent()->GetTitle()).InitServerBoundCert(
      &*server_bound_cert_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeRootNode, public:

CookieTreeRootNode::CookieTreeRootNode(CookiesTreeModel* model)
    : model_(model) {
}

CookieTreeRootNode::~CookieTreeRootNode() {}

CookieTreeOriginNode* CookieTreeRootNode::GetOrCreateOriginNode(
    const GURL& url) {
  CookieTreeOriginNode origin_node(url);

  // First see if there is an existing match.
  std::vector<CookieTreeNode*>::iterator origin_node_iterator =
      std::lower_bound(children().begin(),
                       children().end(),
                       &origin_node,
                       OriginNodeComparator());

  if (origin_node_iterator != children().end() &&
      WideToUTF16Hack(CookieTreeOriginNode::TitleForUrl(url)) ==
      (*origin_node_iterator)->GetTitle())
    return static_cast<CookieTreeOriginNode*>(*origin_node_iterator);
  // Node doesn't exist, create a new one and insert it into the (ordered)
  // children.
  CookieTreeOriginNode* retval = new CookieTreeOriginNode(url);
  DCHECK(model_);
  model_->Add(this, retval, (origin_node_iterator - children().begin()));
  return retval;
}

CookiesTreeModel* CookieTreeRootNode::GetModel() const {
  return model_;
}

CookieTreeNode::DetailedInfo CookieTreeRootNode::GetDetailedInfo() const {
  return DetailedInfo(string16()).Init(DetailedInfo::TYPE_ROOT);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeOriginNode, public:

// static
std::wstring CookieTreeOriginNode::TitleForUrl(
    const GURL& url) {
  return UTF8ToWide(url.SchemeIsFile() ? kFileOriginNodeName : url.host());
}

CookieTreeOriginNode::CookieTreeOriginNode(const GURL& url)
    : CookieTreeNode(WideToUTF16Hack(TitleForUrl(url))),
      cookies_child_(NULL),
      databases_child_(NULL),
      local_storages_child_(NULL),
      session_storages_child_(NULL),
      appcaches_child_(NULL),
      indexed_dbs_child_(NULL),
      file_systems_child_(NULL),
      quota_child_(NULL),
      server_bound_certs_child_(NULL),
      url_(url) {}

CookieTreeOriginNode::~CookieTreeOriginNode() {}

CookieTreeNode::DetailedInfo CookieTreeOriginNode::GetDetailedInfo() const {
  return DetailedInfo(GetTitle()).Init(DetailedInfo::TYPE_ORIGIN);
}

CookieTreeCookiesNode* CookieTreeOriginNode::GetOrCreateCookiesNode() {
  if (cookies_child_)
    return cookies_child_;
  cookies_child_ = new CookieTreeCookiesNode;
  AddChildSortedByTitle(cookies_child_);
  return cookies_child_;
}

CookieTreeDatabasesNode* CookieTreeOriginNode::GetOrCreateDatabasesNode() {
  if (databases_child_)
    return databases_child_;
  databases_child_ = new CookieTreeDatabasesNode;
  AddChildSortedByTitle(databases_child_);
  return databases_child_;
}

CookieTreeLocalStoragesNode*
    CookieTreeOriginNode::GetOrCreateLocalStoragesNode() {
  if (local_storages_child_)
    return local_storages_child_;
  local_storages_child_ = new CookieTreeLocalStoragesNode;
  AddChildSortedByTitle(local_storages_child_);
  return local_storages_child_;
}

CookieTreeSessionStoragesNode*
    CookieTreeOriginNode::GetOrCreateSessionStoragesNode() {
  if (session_storages_child_)
    return session_storages_child_;
  session_storages_child_ = new CookieTreeSessionStoragesNode;
  AddChildSortedByTitle(session_storages_child_);
  return session_storages_child_;
}

CookieTreeAppCachesNode* CookieTreeOriginNode::GetOrCreateAppCachesNode() {
  if (appcaches_child_)
    return appcaches_child_;
  appcaches_child_ = new CookieTreeAppCachesNode;
  AddChildSortedByTitle(appcaches_child_);
  return appcaches_child_;
}

CookieTreeIndexedDBsNode* CookieTreeOriginNode::GetOrCreateIndexedDBsNode() {
  if (indexed_dbs_child_)
    return indexed_dbs_child_;
  indexed_dbs_child_ = new CookieTreeIndexedDBsNode;
  AddChildSortedByTitle(indexed_dbs_child_);
  return indexed_dbs_child_;
}

CookieTreeFileSystemsNode* CookieTreeOriginNode::GetOrCreateFileSystemsNode() {
  if (file_systems_child_)
    return file_systems_child_;
  file_systems_child_ = new CookieTreeFileSystemsNode;
  AddChildSortedByTitle(file_systems_child_);
  return file_systems_child_;
}

CookieTreeQuotaNode* CookieTreeOriginNode::UpdateOrCreateQuotaNode(
    std::list<BrowsingDataQuotaHelper::QuotaInfo>::iterator quota_info) {
  if (quota_child_)
    return quota_child_;
  quota_child_ = new CookieTreeQuotaNode(quota_info);
  AddChildSortedByTitle(quota_child_);
  return quota_child_;
}

CookieTreeServerBoundCertsNode*
CookieTreeOriginNode::GetOrCreateServerBoundCertsNode() {
  if (server_bound_certs_child_)
    return server_bound_certs_child_;
  server_bound_certs_child_ = new CookieTreeServerBoundCertsNode;
  AddChildSortedByTitle(server_bound_certs_child_);
  return server_bound_certs_child_;
}

void CookieTreeOriginNode::CreateContentException(
    CookieSettings* cookie_settings, ContentSetting setting) const {
  DCHECK(setting == CONTENT_SETTING_ALLOW ||
         setting == CONTENT_SETTING_BLOCK ||
         setting == CONTENT_SETTING_SESSION_ONLY);
  if (CanCreateContentException()) {
    cookie_settings->ResetCookieSetting(
        ContentSettingsPattern::FromURLNoWildcard(url_),
        ContentSettingsPattern::Wildcard());
    cookie_settings->SetCookieSetting(
        ContentSettingsPattern::FromURL(url_),
        ContentSettingsPattern::Wildcard(), setting);
  }
}

bool CookieTreeOriginNode::CanCreateContentException() const {
  return !url_.SchemeIsFile();
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeCookiesNode, public:

CookieTreeCookiesNode::CookieTreeCookiesNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_COOKIES)) {
}

CookieTreeCookiesNode::~CookieTreeCookiesNode() {
}

CookieTreeNode::DetailedInfo CookieTreeCookiesNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->GetTitle()).Init(DetailedInfo::TYPE_COOKIES);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeAppCachesNode, public:

CookieTreeAppCachesNode::CookieTreeAppCachesNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(
                         IDS_COOKIES_APPLICATION_CACHES)) {
}

CookieTreeAppCachesNode::~CookieTreeAppCachesNode() {}

CookieTreeNode::DetailedInfo CookieTreeAppCachesNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->GetTitle()).Init(DetailedInfo::TYPE_APPCACHES);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeDatabasesNode, public:

CookieTreeDatabasesNode::CookieTreeDatabasesNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_WEB_DATABASES)) {
}

CookieTreeDatabasesNode::~CookieTreeDatabasesNode() {}

CookieTreeNode::DetailedInfo CookieTreeDatabasesNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->GetTitle()).Init(DetailedInfo::TYPE_DATABASES);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeLocalStoragesNode, public:

CookieTreeLocalStoragesNode::CookieTreeLocalStoragesNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_LOCAL_STORAGE)) {
}

CookieTreeLocalStoragesNode::~CookieTreeLocalStoragesNode() {}

CookieTreeNode::DetailedInfo
CookieTreeLocalStoragesNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->GetTitle()).Init(
      DetailedInfo::TYPE_LOCAL_STORAGES);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeSessionStoragesNode, public:

CookieTreeSessionStoragesNode::CookieTreeSessionStoragesNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_SESSION_STORAGE)) {
}

CookieTreeSessionStoragesNode::~CookieTreeSessionStoragesNode() {}

CookieTreeNode::DetailedInfo
CookieTreeSessionStoragesNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->GetTitle()).Init(
      DetailedInfo::TYPE_SESSION_STORAGES);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeIndexedDBsNode, public:

CookieTreeIndexedDBsNode::CookieTreeIndexedDBsNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_INDEXED_DBS)) {
}

CookieTreeIndexedDBsNode::~CookieTreeIndexedDBsNode() {}

CookieTreeNode::DetailedInfo
CookieTreeIndexedDBsNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->GetTitle()).Init(
      DetailedInfo::TYPE_INDEXED_DBS);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeFileSystemsNode, public:

CookieTreeFileSystemsNode::CookieTreeFileSystemsNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_FILE_SYSTEMS)) {
}

CookieTreeFileSystemsNode::~CookieTreeFileSystemsNode() {}

CookieTreeNode::DetailedInfo
CookieTreeFileSystemsNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->GetTitle()).Init(
      DetailedInfo::TYPE_FILE_SYSTEMS);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeServerBoundCertsNode, public:

CookieTreeServerBoundCertsNode::CookieTreeServerBoundCertsNode()
    : CookieTreeNode(
        l10n_util::GetStringUTF16(IDS_COOKIES_SERVER_BOUND_CERTS)) {
}

CookieTreeServerBoundCertsNode::~CookieTreeServerBoundCertsNode() {}

CookieTreeNode::DetailedInfo
CookieTreeServerBoundCertsNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->GetTitle()).Init(
      DetailedInfo::TYPE_SERVER_BOUND_CERTS);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeNode, protected

bool CookieTreeNode::NodeTitleComparator::operator() (
    const CookieTreeNode* lhs, const CookieTreeNode* rhs) {
  const CookieTreeNode* left =
      static_cast<const CookieTreeNode*>(lhs);
  const CookieTreeNode* right =
      static_cast<const CookieTreeNode*>(rhs);
  return (left->GetTitle() < right->GetTitle());
}

void CookieTreeNode::AddChildSortedByTitle(CookieTreeNode* new_child) {
  DCHECK(new_child);
  std::vector<CookieTreeNode*>::iterator iter =
      std::lower_bound(children().begin(),
                       children().end(),
                       new_child,
                       NodeTitleComparator());
  GetModel()->Add(this, new_child, iter - children().begin());
}

///////////////////////////////////////////////////////////////////////////////
// CookiesTreeModel, public:

CookiesTreeModel::CookiesTreeModel(
    BrowsingDataCookieHelper* cookie_helper,
    BrowsingDataDatabaseHelper* database_helper,
    BrowsingDataLocalStorageHelper* local_storage_helper,
    BrowsingDataLocalStorageHelper* session_storage_helper,
    BrowsingDataAppCacheHelper* appcache_helper,
    BrowsingDataIndexedDBHelper* indexed_db_helper,
    BrowsingDataFileSystemHelper* file_system_helper,
    BrowsingDataQuotaHelper* quota_helper,
    BrowsingDataServerBoundCertHelper* server_bound_cert_helper,
    bool use_cookie_source)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui::TreeNodeModel<CookieTreeNode>(
          new CookieTreeRootNode(this))),
      appcache_helper_(appcache_helper),
      cookie_helper_(cookie_helper),
      database_helper_(database_helper),
      local_storage_helper_(local_storage_helper),
      session_storage_helper_(session_storage_helper),
      indexed_db_helper_(indexed_db_helper),
      file_system_helper_(file_system_helper),
      quota_helper_(quota_helper),
      server_bound_cert_helper_(server_bound_cert_helper),
      batch_update_(0),
      use_cookie_source_(use_cookie_source),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DCHECK(cookie_helper_);
  cookie_helper_->StartFetching(
      base::Bind(&CookiesTreeModel::OnCookiesModelInfoLoaded,
                 weak_ptr_factory_.GetWeakPtr()));
  DCHECK(database_helper_);
  database_helper_->StartFetching(
      base::Bind(&CookiesTreeModel::OnDatabaseModelInfoLoaded,
                 weak_ptr_factory_.GetWeakPtr()));
  DCHECK(local_storage_helper_);
  local_storage_helper_->StartFetching(
      base::Bind(&CookiesTreeModel::OnLocalStorageModelInfoLoaded,
                 weak_ptr_factory_.GetWeakPtr()));
  if (session_storage_helper_) {
    session_storage_helper_->StartFetching(
        base::Bind(&CookiesTreeModel::OnSessionStorageModelInfoLoaded,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // TODO(michaeln): When all of the UI implementations have been updated, make
  // this a required parameter.
  if (appcache_helper_) {
    appcache_helper_->StartFetching(
        base::Bind(&CookiesTreeModel::OnAppCacheModelInfoLoaded,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  if (indexed_db_helper_) {
    indexed_db_helper_->StartFetching(
        base::Bind(&CookiesTreeModel::OnIndexedDBModelInfoLoaded,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  if (file_system_helper_) {
    file_system_helper_->StartFetching(
        base::Bind(&CookiesTreeModel::OnFileSystemModelInfoLoaded,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  if (quota_helper_) {
    quota_helper_->StartFetching(
        base::Bind(&CookiesTreeModel::OnQuotaModelInfoLoaded,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  if (server_bound_cert_helper_) {
    server_bound_cert_helper_->StartFetching(
        base::Bind(&CookiesTreeModel::OnServerBoundCertModelInfoLoaded,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

CookiesTreeModel::~CookiesTreeModel() {}

///////////////////////////////////////////////////////////////////////////////
// CookiesTreeModel, TreeModel methods (public):

// TreeModel methods:
// Returns the set of icons for the nodes in the tree. You only need override
// this if you don't want to use the default folder icons.
void CookiesTreeModel::GetIcons(std::vector<gfx::ImageSkia>* icons) {
  icons->push_back(*ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_OMNIBOX_HTTP));
  icons->push_back(*ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_COOKIE_ICON));
  icons->push_back(*ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_COOKIE_STORAGE_ICON));
}

// Returns the index of the icon to use for |node|. Return -1 to use the
// default icon. The index is relative to the list of icons returned from
// GetIcons.
int CookiesTreeModel::GetIconIndex(ui::TreeModelNode* node) {
  CookieTreeNode* ct_node = static_cast<CookieTreeNode*>(node);
  switch (ct_node->GetDetailedInfo().node_type) {
    case CookieTreeNode::DetailedInfo::TYPE_ORIGIN:
      return ORIGIN;
    case CookieTreeNode::DetailedInfo::TYPE_COOKIE:
      return COOKIE;
    case CookieTreeNode::DetailedInfo::TYPE_DATABASE:
      return DATABASE;
    case CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE:
      return DATABASE;  // close enough
    case CookieTreeNode::DetailedInfo::TYPE_SESSION_STORAGE:
      return DATABASE;  // ditto
    case CookieTreeNode::DetailedInfo::TYPE_APPCACHE:
      return DATABASE;  // ditto
    case CookieTreeNode::DetailedInfo::TYPE_INDEXED_DB:
      return DATABASE;  // ditto
    case CookieTreeNode::DetailedInfo::TYPE_FILE_SYSTEM:
      return DATABASE;  // ditto
    case CookieTreeNode::DetailedInfo::TYPE_QUOTA:
      return -1;
    case CookieTreeNode::DetailedInfo::TYPE_SERVER_BOUND_CERT:
      return COOKIE;  // It's kinda like a cookie?
    default:
      break;
  }
  return -1;
}

void CookiesTreeModel::DeleteAllStoredObjects() {
  NotifyObserverBeginBatch();
  CookieTreeNode* root = GetRoot();
  root->DeleteStoredObjects();
  int num_children = root->child_count();
  for (int i = num_children - 1; i >= 0; --i)
    delete Remove(root, root->GetChild(i));
  NotifyObserverTreeNodeChanged(root);
  NotifyObserverEndBatch();
}

void CookiesTreeModel::DeleteCookieNode(CookieTreeNode* cookie_node) {
  if (cookie_node == GetRoot())
    return;
  cookie_node->DeleteStoredObjects();
  CookieTreeNode* parent_node = cookie_node->parent();
  delete Remove(parent_node, cookie_node);
  if (parent_node->empty())
    DeleteCookieNode(parent_node);
}

void CookiesTreeModel::UpdateSearchResults(const std::wstring& filter) {
  CookieTreeNode* root = GetRoot();
  int num_children = root->child_count();
  NotifyObserverBeginBatch();
  for (int i = num_children - 1; i >= 0; --i)
    delete Remove(root, root->GetChild(i));
  PopulateCookieInfoWithFilter(filter);
  PopulateDatabaseInfoWithFilter(filter);
  PopulateLocalStorageInfoWithFilter(filter);
  PopulateSessionStorageInfoWithFilter(filter);
  PopulateAppCacheInfoWithFilter(filter);
  PopulateIndexedDBInfoWithFilter(filter);
  PopulateFileSystemInfoWithFilter(filter);
  PopulateQuotaInfoWithFilter(filter);
  PopulateServerBoundCertInfoWithFilter(filter);
  NotifyObserverTreeNodeChanged(root);
  NotifyObserverEndBatch();
}

void CookiesTreeModel::AddCookiesTreeObserver(Observer* observer) {
  cookies_observer_list_.AddObserver(observer);
  // Call super so that TreeNodeModel can notify, too.
  ui::TreeNodeModel<CookieTreeNode>::AddObserver(observer);
}

void CookiesTreeModel::RemoveCookiesTreeObserver(Observer* observer) {
  cookies_observer_list_.RemoveObserver(observer);
  // Call super so that TreeNodeModel doesn't have dead pointers.
  ui::TreeNodeModel<CookieTreeNode>::RemoveObserver(observer);
}

void CookiesTreeModel::OnAppCacheModelInfoLoaded() {
  using appcache::AppCacheInfo;
  using appcache::AppCacheInfoCollection;
  using appcache::AppCacheInfoVector;
  typedef std::map<GURL, AppCacheInfoVector> InfoByOrigin;

  scoped_refptr<AppCacheInfoCollection> appcache_info =
      appcache_helper_->info_collection();
  if (!appcache_info || appcache_info->infos_by_origin.empty())
    return;

  for (InfoByOrigin::const_iterator origin =
           appcache_info->infos_by_origin.begin();
       origin != appcache_info->infos_by_origin.end(); ++origin) {
    std::list<AppCacheInfo>& info_list = appcache_info_[origin->first];
    info_list.insert(
        info_list.begin(), origin->second.begin(), origin->second.end());
  }

  PopulateAppCacheInfoWithFilter(std::wstring());
}

void CookiesTreeModel::PopulateAppCacheInfoWithFilter(
    const std::wstring& filter) {
  using appcache::AppCacheInfo;
  typedef std::map<GURL, std::list<AppCacheInfo> > InfoByOrigin;

  if (appcache_info_.empty())
    return;

  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());
  NotifyObserverBeginBatch();
  for (InfoByOrigin::iterator origin = appcache_info_.begin();
       origin != appcache_info_.end(); ++origin) {
    std::wstring origin_node_name = UTF8ToWide(origin->first.host());
    if (filter.empty() ||
        (origin_node_name.find(filter) != std::wstring::npos)) {
      CookieTreeOriginNode* origin_node =
          root->GetOrCreateOriginNode(origin->first);
      CookieTreeAppCachesNode* appcaches_node =
          origin_node->GetOrCreateAppCachesNode();

      for (std::list<AppCacheInfo>::iterator info = origin->second.begin();
           info != origin->second.end(); ++info) {
        appcaches_node->AddAppCacheNode(
            new CookieTreeAppCacheNode(origin->first, info));
      }
    }
  }
  NotifyObserverTreeNodeChanged(root);
  NotifyObserverEndBatch();
}

void CookiesTreeModel::OnCookiesModelInfoLoaded(
    const net::CookieList& cookie_list) {
  cookie_list_.insert(cookie_list_.begin(),
                      cookie_list.begin(),
                      cookie_list.end());
  PopulateCookieInfoWithFilter(std::wstring());
}

void CookiesTreeModel::PopulateCookieInfoWithFilter(
    const std::wstring& filter) {
  // mmargh mmargh mmargh! delicious!

  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());
  NotifyObserverBeginBatch();
  for (CookieList::iterator it = cookie_list_.begin();
       it != cookie_list_.end(); ++it) {
    std::string source_string = it->Source();
    if (source_string.empty() || !use_cookie_source_) {
      std::string domain = it->Domain();
      if (domain.length() > 1 && domain[0] == '.')
        domain = domain.substr(1);

      // We treat secure cookies just the same as normal ones.
      source_string = std::string(chrome::kHttpScheme) +
          content::kStandardSchemeSeparator + domain + "/";
    }

    GURL source(source_string);
    if (!filter.size() ||
        (CookieTreeOriginNode::TitleForUrl(source).find(filter) !=
         std::string::npos)) {
      CookieTreeOriginNode* origin_node =
          root->GetOrCreateOriginNode(source);
      CookieTreeCookiesNode* cookies_node =
          origin_node->GetOrCreateCookiesNode();
      CookieTreeCookieNode* new_cookie = new CookieTreeCookieNode(it);
      cookies_node->AddCookieNode(new_cookie);
    }
  }
  NotifyObserverTreeNodeChanged(root);
  NotifyObserverEndBatch();
}

void CookiesTreeModel::OnDatabaseModelInfoLoaded(
    const DatabaseInfoList& database_info) {
  database_info_list_ = database_info;
  PopulateDatabaseInfoWithFilter(std::wstring());
}

void CookiesTreeModel::PopulateDatabaseInfoWithFilter(
    const std::wstring& filter) {
  if (database_info_list_.empty())
    return;
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());
  NotifyObserverBeginBatch();
  for (DatabaseInfoList::iterator database_info = database_info_list_.begin();
       database_info != database_info_list_.end();
       ++database_info) {
    GURL origin(database_info->origin);

    if (!filter.size() ||
        (CookieTreeOriginNode::TitleForUrl(origin).find(filter) !=
         std::wstring::npos)) {
      CookieTreeOriginNode* origin_node =
          root->GetOrCreateOriginNode(origin);
      CookieTreeDatabasesNode* databases_node =
          origin_node->GetOrCreateDatabasesNode();
      databases_node->AddDatabaseNode(
          new CookieTreeDatabaseNode(database_info));
    }
  }
  NotifyObserverTreeNodeChanged(root);
  NotifyObserverEndBatch();
}

void CookiesTreeModel::OnLocalStorageModelInfoLoaded(
    const LocalStorageInfoList& local_storage_info) {
  local_storage_info_list_ = local_storage_info;
  PopulateLocalStorageInfoWithFilter(std::wstring());
}

void CookiesTreeModel::PopulateLocalStorageInfoWithFilter(
    const std::wstring& filter) {
  if (local_storage_info_list_.empty())
    return;
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());
  NotifyObserverBeginBatch();
  for (LocalStorageInfoList::iterator local_storage_info =
       local_storage_info_list_.begin();
       local_storage_info != local_storage_info_list_.end();
       ++local_storage_info) {
    const GURL& origin(local_storage_info->origin_url);

    if (!filter.size() ||
        (CookieTreeOriginNode::TitleForUrl(origin).find(filter) !=
         std::wstring::npos)) {
      CookieTreeOriginNode* origin_node =
          root->GetOrCreateOriginNode(origin);
      CookieTreeLocalStoragesNode* local_storages_node =
          origin_node->GetOrCreateLocalStoragesNode();
      local_storages_node->AddLocalStorageNode(
          new CookieTreeLocalStorageNode(local_storage_info));
    }
  }
  NotifyObserverTreeNodeChanged(root);
  NotifyObserverEndBatch();
}

void CookiesTreeModel::OnSessionStorageModelInfoLoaded(
    const LocalStorageInfoList& session_storage_info) {
  session_storage_info_list_ = session_storage_info;
  PopulateSessionStorageInfoWithFilter(std::wstring());
}

void CookiesTreeModel::PopulateSessionStorageInfoWithFilter(
    const std::wstring& filter) {
  if (session_storage_info_list_.empty())
    return;
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());
  NotifyObserverBeginBatch();
  for (LocalStorageInfoList::iterator session_storage_info =
       session_storage_info_list_.begin();
       session_storage_info != session_storage_info_list_.end();
       ++session_storage_info) {
    const GURL& origin = session_storage_info->origin_url;
    if (!filter.size() ||
        (CookieTreeOriginNode::TitleForUrl(origin).find(filter) !=
         std::wstring::npos)) {
      CookieTreeOriginNode* origin_node =
          root->GetOrCreateOriginNode(origin);
      CookieTreeSessionStoragesNode* session_storages_node =
          origin_node->GetOrCreateSessionStoragesNode();
      session_storages_node->AddSessionStorageNode(
          new CookieTreeSessionStorageNode(session_storage_info));
    }
  }
  NotifyObserverTreeNodeChanged(root);
  NotifyObserverEndBatch();
}

void CookiesTreeModel::OnIndexedDBModelInfoLoaded(
    const IndexedDBInfoList& indexed_db_info) {
  indexed_db_info_list_ = indexed_db_info;
  PopulateIndexedDBInfoWithFilter(std::wstring());
}

void CookiesTreeModel::PopulateIndexedDBInfoWithFilter(
    const std::wstring& filter) {
  if (indexed_db_info_list_.empty())
    return;
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());
  NotifyObserverBeginBatch();
  for (IndexedDBInfoList::iterator indexed_db_info =
       indexed_db_info_list_.begin();
       indexed_db_info != indexed_db_info_list_.end();
       ++indexed_db_info) {
    const GURL& origin = indexed_db_info->origin;

    if (!filter.size() ||
        (CookieTreeOriginNode::TitleForUrl(origin).find(filter) !=
         std::wstring::npos)) {
      CookieTreeOriginNode* origin_node =
          root->GetOrCreateOriginNode(origin);
      CookieTreeIndexedDBsNode* indexed_dbs_node =
          origin_node->GetOrCreateIndexedDBsNode();
      indexed_dbs_node->AddIndexedDBNode(
          new CookieTreeIndexedDBNode(indexed_db_info));
    }
  }
  NotifyObserverTreeNodeChanged(root);
  NotifyObserverEndBatch();
}

void CookiesTreeModel::OnFileSystemModelInfoLoaded(
    const FileSystemInfoList& file_system_info) {
  file_system_info_list_ = file_system_info;
  PopulateFileSystemInfoWithFilter(std::wstring());
}

void CookiesTreeModel::PopulateFileSystemInfoWithFilter(
    const std::wstring& filter) {
  if (file_system_info_list_.empty())
    return;
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());
  NotifyObserverBeginBatch();
  for (FileSystemInfoList::iterator file_system_info =
       file_system_info_list_.begin();
       file_system_info != file_system_info_list_.end();
       ++file_system_info) {
    GURL origin(file_system_info->origin);

    if (!filter.size() ||
        (CookieTreeOriginNode::TitleForUrl(origin).find(filter) !=
         std::wstring::npos)) {
      CookieTreeOriginNode* origin_node =
          root->GetOrCreateOriginNode(origin);
      CookieTreeFileSystemsNode* file_systems_node =
          origin_node->GetOrCreateFileSystemsNode();
      file_systems_node->AddFileSystemNode(
          new CookieTreeFileSystemNode(file_system_info));
    }
  }
  NotifyObserverTreeNodeChanged(root);
  NotifyObserverEndBatch();
}

void CookiesTreeModel::OnQuotaModelInfoLoaded(
    const QuotaInfoArray& quota_info) {
  quota_info_list_ = quota_info;
  PopulateQuotaInfoWithFilter(std::wstring());
}

void CookiesTreeModel::PopulateQuotaInfoWithFilter(
    const std::wstring& filter) {
  if (quota_info_list_.empty())
    return;
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());
  NotifyObserverBeginBatch();
  for (QuotaInfoArray::iterator quota_info = quota_info_list_.begin();
       quota_info != quota_info_list_.end();
       ++quota_info) {
    if (!filter.size() ||
        (UTF8ToWide(quota_info->host).find(filter) != std::wstring::npos)) {
      CookieTreeOriginNode* origin_node =
          root->GetOrCreateOriginNode(GURL("http://" + quota_info->host));
      origin_node->UpdateOrCreateQuotaNode(quota_info);
    }
  }
  NotifyObserverTreeNodeChanged(root);
  NotifyObserverEndBatch();
}

void CookiesTreeModel::OnServerBoundCertModelInfoLoaded(
    const ServerBoundCertList& cert_list) {
  server_bound_cert_list_ = cert_list;
  PopulateServerBoundCertInfoWithFilter(std::wstring());
}

void CookiesTreeModel::PopulateServerBoundCertInfoWithFilter(
    const std::wstring& filter) {
  if (server_bound_cert_list_.empty())
    return;
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());
  NotifyObserverBeginBatch();
  for (ServerBoundCertList::iterator cert_info =
       server_bound_cert_list_.begin();
       cert_info != server_bound_cert_list_.end();
       ++cert_info) {
    GURL origin(cert_info->server_identifier());
    if (!origin.is_valid()) {
      // Domain Bound Cert.  Make a valid URL to satisfy the
      // CookieTreeRootNode::GetOrCreateOriginNode interface.
      origin = GURL(std::string(chrome::kHttpsScheme) +
                    content::kStandardSchemeSeparator +
                    cert_info->server_identifier() + "/");
    }
    std::wstring title = CookieTreeOriginNode::TitleForUrl(origin);

    if (!filter.size() || title.find(filter) != std::wstring::npos) {
      CookieTreeOriginNode* origin_node =
          root->GetOrCreateOriginNode(origin);
      CookieTreeServerBoundCertsNode* server_bound_certs_node =
          origin_node->GetOrCreateServerBoundCertsNode();
      server_bound_certs_node->AddServerBoundCertNode(
          new CookieTreeServerBoundCertNode(cert_info));
    }
  }
  NotifyObserverTreeNodeChanged(root);
  NotifyObserverEndBatch();
}

void CookiesTreeModel::NotifyObserverBeginBatch() {
  // Only notify the model once if we're batching in a nested manner.
  if (batch_update_++ == 0) {
    FOR_EACH_OBSERVER(Observer,
                      cookies_observer_list_,
                      TreeModelBeginBatch(this));
  }
}

void CookiesTreeModel::NotifyObserverEndBatch() {
  // Only notify the observers if this is the outermost call to EndBatch() if
  // called in a nested manner.
  if (--batch_update_ == 0) {
    FOR_EACH_OBSERVER(Observer,
                      cookies_observer_list_,
                      TreeModelEndBatch(this));
  }
}
