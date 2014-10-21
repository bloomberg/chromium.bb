// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CACHE_MAP_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CACHE_MAP_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_database.h"
#include "content/common/content_export.h"
#include "net/url_request/url_request_status.h"

class GURL;

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerVersion;

// Class that maintains the mapping between urls and a resource id
// for a particular version's implicit script resources.
class CONTENT_EXPORT ServiceWorkerScriptCacheMap {
 public:
  int64 LookupResourceId(const GURL& url);
  // A size of -1 means that we don't know the size yet
  // (it has not finished caching).
  int64 LookupResourceSize(const GURL& url);

  // Used during the initial run of a new version to build the map
  // of resources ids.
  void NotifyStartedCaching(const GURL& url, int64 resource_id);
  void NotifyFinishedCaching(const GURL& url,
                             int64 size_bytes,
                             const net::URLRequestStatus& status);

  // Used to retrieve the results of the initial run of a new version.
  void GetResources(
      std::vector<ServiceWorkerDatabase::ResourceRecord>* resources);

  // Used when loading an existing version.
  void SetResources(
     const std::vector<ServiceWorkerDatabase::ResourceRecord>& resources);

  size_t size() const { return resource_map_.size(); }

  const net::URLRequestStatus& main_script_status() const {
    return main_script_status_;
  }

 private:
  typedef std::map<GURL, ServiceWorkerDatabase::ResourceRecord> ResourceMap;

  // The version objects owns its script cache and provides a rawptr to it.
  friend class ServiceWorkerVersion;
  ServiceWorkerScriptCacheMap(
      ServiceWorkerVersion* owner,
      base::WeakPtr<ServiceWorkerContextCore> context);
  ~ServiceWorkerScriptCacheMap();

  ServiceWorkerVersion* owner_;
  base::WeakPtr<ServiceWorkerContextCore> context_;
  ResourceMap resource_map_;
  net::URLRequestStatus main_script_status_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerScriptCacheMap);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CACHE_MAP_H_
