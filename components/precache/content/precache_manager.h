// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRECACHE_CONTENT_PRECACHE_MANAGER_H_
#define COMPONENTS_PRECACHE_CONTENT_PRECACHE_MANAGER_H_

#include <list>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/precache/core/precache_fetcher.h"
#include "url/gurl.h"

namespace base {
class Time;
}

namespace content {
class BrowserContext;
}

namespace history {
class HistoryService;
}

namespace sync_driver {
class SyncService;
}

namespace precache {

class PrecacheDatabase;

// Visible for test.
int NumTopHosts();

// Class that manages all precaching-related activities. Owned by the
// BrowserContext that it is constructed for. Use
// PrecacheManagerFactory::GetForBrowserContext to get an instance of this
// class. All methods must be called on the UI thread unless indicated
// otherwise.
// TODO(sclittle): Delete precache history when browsing history is deleted.
// http://crbug.com/326549
class PrecacheManager : public KeyedService,
                        public PrecacheFetcher::PrecacheDelegate,
                        public base::SupportsWeakPtr<PrecacheManager> {
 public:
  typedef base::Closure PrecacheCompletionCallback;

  PrecacheManager(content::BrowserContext* browser_context,
                  const sync_driver::SyncService* const sync_service);
  ~PrecacheManager() override;

  // Returns true if precaching is allowed for the browser context based on user
  // settings, and enabled as part of a field trial or by commandline flag.
  // Virtual for testing.
  virtual bool ShouldRun() const;

  // Returns true if precaching is allowed for the browser context based on user
  // settings. Virtual for testing.
  virtual bool WouldRun() const;

  // Starts precaching resources that the user is predicted to fetch in the
  // future. If precaching is already currently in progress, then this method
  // does nothing. The |precache_completion_callback| will be run when
  // precaching finishes, but will not be run if precaching is canceled.
  void StartPrecaching(
      const PrecacheCompletionCallback& precache_completion_callback,
      const history::HistoryService& history_service);

  // Cancels precaching if it is in progress.
  void CancelPrecaching();

  // Returns true if precaching is currently in progress, or false otherwise.
  bool IsPrecaching() const;

  // Update precache-related metrics in response to a URL being fetched.
  void RecordStatsForFetch(const GURL& url,
                           const base::Time& fetch_time,
                           int64 size,
                           bool was_cached);

  // Posts a task to the DB thread to delete all history entries from the
  // database. Does not wait for completion of this task.
  void ClearHistory();

 private:
  // From KeyedService.
  void Shutdown() override;

  // From PrecacheFetcher::PrecacheDelegate.
  void OnDone() override;

  // From history::HistoryService::TopHosts.
  void OnHostsReceived(const history::TopHostsList& host_counts);

  // Returns true if precaching is enabled as part of a field trial or by the
  // command line flag. This has a different meaning from the
  // "is_precaching_enabled" pref set in PrecacheServiceLauncher. This method
  // can be called on any thread.
  static bool IsPrecachingEnabled();

  // Returns true if precaching is allowed for the browser context.
  bool IsPrecachingAllowed() const;

  // The browser context that owns this PrecacheManager.
  content::BrowserContext* const browser_context_;

  // The sync service corresponding to the browser context. Used to determine
  // whether precache can run. May be null.
  const sync_driver::SyncService* const sync_service_;

  // The PrecacheFetcher used to precache resources. Should only be used on the
  // UI thread.
  scoped_ptr<PrecacheFetcher> precache_fetcher_;

  // The callback that will be run if precaching finishes without being
  // canceled.
  PrecacheCompletionCallback precache_completion_callback_;

  // The PrecacheDatabase for tracking precache metrics. Should only be used on
  // the DB thread.
  const scoped_refptr<PrecacheDatabase> precache_database_;

  // Flag indicating whether or not precaching is currently in progress.
  bool is_precaching_;

  DISALLOW_COPY_AND_ASSIGN(PrecacheManager);
};

}  // namespace precache

#endif  // COMPONENTS_PRECACHE_CONTENT_PRECACHE_MANAGER_H_
