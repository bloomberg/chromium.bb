// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cookies_tree_model.h"

#include <algorithm>
#include <functional>
#include <vector>

#include "base/callback.h"
#include "base/linked_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/cookie_monster.h"
#include "net/base/registry_controlled_domain.h"
#include "net/url_request/url_request_context.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

static const char kFileOriginNodeName[] = "file://";

///////////////////////////////////////////////////////////////////////////////
// CookieTreeNode, public:

void CookieTreeNode::DeleteStoredObjects() {
  std::for_each(children().begin(),
                children().end(),
                std::mem_fun(&CookieTreeNode::DeleteStoredObjects));
}

CookiesTreeModel* CookieTreeNode::GetModel() const {
  if (GetParent())
    return GetParent()->GetModel();
  else
    return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeCookieNode, public:

CookieTreeCookieNode::CookieTreeCookieNode(
    net::CookieMonster::CanonicalCookie* cookie)
    : CookieTreeNode(UTF8ToUTF16(cookie->Name())),
      cookie_(cookie) {
}

CookieTreeCookieNode::~CookieTreeCookieNode() {}

void CookieTreeCookieNode::DeleteStoredObjects() {
  // notify CookieMonster that we should delete this cookie
  // We have stored a copy of all the cookies in the model, and our model is
  // never re-calculated. Thus, we just need to delete the nodes from our
  // model, and tell CookieMonster to delete the cookies. We can keep the
  // vector storing the cookies in-tact and not delete from there (that would
  // invalidate our pointers), and the fact that it contains semi out-of-date
  // data is not problematic as we don't re-build the model based on that.
  GetModel()->cookie_monster_->DeleteCanonicalCookie(*cookie_);
}

CookieTreeNode::DetailedInfo CookieTreeCookieNode::GetDetailedInfo() const {
  return DetailedInfo(GetParent()->GetParent()->GetTitle(),
                      DetailedInfo::TYPE_COOKIE,
                      cookie_, NULL, NULL, NULL, NULL, NULL);
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
    const appcache::AppCacheInfo* appcache_info)
    : CookieTreeNode(UTF8ToUTF16(appcache_info->manifest_url.spec())),
      appcache_info_(appcache_info) {
}

void CookieTreeAppCacheNode::DeleteStoredObjects() {
  DCHECK(GetModel()->appcache_helper_);
  GetModel()->appcache_helper_->DeleteAppCacheGroup(
      appcache_info_->manifest_url);
}

CookieTreeNode::DetailedInfo CookieTreeAppCacheNode::GetDetailedInfo() const {
  return DetailedInfo(GetParent()->GetParent()->GetTitle(),
                      DetailedInfo::TYPE_APPCACHE,
                      NULL, NULL, NULL, NULL, appcache_info_, NULL);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeDatabaseNode, public:

CookieTreeDatabaseNode::CookieTreeDatabaseNode(
    BrowsingDataDatabaseHelper::DatabaseInfo* database_info)
    : CookieTreeNode(database_info->database_name.empty() ?
          l10n_util::GetStringUTF16(IDS_COOKIES_WEB_DATABASE_UNNAMED_NAME) :
          UTF8ToUTF16(database_info->database_name)),
      database_info_(database_info) {
}

CookieTreeDatabaseNode::~CookieTreeDatabaseNode() {}

void CookieTreeDatabaseNode::DeleteStoredObjects() {
  GetModel()->database_helper_->DeleteDatabase(
      database_info_->origin_identifier, database_info_->database_name);
}

CookieTreeNode::DetailedInfo CookieTreeDatabaseNode::GetDetailedInfo() const {
  return DetailedInfo(GetParent()->GetParent()->GetTitle(),
                      DetailedInfo::TYPE_DATABASE,
                      NULL, database_info_, NULL, NULL, NULL, NULL);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeLocalStorageNode, public:

CookieTreeLocalStorageNode::CookieTreeLocalStorageNode(
    BrowsingDataLocalStorageHelper::LocalStorageInfo* local_storage_info)
    : CookieTreeNode(UTF8ToUTF16(
          local_storage_info->origin.empty() ?
              local_storage_info->database_identifier :
              local_storage_info->origin)),
      local_storage_info_(local_storage_info) {
}

CookieTreeLocalStorageNode::~CookieTreeLocalStorageNode() {}

void CookieTreeLocalStorageNode::DeleteStoredObjects() {
  GetModel()->local_storage_helper_->DeleteLocalStorageFile(
      local_storage_info_->file_path);
}

CookieTreeNode::DetailedInfo
CookieTreeLocalStorageNode::GetDetailedInfo() const {
  return DetailedInfo(GetParent()->GetParent()->GetTitle(),
                      DetailedInfo::TYPE_LOCAL_STORAGE,
                      NULL, NULL, local_storage_info_, NULL, NULL, NULL);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeSessionStorageNode, public:

CookieTreeSessionStorageNode::CookieTreeSessionStorageNode(
    BrowsingDataLocalStorageHelper::LocalStorageInfo* session_storage_info)
    : CookieTreeNode(UTF8ToUTF16(
          session_storage_info->origin.empty() ?
              session_storage_info->database_identifier :
              session_storage_info->origin)),
      session_storage_info_(session_storage_info) {
}

CookieTreeSessionStorageNode::~CookieTreeSessionStorageNode() {}

CookieTreeNode::DetailedInfo
CookieTreeSessionStorageNode::GetDetailedInfo() const {
  return DetailedInfo(GetParent()->GetParent()->GetTitle(),
                      DetailedInfo::TYPE_SESSION_STORAGE,
                      NULL, NULL, NULL, session_storage_info_, NULL, NULL);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeIndexedDBNode, public:

CookieTreeIndexedDBNode::CookieTreeIndexedDBNode(
    BrowsingDataIndexedDBHelper::IndexedDBInfo* indexed_db_info)
    : CookieTreeNode(UTF8ToUTF16(
          indexed_db_info->origin.empty() ?
              indexed_db_info->database_identifier :
              indexed_db_info->origin)),
      indexed_db_info_(indexed_db_info) {
}

CookieTreeIndexedDBNode::~CookieTreeIndexedDBNode() {}

void CookieTreeIndexedDBNode::DeleteStoredObjects() {
  GetModel()->indexed_db_helper_->DeleteIndexedDBFile(
      indexed_db_info_->file_path);
}

CookieTreeNode::DetailedInfo CookieTreeIndexedDBNode::GetDetailedInfo() const {
  return DetailedInfo(GetParent()->GetParent()->GetTitle(),
                      DetailedInfo::TYPE_INDEXED_DB,
                      NULL, NULL, NULL, NULL, NULL, indexed_db_info_);
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
      lower_bound(children().begin(),
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
  model_->Add(this, (origin_node_iterator - children().begin()), retval);
  return retval;
}

CookiesTreeModel* CookieTreeRootNode::GetModel() const {
  return model_;
}

CookieTreeNode::DetailedInfo CookieTreeRootNode::GetDetailedInfo() const {
  return DetailedInfo(string16(),
                      DetailedInfo::TYPE_ROOT,
                      NULL, NULL, NULL, NULL, NULL, NULL);
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
      url_(url) {}

CookieTreeOriginNode::~CookieTreeOriginNode() {}

CookieTreeNode::DetailedInfo CookieTreeOriginNode::GetDetailedInfo() const {
  return DetailedInfo(GetTitle(),
                      DetailedInfo::TYPE_ORIGIN,
                      NULL, NULL, NULL, NULL, NULL, NULL);
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

void CookieTreeOriginNode::CreateContentException(
    HostContentSettingsMap* content_settings, ContentSetting setting) const {
  if (CanCreateContentException()) {
    content_settings->AddExceptionForURL(url_,
                                         CONTENT_SETTINGS_TYPE_COOKIES,
                                         "",
                                         setting);
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
  return DetailedInfo(GetParent()->GetTitle(),
                      DetailedInfo::TYPE_COOKIES,
                      NULL, NULL, NULL, NULL, NULL, NULL);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeAppCachesNode, public:

CookieTreeAppCachesNode::CookieTreeAppCachesNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(
                         IDS_COOKIES_APPLICATION_CACHES)) {
}

CookieTreeAppCachesNode::~CookieTreeAppCachesNode() {}

CookieTreeNode::DetailedInfo CookieTreeAppCachesNode::GetDetailedInfo() const {
  return DetailedInfo(GetParent()->GetTitle(),
                      DetailedInfo::TYPE_APPCACHES,
                      NULL, NULL, NULL, NULL, NULL, NULL);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeDatabasesNode, public:

CookieTreeDatabasesNode::CookieTreeDatabasesNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_WEB_DATABASES)) {
}

CookieTreeDatabasesNode::~CookieTreeDatabasesNode() {}

CookieTreeNode::DetailedInfo CookieTreeDatabasesNode::GetDetailedInfo() const {
  return DetailedInfo(GetParent()->GetTitle(),
                      DetailedInfo::TYPE_DATABASES,
                      NULL, NULL, NULL, NULL, NULL, NULL);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeLocalStoragesNode, public:

CookieTreeLocalStoragesNode::CookieTreeLocalStoragesNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_LOCAL_STORAGE)) {
}

CookieTreeLocalStoragesNode::~CookieTreeLocalStoragesNode() {}

CookieTreeNode::DetailedInfo
CookieTreeLocalStoragesNode::GetDetailedInfo() const {
  return DetailedInfo(GetParent()->GetTitle(),
                      DetailedInfo::TYPE_LOCAL_STORAGES,
                      NULL, NULL, NULL, NULL, NULL, NULL);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeSessionStoragesNode, public:

CookieTreeSessionStoragesNode::CookieTreeSessionStoragesNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_SESSION_STORAGE)) {
}

CookieTreeSessionStoragesNode::~CookieTreeSessionStoragesNode() {}

CookieTreeNode::DetailedInfo
CookieTreeSessionStoragesNode::GetDetailedInfo() const {
  return DetailedInfo(GetParent()->GetTitle(),
                      DetailedInfo::TYPE_SESSION_STORAGES,
                      NULL, NULL, NULL, NULL, NULL, NULL);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeIndexedDBsNode, public:

CookieTreeIndexedDBsNode::CookieTreeIndexedDBsNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_INDEXED_DBS)) {
}

CookieTreeIndexedDBsNode::~CookieTreeIndexedDBsNode() {}

CookieTreeNode::DetailedInfo
CookieTreeIndexedDBsNode::GetDetailedInfo() const {
  return DetailedInfo(GetParent()->GetTitle(),
                      DetailedInfo::TYPE_INDEXED_DBS,
                      NULL, NULL, NULL, NULL, NULL, NULL);
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
  std::vector<CookieTreeNode*>::iterator iter =
      lower_bound(children().begin(),
                  children().end(),
                  new_child,
                  NodeTitleComparator());
  GetModel()->Add(this, iter - children().begin(), new_child);
}

///////////////////////////////////////////////////////////////////////////////
// CookiesTreeModel, public:

CookiesTreeModel::CookiesTreeModel(
    net::CookieMonster* cookie_monster,
    BrowsingDataDatabaseHelper* database_helper,
    BrowsingDataLocalStorageHelper* local_storage_helper,
    BrowsingDataLocalStorageHelper* session_storage_helper,
    BrowsingDataAppCacheHelper* appcache_helper,
    BrowsingDataIndexedDBHelper* indexed_db_helper)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui::TreeNodeModel<CookieTreeNode>(
          new CookieTreeRootNode(this))),
      cookie_monster_(cookie_monster),
      appcache_helper_(appcache_helper),
      database_helper_(database_helper),
      local_storage_helper_(local_storage_helper),
      session_storage_helper_(session_storage_helper),
      indexed_db_helper_(indexed_db_helper),
      batch_update_(0) {
  LoadCookies();
  DCHECK(database_helper_);
  database_helper_->StartFetching(NewCallback(
      this, &CookiesTreeModel::OnDatabaseModelInfoLoaded));
  DCHECK(local_storage_helper_);
  local_storage_helper_->StartFetching(NewCallback(
      this, &CookiesTreeModel::OnLocalStorageModelInfoLoaded));
  if (session_storage_helper_) {
    session_storage_helper_->StartFetching(NewCallback(
        this, &CookiesTreeModel::OnSessionStorageModelInfoLoaded));
  }

  // TODO(michaeln): when all of the ui impls have been updated,
  // make this a required parameter.
  if (appcache_helper_) {
    appcache_helper_->StartFetching(NewCallback(
        this, &CookiesTreeModel::OnAppCacheModelInfoLoaded));
  }

  if (indexed_db_helper_) {
    indexed_db_helper_->StartFetching(NewCallback(
        this, &CookiesTreeModel::OnIndexedDBModelInfoLoaded));
  }
}

CookiesTreeModel::~CookiesTreeModel() {
  database_helper_->CancelNotification();
  local_storage_helper_->CancelNotification();
  if (session_storage_helper_)
    session_storage_helper_->CancelNotification();
  if (appcache_helper_)
    appcache_helper_->CancelNotification();
  if (indexed_db_helper_)
    indexed_db_helper_->CancelNotification();
}

///////////////////////////////////////////////////////////////////////////////
// CookiesTreeModel, TreeModel methods (public):

// TreeModel methods:
// Returns the set of icons for the nodes in the tree. You only need override
// this if you don't want to use the default folder icons.
void CookiesTreeModel::GetIcons(std::vector<SkBitmap>* icons) {
  icons->push_back(*ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_DEFAULT_FAVICON));
  icons->push_back(*ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_COOKIE_ICON));
  icons->push_back(*ResourceBundle::GetSharedInstance().GetBitmapNamed(
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
    default:
      break;
  }
  return -1;
}

void CookiesTreeModel::LoadCookies() {
  LoadCookiesWithFilter(std::wstring());
}

void CookiesTreeModel::LoadCookiesWithFilter(const std::wstring& filter) {
  // mmargh mmargh mmargh! delicious!

  all_cookies_ = cookie_monster_->GetAllCookies();
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());
  for (CookieList::iterator it = all_cookies_.begin();
       it != all_cookies_.end(); ++it) {
    std::string source_string = it->Source();
    if (source_string.empty()) {
      std::string domain = it->Domain();
      if (domain.length() > 1 && domain[0] == '.')
        domain = domain.substr(1);

      // We treat secure cookies just the same as normal ones.
      source_string = std::string(chrome::kHttpScheme) +
          chrome::kStandardSchemeSeparator + domain + "/";
    }

    GURL source(source_string);
    if (!filter.size() ||
        (CookieTreeOriginNode::TitleForUrl(source).find(filter) !=
         std::string::npos)) {
      CookieTreeOriginNode* origin_node =
          root->GetOrCreateOriginNode(source);
      CookieTreeCookiesNode* cookies_node =
          origin_node->GetOrCreateCookiesNode();
      CookieTreeCookieNode* new_cookie = new CookieTreeCookieNode(&*it);
      cookies_node->AddCookieNode(new_cookie);
    }
  }
}

void CookiesTreeModel::DeleteAllStoredObjects() {
  NotifyObserverBeginBatch();
  CookieTreeNode* root = GetRoot();
  root->DeleteStoredObjects();
  int num_children = root->GetChildCount();
  for (int i = num_children - 1; i >= 0; --i)
    delete Remove(root, i);
  NotifyObserverTreeNodeChanged(root);
  NotifyObserverEndBatch();
}

void CookiesTreeModel::DeleteCookieNode(CookieTreeNode* cookie_node) {
  if (cookie_node == GetRoot())
    return;
  cookie_node->DeleteStoredObjects();
  // find the parent and index
  CookieTreeNode* parent_node = cookie_node->GetParent();
  int cookie_node_index = parent_node->GetIndexOf(cookie_node);
  delete Remove(parent_node, cookie_node_index);
  if (parent_node->GetChildCount() == 0)
    DeleteCookieNode(parent_node);
}

void CookiesTreeModel::UpdateSearchResults(const std::wstring& filter) {
  CookieTreeNode* root = GetRoot();
  int num_children = root->GetChildCount();
  NotifyObserverBeginBatch();
  for (int i = num_children - 1; i >= 0; --i)
    delete Remove(root, i);
  LoadCookiesWithFilter(filter);
  PopulateDatabaseInfoWithFilter(filter);
  PopulateLocalStorageInfoWithFilter(filter);
  PopulateSessionStorageInfoWithFilter(filter);
  PopulateAppCacheInfoWithFilter(filter);
  PopulateIndexedDBInfoWithFilter(filter);
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
  appcache_info_ = appcache_helper_->info_collection();
  PopulateAppCacheInfoWithFilter(std::wstring());
}

void CookiesTreeModel::PopulateAppCacheInfoWithFilter(
    const std::wstring& filter) {
  using appcache::AppCacheInfo;
  using appcache::AppCacheInfoVector;
  typedef std::map<GURL, AppCacheInfoVector> InfoByOrigin;

  if (!appcache_info_ || appcache_info_->infos_by_origin.empty())
    return;

  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());
  NotifyObserverBeginBatch();
  for (InfoByOrigin::const_iterator origin =
           appcache_info_->infos_by_origin.begin();
       origin != appcache_info_->infos_by_origin.end(); ++origin) {
    std::wstring origin_node_name = UTF8ToWide(origin->first.host());
    if (filter.empty() ||
        (origin_node_name.find(filter) != std::wstring::npos)) {
      CookieTreeOriginNode* origin_node =
          root->GetOrCreateOriginNode(origin->first);
      CookieTreeAppCachesNode* appcaches_node =
          origin_node->GetOrCreateAppCachesNode();

      for (AppCacheInfoVector::const_iterator info = origin->second.begin();
           info != origin->second.end(); ++info) {
        appcaches_node->AddAppCacheNode(
            new CookieTreeAppCacheNode(&(*info)));
      }
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
          new CookieTreeDatabaseNode(&(*database_info)));
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
    GURL origin(local_storage_info->origin);

    if (!filter.size() ||
        (CookieTreeOriginNode::TitleForUrl(origin).find(filter) !=
         std::wstring::npos)) {
      CookieTreeOriginNode* origin_node =
          root->GetOrCreateOriginNode(origin);
      CookieTreeLocalStoragesNode* local_storages_node =
          origin_node->GetOrCreateLocalStoragesNode();
      local_storages_node->AddLocalStorageNode(
          new CookieTreeLocalStorageNode(&(*local_storage_info)));
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
    GURL origin(session_storage_info->origin);

    if (!filter.size() ||
        (CookieTreeOriginNode::TitleForUrl(origin).find(filter) !=
         std::wstring::npos)) {
      CookieTreeOriginNode* origin_node =
          root->GetOrCreateOriginNode(origin);
      CookieTreeSessionStoragesNode* session_storages_node =
          origin_node->GetOrCreateSessionStoragesNode();
      session_storages_node->AddSessionStorageNode(
          new CookieTreeSessionStorageNode(&(*session_storage_info)));
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
    GURL origin(indexed_db_info->origin);

    if (!filter.size() ||
        (CookieTreeOriginNode::TitleForUrl(origin).find(filter) !=
         std::wstring::npos)) {
      CookieTreeOriginNode* origin_node =
          root->GetOrCreateOriginNode(origin);
      CookieTreeIndexedDBsNode* indexed_dbs_node =
          origin_node->GetOrCreateIndexedDBsNode();
      indexed_dbs_node->AddIndexedDBNode(
          new CookieTreeIndexedDBNode(&(*indexed_db_info)));
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
