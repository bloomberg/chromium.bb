// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_SCRIPT_CACHE_MAP_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_SCRIPT_CACHE_MAP_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "content/browser/service_worker/service_worker_database.h"

class GURL;

namespace content {

class ServiceWorkerVersion;

// Class that maintains the mapping between urls and a resource id
// for a particular versions implicit script resources.
class ServiceWorkerScriptCacheMap {
 public:
  int64 Lookup(const GURL& url);

  // Used during the initial run of a new version to build the map
  // of resources ids.
  void NotifyStartedCaching(const GURL& url, int64 resource_id);
  void NotifyFinishedCaching(const GURL& url, bool success);

  // Used to retrieve the results of the initial run of a new version.
  bool HasError() const { return has_error_; }
  void GetResources(
      std::vector<ServiceWorkerDatabase::ResourceRecord>* resources);

  // Used when loading an existing version.
  void SetResources(
     const std::vector<ServiceWorkerDatabase::ResourceRecord>& resources);

 private:
  typedef std::map<GURL, int64> ResourceIDMap;

  // The version objects owns its script cache and provides a rawptr to it.
  friend class ServiceWorkerVersion;
  explicit ServiceWorkerScriptCacheMap(ServiceWorkerVersion* owner);
  ~ServiceWorkerScriptCacheMap();

  ServiceWorkerVersion* owner_;
  ResourceIDMap resource_ids_;
  bool has_error_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerScriptCacheMap);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_SCRIPT_CACHE_MAP_H_
