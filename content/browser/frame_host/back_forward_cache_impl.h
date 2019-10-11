// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_BACK_FORWARD_CACHE_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_BACK_FORWARD_CACHE_IMPL_H_

#include <list>
#include <memory>
#include <unordered_map>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/frame_host/back_forward_cache_metrics.h"
#include "content/common/content_export.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/global_routing_id.h"
#include "url/gurl.h"

namespace content {

class RenderFrameHostImpl;
class RenderFrameProxyHost;
class RenderViewHostImpl;

// BackForwardCache:
//
// After the user navigates away from a document, the old one goes into the
// frozen state and is kept in this object. They can potentially be reused
// after an history navigation. Reusing a document means swapping it back with
// the current_frame_host.
class CONTENT_EXPORT BackForwardCacheImpl : public BackForwardCache {
 public:
  struct Entry {
    using RenderFrameProxyHostMap =
        std::unordered_map<int32_t /* SiteInstance ID */,
                           std::unique_ptr<RenderFrameProxyHost>>;

    Entry(std::unique_ptr<RenderFrameHostImpl> rfh,
          RenderFrameProxyHostMap proxy_hosts,
          std::set<RenderViewHostImpl*> render_view_hosts);
    ~Entry();

    // The main document being stored.
    std::unique_ptr<RenderFrameHostImpl> render_frame_host;

    // Proxies of the main document as seen by other processes.
    // Currently, we only store proxies for SiteInstances of all subframes on
    // the page, because pages using window.open and nested WebContents are not
    // cached.
    RenderFrameProxyHostMap proxy_hosts;

    // RenderViewHosts belonging to the main frame, and its proxies (if any).
    //
    // While RenderViewHostImpl(s) are in the BackForwardCache, they aren't
    // reused for pages outside the cache. This prevents us from having two main
    // frames, (one in the cache, one live), associated with a single
    // RenderViewHost.
    //
    // Keeping these here also prevents RenderFrameHostManager code from
    // unwittingly iterating over RenderViewHostImpls that are in the cache.
    std::set<RenderViewHostImpl*> render_view_hosts;

    DISALLOW_COPY_AND_ASSIGN(Entry);
  };

  BackForwardCacheImpl();
  ~BackForwardCacheImpl();

  struct CanStoreDocumentResult {
    CanStoreDocumentResult(const CanStoreDocumentResult&);
    ~CanStoreDocumentResult();

    bool can_store;
    base::Optional<BackForwardCacheMetrics::CanNotStoreDocumentReason> reason;
    uint64_t blocklisted_features;

    static CanStoreDocumentResult Yes();
    static CanStoreDocumentResult No(
        BackForwardCacheMetrics::CanNotStoreDocumentReason reason);
    static CanStoreDocumentResult NoDueToFeatures(uint64_t features);

    std::string ToString();

    operator bool() const { return can_store; }

   private:
    CanStoreDocumentResult(
        bool can_store,
        base::Optional<BackForwardCacheMetrics::CanNotStoreDocumentReason>
            reason,
        uint64_t blocklisted_features);
  };

  // Returns whether a RenderFrameHost can be stored into the
  // BackForwardCache. Depends on the |render_frame_host| and its children's
  // state.
  CanStoreDocumentResult CanStoreDocument(
      RenderFrameHostImpl* render_frame_host);

  // Moves the specified BackForwardCache entry into the BackForwardCache. It
  // can be reused in a future history navigation by using RestoreEntry(). When
  // the BackForwardCache is full, the least recently used document is evicted.
  // Precondition: CanStoreDocument(*(entry->render_frame_host)).
  void StoreEntry(std::unique_ptr<Entry> entry);

  // Iterates over all the RenderViewHost inside |main_rfh| and freeze or
  // resume them.
  static void Freeze(RenderFrameHostImpl* main_rfh);
  static void Resume(RenderFrameHostImpl* main_rfh);

