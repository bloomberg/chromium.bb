// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/local_shared_objects_container.h"

#include "chrome/browser/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data_cookie_helper.h"
#include "chrome/browser/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data_file_system_helper.h"
#include "chrome/browser/browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "chrome/browser/browsing_data_server_bound_cert_helper.h"
#include "chrome/browser/profiles/profile.h"

LocalSharedObjectsContainer::LocalSharedObjectsContainer(Profile* profile)
    : appcaches_(new CannedBrowsingDataAppCacheHelper(profile)),
      cookies_(new CannedBrowsingDataCookieHelper(profile)),
      databases_(new CannedBrowsingDataDatabaseHelper(profile)),
      file_systems_(new CannedBrowsingDataFileSystemHelper(profile)),
      indexed_dbs_(new CannedBrowsingDataIndexedDBHelper()),
      local_storages_(new CannedBrowsingDataLocalStorageHelper(profile)),
      server_bound_certs_(new CannedBrowsingDataServerBoundCertHelper()),
      session_storages_(new CannedBrowsingDataLocalStorageHelper(profile)) {
}

LocalSharedObjectsContainer::~LocalSharedObjectsContainer() {
}

void LocalSharedObjectsContainer::Reset() {
  appcaches_->Reset();
  cookies_->Reset();
  databases_->Reset();
  file_systems_->Reset();
  indexed_dbs_->Reset();
  local_storages_->Reset();
  server_bound_certs_->Reset();
  session_storages_->Reset();
}

bool LocalSharedObjectsContainer::IsEmpty() const {
  return appcaches_->empty() &&
      cookies_->empty() &&
      databases_->empty() &&
      file_systems_->empty() &&
      indexed_dbs_->empty() &&
      local_storages_->empty() &&
      server_bound_certs_->empty() &&
      session_storages_->empty();
}
