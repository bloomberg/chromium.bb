// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_STORAGE_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_STORAGE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/appcache/appcache_working_set.h"
#include "content/common/content_export.h"
#include "net/base/completion_callback.h"

class GURL;

namespace content {
FORWARD_DECLARE_TEST(AppCacheStorageTest, DelegateReferences);
FORWARD_DECLARE_TEST(AppCacheStorageTest, UsageMap);
class AppCache;
class AppCacheEntry;
class AppCacheGroup;
class AppCacheQuotaClientTest;
class AppCacheResponseMetadataWriter;
class AppCacheResponseReader;
class AppCacheResponseTest;
class AppCacheResponseWriter;
class AppCacheServiceImpl;
class AppCacheStorageTest;
struct AppCacheInfoCollection;
struct HttpResponseInfoIOBuffer;

class CONTENT_EXPORT AppCacheStorage {
 public:
  typedef std::map<GURL, int64_t> UsageMap;

  class CONTENT_EXPORT Delegate {
   public:
    // If retrieval fails, 'collection' will be NULL.
    virtual void OnAllInfo(AppCacheInfoCollection* collection) {}

    // If a load fails the 'cache' will be NULL.
    virtual void OnCacheLoaded(AppCache* cache, int64_t cache_id) {}

    // If a load fails the 'group' will be NULL.
    virtual void OnGroupLoaded(
        AppCacheGroup* group, const GURL& manifest_url) {}

    // If successfully stored 'success' will be true.
    virtual void OnGroupAndNewestCacheStored(
        AppCacheGroup* group, AppCache* newest_cache, bool success,
        bool would_exceed_quota) {}

    // If the operation fails, success will be false.
    virtual void OnGroupMadeObsolete(AppCacheGroup* group,
                                     bool success,
                                     int response_code) {}

    // If a load fails the 'response_info' will be NULL.
    virtual void OnResponseInfoLoaded(AppCacheResponseInfo* response_info,
                                      int64_t response_id) {}

    // If no response is found, entry.response_id() and
    // fallback_entry.response_id() will be kAppCacheNoResponseId.
    // If the response is the entry for an intercept or fallback
    // namespace, the url of the namespece entry is returned.
    // If a response is found, the cache id and manifest url of the
    // containing cache and group are also returned.
    virtual void OnMainResponseFound(const GURL& url,
                                     const AppCacheEntry& entry,
                                     const GURL& namespace_entry_url,
                                     const AppCacheEntry& fallback_entry,
                                     int64_t cache_id,
                                     int64_t group_id,
                                     const GURL& mainfest_url) {}

   protected:
    virtual ~Delegate() {}
  };

  explicit AppCacheStorage(AppCacheServiceImpl* service);
  virtual ~AppCacheStorage();

  // Schedules a task to retrieve basic info about all groups and caches
  // stored in the system. Upon completion the delegate will be called
  // with the results.
  virtual void GetAllInfo(Delegate* delegate) = 0;

  // Schedules a cache to be loaded from storage. Upon load completion
  // the delegate will be called back. If the cache already resides in
  // memory, the delegate will be called back immediately without returning
  // to the message loop. If the load fails, the delegate will be called
  // back with a NULL cache pointer.
  virtual void LoadCache(int64_t id, Delegate* delegate) = 0;

  // Schedules a group and its newest cache, if any, to be loaded from storage.
  // Upon load completion the delegate will be called back. If the group
  // and newest cache already reside in memory, the delegate will be called
  // back immediately without returning to the message loop. If the load fails,
  // the delegate will be called back with a NULL group pointer.
  virtual void LoadOrCreateGroup(
      const GURL& manifest_url, Delegate* delegate) = 0;

  // Schedules response info to be loaded from storage.
  // Upon load completion the delegate will be called back. If the data
  // already resides in memory, the delegate will be called back
  // immediately without returning to the message loop. If the load fails,
  // the delegate will be called back with a NULL pointer.
  virtual void LoadResponseInfo(const GURL& manifest_url,
                                int64_t response_id,
                                Delegate* delegate);

