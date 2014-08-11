// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_SERVICE_WORKER_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_SERVICE_WORKER_HELPER_H_

#include <list>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_usage_info.h"
#include "url/gurl.h"

class Profile;

// BrowsingDataServiceWorkerHelper is an interface for classes dealing with
// aggregating and deleting browsing data stored for Service Workers -
// registrations, scripts, and caches.
// A client of this class need to call StartFetching from the UI thread to
// initiate the flow, and it'll be notified by the callback in its UI thread at
// some later point.
class BrowsingDataServiceWorkerHelper
    : public base::RefCountedThreadSafe<BrowsingDataServiceWorkerHelper> {
 public:
  // Create a BrowsingDataServiceWorkerHelper instance for the Service Workers
  // stored in |context|'s associated profile's user data directory.
  explicit BrowsingDataServiceWorkerHelper(
      content::ServiceWorkerContext* context);

  // Starts the fetching process, which will notify its completion via
  // |callback|. This must be called only in the UI thread.
  virtual void StartFetching(const base::Callback<
      void(const std::list<content::ServiceWorkerUsageInfo>&)>& callback);
  // Requests the Service Worker data for an origin be deleted.
  virtual void DeleteServiceWorkers(const GURL& origin);

 protected:
  virtual ~BrowsingDataServiceWorkerHelper();

  // Owned by the profile.
  content::ServiceWorkerContext* service_worker_context_;

  // Access to |service_worker_info_| is triggered indirectly via the UI
  // thread and guarded by |is_fetching_|. This means |service_worker_info_|
  // is only accessed while |is_fetching_| is true. The flag |is_fetching_| is
  // only accessed on the UI thread.
  // In the context of this class |service_worker_info_| is only accessed on the
  // context's ServiceWorker thread.
  std::list<content::ServiceWorkerUsageInfo> service_worker_info_;

  // This member is only mutated on the UI thread.
  base::Callback<void(const std::list<content::ServiceWorkerUsageInfo>&)>
      completion_callback_;

  // Indicates whether or not we're currently fetching information:
  // it's true when StartFetching() is called in the UI thread, and it's reset
  // after we notified the callback in the UI thread.
  // This only mutates on the UI thread.
  bool is_fetching_;

 private:
  friend class base::RefCountedThreadSafe<BrowsingDataServiceWorkerHelper>;

  // Enumerates all Service Worker instances on the IO thread.
  void FetchServiceWorkerUsageInfoOnIOThread();

  // Notifies the completion callback in the UI thread.
  void NotifyOnUIThread();

  // Deletes Service Workers for an origin the IO thread.
  void DeleteServiceWorkersOnIOThread(const GURL& origin);

  // Callback from ServiceWorkerContext::GetAllOriginsInfo()
  void GetAllOriginsInfoCallback(
      const std::vector<content::ServiceWorkerUsageInfo>& origins);

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataServiceWorkerHelper);
};

// This class is an implementation of BrowsingDataServiceWorkerHelper that does
// not fetch its information from the Service Worker context, but is passed the
// info as a parameter.
class CannedBrowsingDataServiceWorkerHelper
    : public BrowsingDataServiceWorkerHelper {
 public:
  // Contains information about a Service Worker.
  struct PendingServiceWorkerUsageInfo {
    PendingServiceWorkerUsageInfo(const GURL& origin,
                                  const std::vector<GURL>& scopes);
    ~PendingServiceWorkerUsageInfo();

    bool operator<(const PendingServiceWorkerUsageInfo& other) const;

    GURL origin;
    std::vector<GURL> scopes;
  };

  explicit CannedBrowsingDataServiceWorkerHelper(
      content::ServiceWorkerContext* context);

  // Return a copy of the ServiceWorker helper. Only one consumer can use the
  // StartFetching method at a time, so we need to create a copy of the helper
  // every time we instantiate a cookies tree model for it.
  CannedBrowsingDataServiceWorkerHelper* Clone();

  // Add a Service Worker to the set of canned Service Workers that is
  // returned by this helper.
  void AddServiceWorker(const GURL& origin, const std::vector<GURL>& scopes);

  // Clear the list of canned Service Workers.
  void Reset();

  // True if no Service Workers are currently stored.
  bool empty() const;

  // Returns the number of currently stored Service Workers.
  size_t GetServiceWorkerCount() const;

  // Returns the current list of Service Workers.
  const std::set<
      CannedBrowsingDataServiceWorkerHelper::PendingServiceWorkerUsageInfo>&
      GetServiceWorkerUsageInfo() const;

  // BrowsingDataServiceWorkerHelper methods.
  virtual void StartFetching(const base::Callback<void(
      const std::list<content::ServiceWorkerUsageInfo>&)>& callback) OVERRIDE;
  virtual void DeleteServiceWorkers(const GURL& origin) OVERRIDE;

 private:
  virtual ~CannedBrowsingDataServiceWorkerHelper();

  std::set<PendingServiceWorkerUsageInfo> pending_service_worker_info_;

  DISALLOW_COPY_AND_ASSIGN(CannedBrowsingDataServiceWorkerHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_SERVICE_WORKER_HELPER_H_