  // Returns a pointer to a cached BackForwardCache entry matching
  // |navigation_entry_id| if it exists in the BackForwardCache. Returns nullptr
  // if no matching entry is found.
  //
  // Note: The returned pointer should be used temporarily only within the
  // execution of a single task on the event loop. Beyond that, there is no
  // guarantee the pointer will be valid, because the document may be
  // removed/evicted from the cache.
  Entry* GetEntry(int navigation_entry_id);

  // During a history navigation, moves an entry out of the BackForwardCache
  // knowing its |navigation_entry_id|. Returns nullptr when none is found.
  std::unique_ptr<Entry> RestoreEntry(int navigation_entry_id);

  // Remove all entries from the BackForwardCache.
  void Flush();

  // Posts a task to destroy all frames in the BackForwardCache that have been
  // marked as evicted.
  void PostTaskToDestroyEvictedFrames();

  // Storing frames in back-forward cache is not supported indefinitely
  // due to potential privacy issues and memory leaks. Instead we are evicting
  // the frame from the cache after the time to live, which can be controlled
  // via experiment.
  static base::TimeDelta GetTimeToLiveInBackForwardCache();

  // Returns the task runner that should be used by the eviction timer.
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() {
    return task_runner_for_testing_ ? task_runner_for_testing_
                                    : base::ThreadTaskRunnerHandle::Get();
  }

  // Inject task runner for precise timing control in browser tests.
  void SetTaskRunnerForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    task_runner_for_testing_ = task_runner;
  }

  // Sets the number of documents that can be stored in the cache. This is meant
  // for use from within tests only.
  // If |cache_size_limit_for_testing| is 0 (the default), the normal cache
  // size limit will be used.
  void set_cache_size_limit_for_testing(size_t cache_size_limit_for_testing) {
    cache_size_limit_for_testing_ = cache_size_limit_for_testing;
  }

  void DisableForTesting(DisableForTestingReason reason) override;

 private:
  // Destroys all evicted frames in the BackForwardCache.
  void DestroyEvictedFrames();

  // Helper for recursively checking each child.
  CanStoreDocumentResult CanStoreRenderFrameHost(
      RenderFrameHostImpl* render_frame_host,
      uint64_t disallowed_features);

  // Checks if the url's host and path matches with the |allowed_urls_| host and
  // path. This is controlled by "allowed_websites" param on BackForwardCache
  // feature and if the param is not set, it will allow all websites by default.
  bool IsAllowed(const GURL& current_url);

  // Contains the set of stored Entries.
  // Invariant:
  // - Ordered from the most recently used to the last recently used.
  // - Once the list is full, the least recently used document is evicted.
  std::list<std::unique_ptr<Entry>> entries_;

  // Only used in tests. Whether the BackforwardCached has been disabled for
  // testing.
  bool is_disabled_for_testing_ = false;

  // Only used in tests. If non-zero, this value will be used as the cache size
  // limit.
  size_t cache_size_limit_for_testing_ = 0;

  // Only used for tests. This task runner is used for precise injection in
  // browser tests and for timing control.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_for_testing_;

  // To enter the back-forward cache, the main document URL's must match one of
  // the field trial parameter "allowed_websites". This is represented here by a
  // set of host and path prefix.
  std::map<std::string,              // URL's host,
           std::vector<std::string>  // URL's path prefix
           >
      allowed_urls_;

  base::WeakPtrFactory<BackForwardCacheImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackForwardCacheImpl);
};

// Allow external code to be notified when back-forward cache is disabled for a
// RenderFrameHost. This should be used only by the testing infrastructure which
// want to know the exact reason why the cache was disabled. There can be only
// one observer.
class CONTENT_EXPORT BackForwardCacheTestDelegate {
 public:
  BackForwardCacheTestDelegate();
  virtual ~BackForwardCacheTestDelegate();

  virtual void OnDisabledForFrameWithReason(GlobalFrameRoutingId id,
                                            base::StringPiece reason) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_BACK_FORWARD_CACHE_IMPL_H_