  // Schedules a group and its newest complete cache to be initially stored or
  // incrementally updated with new changes. Upon completion the delegate
  // will be called back. A group without a newest cache cannot be stored.
  // It's a programming error to call this method without a newest cache. A
  // side effect of storing a new newest cache is the removal of the group's
  // old caches and responses from persistent storage (although they may still
  // linger in the in-memory working set until no longer needed). The new
  // cache will be added as the group's newest complete cache only if storage
  // succeeds.
  virtual void StoreGroupAndNewestCache(
      AppCacheGroup* group, AppCache* newest_cache, Delegate* delegate) = 0;

  // Schedules a query to identify a response for a main request. Upon
  // completion the delegate will be called back.
  virtual void FindResponseForMainRequest(
      const GURL& url,
      const GURL& preferred_manifest_url,
      Delegate* delegate) = 0;

  // Performs an immediate lookup of the in-memory cache to
  // identify a response for a sub resource request.
  virtual void FindResponseForSubRequest(
      AppCache* cache, const GURL& url,
      AppCacheEntry* found_entry, AppCacheEntry* found_fallback_entry,
      bool* found_network_namespace) = 0;

  // Immediately updates in-memory storage, if the cache is in memory,
  // and schedules a task to update persistent storage. If the cache is
  // already scheduled to be loaded, upon loading completion the entry
  // will be marked. There is no delegate completion callback.
  virtual void MarkEntryAsForeign(const GURL& entry_url, int64_t cache_id) = 0;

  // Schedules a task to update persistent storage and doom the group and all
  // related caches and responses for deletion. Upon completion the in-memory
  // instance is marked as obsolete and the delegate callback is called.
  virtual void MakeGroupObsolete(AppCacheGroup* group,
                                 Delegate* delegate,
                                 int response_code) = 0;

  // Schedules a task to update persistent storage with the times of the first
  // evictable error and last successful full update check.
  virtual void StoreEvictionTimes(AppCacheGroup* group) = 0;

  // Cancels all pending callbacks for the delegate. The delegate callbacks
  // will not be invoked after, however any scheduled operations will still
  // take place. The callbacks for subsequently scheduled operations are
  // unaffected.
  void CancelDelegateCallbacks(Delegate* delegate) {
    DelegateReference* delegate_reference = GetDelegateReference(delegate);
    if (delegate_reference)
      delegate_reference->CancelReference();
  }

  // Creates a reader to read a response from storage.
  virtual AppCacheResponseReader* CreateResponseReader(const GURL& manifest_url,
                                                       int64_t response_id) = 0;

  // Creates a writer to write a new response to storage. This call
  // establishes a new response id.
  virtual AppCacheResponseWriter* CreateResponseWriter(
      const GURL& manifest_url) = 0;

  // Creates a metadata writer to write metadata of response to storage.
  virtual AppCacheResponseMetadataWriter* CreateResponseMetadataWriter(
      int64_t response_id) = 0;

  // Schedules the lazy deletion of responses and saves the ids
  // persistently such that the responses will be deleted upon restart
  // if they aren't deleted prior to shutdown.
  virtual void DoomResponses(const GURL& manifest_url,
                             const std::vector<int64_t>& response_ids) = 0;

  // Schedules the lazy deletion of responses without persistently saving
  // the response ids.
  virtual void DeleteResponses(const GURL& manifest_url,
                               const std::vector<int64_t>& response_ids) = 0;

  // Returns true if the AppCacheStorage instance is initialized.
  virtual bool IsInitialized() = 0;

  // Generates unique storage ids for different object types.
  int64_t NewCacheId() { return ++last_cache_id_; }
  int64_t NewGroupId() { return ++last_group_id_; }

  // The working set of object instances currently in memory.
  AppCacheWorkingSet* working_set() { return &working_set_; }

  // A map of origins to usage.
  const UsageMap* usage_map() { return &usage_map_; }

  // Simple ptr back to the service object that owns us.
  AppCacheServiceImpl* service() { return service_; }

  // Returns a weak pointer reference to the AppCacheStorage instance.
  base::WeakPtr<AppCacheStorage> GetWeakPtr();

 protected:
  friend class content::AppCacheQuotaClientTest;
  friend class content::AppCacheResponseTest;
  friend class content::AppCacheStorageTest;

