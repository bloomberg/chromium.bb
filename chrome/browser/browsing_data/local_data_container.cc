// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/local_data_container.h"

#include "base/bind.h"
#include "base/memory/linked_ptr.h"
#include "chrome/browser/browsing_data/browsing_data_flash_lso_helper.h"
#include "chrome/browser/browsing_data/browsing_data_server_bound_cert_helper.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "net/cookies/canonical_cookie.h"

///////////////////////////////////////////////////////////////////////////////
// LocalDataContainer, public:

LocalDataContainer::LocalDataContainer(
    BrowsingDataCookieHelper* cookie_helper,
    BrowsingDataDatabaseHelper* database_helper,
    BrowsingDataLocalStorageHelper* local_storage_helper,
    BrowsingDataLocalStorageHelper* session_storage_helper,
    BrowsingDataAppCacheHelper* appcache_helper,
    BrowsingDataIndexedDBHelper* indexed_db_helper,
    BrowsingDataFileSystemHelper* file_system_helper,
    BrowsingDataQuotaHelper* quota_helper,
    BrowsingDataServerBoundCertHelper* server_bound_cert_helper,
    BrowsingDataFlashLSOHelper* flash_lso_helper)
    : appcache_helper_(appcache_helper),
      cookie_helper_(cookie_helper),
      database_helper_(database_helper),
      local_storage_helper_(local_storage_helper),
      session_storage_helper_(session_storage_helper),
      indexed_db_helper_(indexed_db_helper),
      file_system_helper_(file_system_helper),
      quota_helper_(quota_helper),
      server_bound_cert_helper_(server_bound_cert_helper),
      flash_lso_helper_(flash_lso_helper),
      model_(NULL),
      weak_ptr_factory_(this) {}

LocalDataContainer::~LocalDataContainer() {}

void LocalDataContainer::Init(CookiesTreeModel* model) {
  DCHECK(!model_);
  model_ = model;

  DCHECK(cookie_helper_.get());
  cookie_helper_->StartFetching(
      base::Bind(&LocalDataContainer::OnCookiesModelInfoLoaded,
                 weak_ptr_factory_.GetWeakPtr()));

  if (database_helper_.get()) {
    database_helper_->StartFetching(
        base::Bind(&LocalDataContainer::OnDatabaseModelInfoLoaded,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  if (local_storage_helper_.get()) {
    local_storage_helper_->StartFetching(
        base::Bind(&LocalDataContainer::OnLocalStorageModelInfoLoaded,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  if (session_storage_helper_.get()) {
    session_storage_helper_->StartFetching(
        base::Bind(&LocalDataContainer::OnSessionStorageModelInfoLoaded,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // TODO(michaeln): When all of the UI implementations have been updated, make
  // this a required parameter.
  if (appcache_helper_.get()) {
    appcache_helper_->StartFetching(
        base::Bind(&LocalDataContainer::OnAppCacheModelInfoLoaded,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  if (indexed_db_helper_.get()) {
    indexed_db_helper_->StartFetching(
        base::Bind(&LocalDataContainer::OnIndexedDBModelInfoLoaded,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  if (file_system_helper_.get()) {
    file_system_helper_->StartFetching(
        base::Bind(&LocalDataContainer::OnFileSystemModelInfoLoaded,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  if (quota_helper_.get()) {
    quota_helper_->StartFetching(
        base::Bind(&LocalDataContainer::OnQuotaModelInfoLoaded,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  if (server_bound_cert_helper_.get()) {
    server_bound_cert_helper_->StartFetching(
        base::Bind(&LocalDataContainer::OnServerBoundCertModelInfoLoaded,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  if (flash_lso_helper_.get()) {
    flash_lso_helper_->StartFetching(
        base::Bind(&LocalDataContainer::OnFlashLSOInfoLoaded,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void LocalDataContainer::OnAppCacheModelInfoLoaded() {
  using content::AppCacheInfo;
  using content::AppCacheInfoCollection;
  using content::AppCacheInfoVector;
  typedef std::map<GURL, AppCacheInfoVector> InfoByOrigin;

  scoped_refptr<AppCacheInfoCollection> appcache_info =
      appcache_helper_->info_collection();
  if (!appcache_info.get() || appcache_info->infos_by_origin.empty())
    return;

  for (InfoByOrigin::const_iterator origin =
           appcache_info->infos_by_origin.begin();
       origin != appcache_info->infos_by_origin.end(); ++origin) {
    std::list<AppCacheInfo>& info_list = appcache_info_[origin->first];
    info_list.insert(
        info_list.begin(), origin->second.begin(), origin->second.end());
  }

  model_->PopulateAppCacheInfo(this);
}

void LocalDataContainer::OnCookiesModelInfoLoaded(
    const net::CookieList& cookie_list) {
  cookie_list_.insert(cookie_list_.begin(),
                      cookie_list.begin(),
                      cookie_list.end());
  DCHECK(model_);
  model_->PopulateCookieInfo(this);
}

void LocalDataContainer::OnDatabaseModelInfoLoaded(
    const DatabaseInfoList& database_info) {
  database_info_list_ = database_info;
  DCHECK(model_);
  model_->PopulateDatabaseInfo(this);
}

void LocalDataContainer::OnLocalStorageModelInfoLoaded(
    const LocalStorageInfoList& local_storage_info) {
  local_storage_info_list_ = local_storage_info;
  DCHECK(model_);
  model_->PopulateLocalStorageInfo(this);
}

void LocalDataContainer::OnSessionStorageModelInfoLoaded(
    const LocalStorageInfoList& session_storage_info) {
  session_storage_info_list_ = session_storage_info;
  DCHECK(model_);
  model_->PopulateSessionStorageInfo(this);
}

void LocalDataContainer::OnIndexedDBModelInfoLoaded(
    const IndexedDBInfoList& indexed_db_info) {
  indexed_db_info_list_ = indexed_db_info;
  DCHECK(model_);
  model_->PopulateIndexedDBInfo(this);
}

void LocalDataContainer::OnFileSystemModelInfoLoaded(
    const FileSystemInfoList& file_system_info) {
  file_system_info_list_ = file_system_info;
  DCHECK(model_);
  model_->PopulateFileSystemInfo(this);
}

void LocalDataContainer::OnQuotaModelInfoLoaded(
    const QuotaInfoList& quota_info) {
  quota_info_list_ = quota_info;
  DCHECK(model_);
  model_->PopulateQuotaInfo(this);
}

void LocalDataContainer::OnServerBoundCertModelInfoLoaded(
    const ServerBoundCertList& cert_list) {
  server_bound_cert_list_ = cert_list;
  DCHECK(model_);
  model_->PopulateServerBoundCertInfo(this);
}

void LocalDataContainer::OnFlashLSOInfoLoaded(
    const FlashLSODomainList& domains) {
  flash_lso_domain_list_ = domains;
  DCHECK(model_);
  model_->PopulateFlashLSOInfo(this);
}
