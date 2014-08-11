// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/local_shared_objects_container.h"

#include "chrome/browser/browsing_data/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data/browsing_data_channel_id_helper.h"
#include "chrome/browser/browsing_data/browsing_data_cookie_helper.h"
#include "chrome/browser/browsing_data/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data/browsing_data_file_system_helper.h"
#include "chrome/browser/browsing_data/browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"
#include "chrome/browser/browsing_data/browsing_data_service_worker_helper.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "url/gurl.h"

namespace {

bool SameDomainOrHost(const GURL& gurl1, const GURL& gurl2) {
  return net::registry_controlled_domains::SameDomainOrHost(
      gurl1,
      gurl2,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

}  // namespace

LocalSharedObjectsContainer::LocalSharedObjectsContainer(Profile* profile)
    : appcaches_(new CannedBrowsingDataAppCacheHelper(profile)),
      channel_ids_(new CannedBrowsingDataChannelIDHelper()),
      cookies_(new CannedBrowsingDataCookieHelper(
          profile->GetRequestContext())),
      databases_(new CannedBrowsingDataDatabaseHelper(profile)),
      file_systems_(new CannedBrowsingDataFileSystemHelper(profile)),
      indexed_dbs_(new CannedBrowsingDataIndexedDBHelper(
          content::BrowserContext::GetDefaultStoragePartition(profile)->
              GetIndexedDBContext())),
      local_storages_(new CannedBrowsingDataLocalStorageHelper(profile)),
      service_workers_(new CannedBrowsingDataServiceWorkerHelper(
          content::BrowserContext::GetDefaultStoragePartition(profile)->
              GetServiceWorkerContext())),
      session_storages_(new CannedBrowsingDataLocalStorageHelper(profile)) {
}

LocalSharedObjectsContainer::~LocalSharedObjectsContainer() {
}

void LocalSharedObjectsContainer::Reset() {
  appcaches_->Reset();
  channel_ids_->Reset();
  cookies_->Reset();
  databases_->Reset();
  file_systems_->Reset();
  indexed_dbs_->Reset();
  local_storages_->Reset();
  service_workers_->Reset();
  session_storages_->Reset();
}

size_t LocalSharedObjectsContainer::GetObjectCount() const {
  size_t count = 0;
  count += appcaches()->GetAppCacheCount();
  count += channel_ids()->GetChannelIDCount();
  count += cookies()->GetCookieCount();
  count += databases()->GetDatabaseCount();
  count += file_systems()->GetFileSystemCount();
  count += indexed_dbs()->GetIndexedDBCount();
  count += local_storages()->GetLocalStorageCount();
  count += service_workers()->GetServiceWorkerCount();
  count += session_storages()->GetLocalStorageCount();
  return count;
}

size_t LocalSharedObjectsContainer::GetObjectCountForDomain(
    const GURL& origin) const {
  size_t count = 0;

  // Count all cookies that have the same domain as the provided |origin|. This
  // means count all cookies that has been set by a host that is not considered
  // to be a third party regarding the domain of the provided |origin|.
  // E.g. if the origin is "http://foo.com" then all cookies with domain foo.com,
  // a.foo.com, b.a.foo.com or *.foo.com will be counted.
  typedef CannedBrowsingDataCookieHelper::OriginCookieListMap
      OriginCookieListMap;
  const OriginCookieListMap& origin_cookies_list_map =
      cookies()->origin_cookie_list_map();
  for (OriginCookieListMap::const_iterator it =
          origin_cookies_list_map.begin();
      it != origin_cookies_list_map.end();
      ++it) {
    const net::CookieList* cookie_list = it->second;
    for (net::CookieList::const_iterator cookie = cookie_list->begin();
         cookie != cookie_list->end();
         ++cookie) {
      // Strip leading '.'s.
      std::string cookie_domain = cookie->Domain();
      if (cookie_domain[0] == '.')
        cookie_domain = cookie_domain.substr(1);
      // The |domain_url| is only created in order to use the
      // SameDomainOrHost method below. It does not matter which scheme is
      // used as the scheme is ignored by the SameDomainOrHost method.
      GURL domain_url(std::string(url::kHttpScheme) +
                      url::kStandardSchemeSeparator + cookie_domain);
      if (SameDomainOrHost(origin, domain_url))
        ++count;
    }
  }

  // Count local storages for the domain of the given |origin|.
  const std::set<GURL> local_storage_info =
      local_storages()->GetLocalStorageInfo();
  for (std::set<GURL>::const_iterator it = local_storage_info.begin();
       it != local_storage_info.end();
       ++it) {
    if (SameDomainOrHost(origin, *it))
      ++count;
  }

  // Count session storages for the domain of the given |origin|.
  const std::set<GURL> urls = session_storages()->GetLocalStorageInfo();
  for (std::set<GURL>::const_iterator it = urls.begin();
       it != urls.end();
       ++it) {
    if (SameDomainOrHost(origin, *it))
      ++count;
  }

  // Count indexed dbs for the domain of the given |origin|.
  typedef CannedBrowsingDataIndexedDBHelper::PendingIndexedDBInfo IndexedDBInfo;
  const std::set<IndexedDBInfo>& indexed_db_info =
      indexed_dbs()->GetIndexedDBInfo();
  for (std::set<IndexedDBInfo>::const_iterator it =
          indexed_db_info.begin();
      it != indexed_db_info.end();
      ++it) {
    if (SameDomainOrHost(origin, it->origin))
      ++count;
  }

  // Count service workers for the domain of the given |origin|.
  typedef CannedBrowsingDataServiceWorkerHelper::PendingServiceWorkerUsageInfo
      ServiceWorkerInfo;
  const std::set<ServiceWorkerInfo>& service_worker_info =
      service_workers()->GetServiceWorkerUsageInfo();
  for (std::set<ServiceWorkerInfo>::const_iterator it =
          service_worker_info.begin();
      it != service_worker_info.end();
      ++it) {
    if (SameDomainOrHost(origin, it->origin))
      ++count;
  }

  // Count filesystems for the domain of the given |origin|.
  typedef BrowsingDataFileSystemHelper::FileSystemInfo FileSystemInfo;
  typedef std::list<FileSystemInfo> FileSystemInfoList;
  const FileSystemInfoList& file_system_info =
      file_systems()->GetFileSystemInfo();
  for (FileSystemInfoList::const_iterator it = file_system_info.begin();
       it != file_system_info.end();
       ++it) {
    if (SameDomainOrHost(origin, it->origin))
      ++count;
  }

  // Count databases for the domain of the given |origin|.
  typedef CannedBrowsingDataDatabaseHelper::PendingDatabaseInfo DatabaseInfo;
  const std::set<DatabaseInfo>& database_list =
      databases()->GetPendingDatabaseInfo();
  for (std::set<DatabaseInfo>::const_iterator it =
          database_list.begin();
      it != database_list.end();
      ++it) {
    if (SameDomainOrHost(origin, it->origin))
      ++count;
  }

  // Count the AppCache manifest files for the domain of the given |origin|.
  typedef BrowsingDataAppCacheHelper::OriginAppCacheInfoMap
      OriginAppCacheInfoMap;
  const OriginAppCacheInfoMap& map = appcaches()->GetOriginAppCacheInfoMap();
  for (OriginAppCacheInfoMap::const_iterator it = map.begin();
       it != map.end();
       ++it) {
    const content::AppCacheInfoVector& info_vector = it->second;
    for (content::AppCacheInfoVector::const_iterator info =
             info_vector.begin();
         info != info_vector.end();
         ++info) {
       if (SameDomainOrHost(origin, info->manifest_url))
         ++count;
    }
  }

  return count;
}

scoped_ptr<CookiesTreeModel>
LocalSharedObjectsContainer::CreateCookiesTreeModel() const {
  LocalDataContainer* container = new LocalDataContainer(
      cookies(),
      databases(),
      local_storages(),
      session_storages(),
      appcaches(),
      indexed_dbs(),
      file_systems(),
      NULL,
      channel_ids(),
      service_workers(),
      NULL);

  return make_scoped_ptr(new CookiesTreeModel(container, NULL, true));
}
