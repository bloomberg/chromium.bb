// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_SCRIPT_CACHE_MAP_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_SCRIPT_CACHE_MAP_H_

#include <map>

#include "base/basictypes.h"
#include "base/observer_list.h"

class GURL;

namespace content {

class ServiceWorkerVersion;

// Class that maintains the mapping between urls and a resource id
// for a particular versions implicit script resources.
class ServiceWorkerScriptCacheMap {
 public:
  class Observer {
   public:
    // Notification that the main script resource has been written to
    // the disk cache. Only called when a version is being initially
    // installed.
    virtual void OnMainScriptCached(ServiceWorkerVersion* version,
                                    bool success) = 0;

    // Notification that the main script resource and all imports have
    // been written to the disk cache. Only called when a version is
    // being initially installed.
    virtual void OnAllScriptsCached(ServiceWorkerVersion* version,
                                    bool success) = 0;
  };

  int64 Lookup(const GURL& url);

  // Adds and removes Observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Used during the initial run of a new version to build the map
  // of resources ids.
  // TODO(michaeln): Need more info about errors in Finished.
  void NotifyStartedCaching(const GURL& url, int64 resource_id);
  void NotifyFinishedCaching(const GURL& url, bool success);
  void NotifyEvalCompletion();

 private:
  typedef std::map<GURL, int64> ResourceIDMap;

  // The version objects owns its script cache and provides a rawptr to it.
  friend class ServiceWorkerVersion;
  explicit ServiceWorkerScriptCacheMap(ServiceWorkerVersion* owner);
  ~ServiceWorkerScriptCacheMap();

  ServiceWorkerVersion* owner_;
  ResourceIDMap resource_ids_;

  // Members used only during initial install.
  bool is_eval_complete_;
  int resources_started_;
  int resources_finished_;
  bool has_error_;
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerScriptCacheMap);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_SCRIPT_CACHE_MAP_H_