  // Helper to call a collection of delegates.
  #define FOR_EACH_DELEGATE(delegates, func_and_args)                \
    do {                                                             \
      for (DelegateReferenceVector::iterator it = delegates.begin(); \
           it != delegates.end(); ++it) {                            \
        if (it->get()->delegate)                                     \
          it->get()->delegate->func_and_args;                        \
      }                                                              \
    } while (0)

  // Helper used to manage multiple references to a 'delegate' and to
  // allow all pending callbacks to that delegate to be easily cancelled.
  struct CONTENT_EXPORT DelegateReference :
      public base::RefCounted<DelegateReference> {
    Delegate* delegate;
    AppCacheStorage* storage;

    DelegateReference(Delegate* delegate, AppCacheStorage* storage);

    void CancelReference() {
      storage->delegate_references_.erase(delegate);
      storage = NULL;
      delegate = NULL;
    }

   private:
    friend class base::RefCounted<DelegateReference>;

    virtual ~DelegateReference();
  };
  typedef std::map<Delegate*, DelegateReference*> DelegateReferenceMap;
  typedef std::vector<scoped_refptr<DelegateReference> >
      DelegateReferenceVector;

  // Helper used to manage an async LoadResponseInfo calls on behalf of
  // multiple callers.
  class ResponseInfoLoadTask {
   public:
    ResponseInfoLoadTask(const GURL& manifest_url,
                         int64_t response_id,
                         AppCacheStorage* storage);
    ~ResponseInfoLoadTask();

    int64_t response_id() const { return response_id_; }
    const GURL& manifest_url() const { return manifest_url_; }

    void AddDelegate(DelegateReference* delegate_reference) {
      delegates_.push_back(delegate_reference);
    }

    void StartIfNeeded();

   private:
    void OnReadComplete(int result);

    AppCacheStorage* storage_;
    GURL manifest_url_;
    int64_t response_id_;
    std::unique_ptr<AppCacheResponseReader> reader_;
    DelegateReferenceVector delegates_;
    scoped_refptr<HttpResponseInfoIOBuffer> info_buffer_;
  };

  DelegateReference* GetDelegateReference(Delegate* delegate) {
    DelegateReferenceMap::iterator iter =
        delegate_references_.find(delegate);
    if (iter != delegate_references_.end())
      return iter->second;
    return NULL;
  }

  DelegateReference* GetOrCreateDelegateReference(Delegate* delegate) {
    DelegateReference* reference = GetDelegateReference(delegate);
    if (reference)
      return reference;
    return new DelegateReference(delegate, this);
  }

  ResponseInfoLoadTask* GetOrCreateResponseInfoLoadTask(
      const GURL& manifest_url,
      int64_t response_id) {
    auto iter = pending_info_loads_.find(response_id);
    if (iter != pending_info_loads_.end())
      return iter->second.get();
    return new ResponseInfoLoadTask(manifest_url, response_id, this);
  }

  // Should only be called when creating a new response writer.
  int64_t NewResponseId() { return ++last_response_id_; }

  // Helpers to query and notify the QuotaManager.
  void UpdateUsageMapAndNotify(const GURL& origin, int64_t new_usage);
  void ClearUsageMapAndNotify();
  void NotifyStorageAccessed(const GURL& origin);

  // The last storage id used for different object types.
  int64_t last_cache_id_;
  int64_t last_group_id_;
  int64_t last_response_id_;

  UsageMap usage_map_;  // maps origin to usage
  AppCacheWorkingSet working_set_;
  AppCacheServiceImpl* service_;
  DelegateReferenceMap delegate_references_;

  // Note that the ResponseInfoLoadTask items add themselves to this map.
  std::map<int64_t, std::unique_ptr<ResponseInfoLoadTask>> pending_info_loads_;

  // The set of last ids must be retrieved from storage prior to being used.
  static const int64_t kUnitializedId;

  FRIEND_TEST_ALL_PREFIXES(content::AppCacheStorageTest, DelegateReferences);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheStorageTest, UsageMap);

  // The WeakPtrFactory below must occur last in the class definition so they
  // get destroyed last.
  base::WeakPtrFactory<AppCacheStorage> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheStorage);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_STORAGE_H_
