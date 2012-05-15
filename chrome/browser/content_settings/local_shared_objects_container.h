// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_LOCAL_SHARED_OBJECTS_CONTAINER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_LOCAL_SHARED_OBJECTS_CONTAINER_H_
#pragma once

#include "base/memory/ref_counted.h"

class CannedBrowsingDataAppCacheHelper;
class CannedBrowsingDataCookieHelper;
class CannedBrowsingDataDatabaseHelper;
class CannedBrowsingDataFileSystemHelper;
class CannedBrowsingDataIndexedDBHelper;
class CannedBrowsingDataLocalStorageHelper;
class CannedBrowsingDataServerBoundCertHelper;
class GURL;
class Profile;

class LocalSharedObjectsContainer {
 public:
  explicit LocalSharedObjectsContainer(Profile* profile);
  ~LocalSharedObjectsContainer();

  // Empties the container.
  void Reset();

  // Returns the number of objects stored in the container.
  size_t GetObjectCount() const;

  // Returns the number of objects for the given |origin|.
  size_t GetObjectCountForDomain(const GURL& url) const;

  CannedBrowsingDataAppCacheHelper* appcaches() const {
    return appcaches_;
  }
  CannedBrowsingDataCookieHelper* cookies() const {
    return cookies_;
  }
  CannedBrowsingDataDatabaseHelper* databases() const {
    return databases_;
  }
  CannedBrowsingDataFileSystemHelper* file_systems() const {
    return file_systems_;
  }
  CannedBrowsingDataIndexedDBHelper* indexed_dbs() const {
    return indexed_dbs_;
  }
  CannedBrowsingDataLocalStorageHelper* local_storages() const {
    return local_storages_;
  }
  CannedBrowsingDataServerBoundCertHelper* server_bound_certs() const {
    return server_bound_certs_;
  }
  CannedBrowsingDataLocalStorageHelper* session_storages() const {
    return session_storages_;
  }

 private:
  scoped_refptr<CannedBrowsingDataAppCacheHelper> appcaches_;
  scoped_refptr<CannedBrowsingDataCookieHelper> cookies_;
  scoped_refptr<CannedBrowsingDataDatabaseHelper> databases_;
  scoped_refptr<CannedBrowsingDataFileSystemHelper> file_systems_;
  scoped_refptr<CannedBrowsingDataIndexedDBHelper> indexed_dbs_;
  scoped_refptr<CannedBrowsingDataLocalStorageHelper> local_storages_;
  scoped_refptr<CannedBrowsingDataServerBoundCertHelper> server_bound_certs_;
  scoped_refptr<CannedBrowsingDataLocalStorageHelper> session_storages_;

  DISALLOW_COPY_AND_ASSIGN(LocalSharedObjectsContainer);
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_LOCAL_SHARED_OBJECTS_CONTAINER_H_
