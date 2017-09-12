// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_CONTEXT_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_CONTEXT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/WebKit/public/platform/modules/background_fetch/background_fetch.mojom.h"

namespace url {
class Origin;
}

namespace content {

class BackgroundFetchDataManager;
class BackgroundFetchDelegate;
class BackgroundFetchDelegateProxy;
class BackgroundFetchEventDispatcher;
class BackgroundFetchJobController;
struct BackgroundFetchOptions;
class BackgroundFetchRegistrationId;
class BlobHandle;
class BrowserContext;
class ServiceWorkerContextWrapper;
struct ServiceWorkerFetchRequest;

// The BackgroundFetchContext is the central moderator of ongoing background
// fetch requests from the Mojo service and from other callers.
// Background Fetch requests function similarly to normal fetches except that
// they are persistent across Chromium or service worker shutdown.
class CONTENT_EXPORT BackgroundFetchContext
    : public base::RefCountedThreadSafe<BackgroundFetchContext,
                                        BrowserThread::DeleteOnIOThread> {
 public:
  // The BackgroundFetchContext will watch the ServiceWorkerContextWrapper so
  // that it can respond to service worker events such as unregister.
  BackgroundFetchContext(
      BrowserContext* browser_context,
      const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context);

  // Starts a Background Fetch for the |registration_id|. The |requests| will be
  // asynchronously fetched. The |callback| will be invoked when the fetch has
  // been registered, or an error occurred that avoids it from doing so.
  void StartFetch(const BackgroundFetchRegistrationId& registration_id,
                  const std::vector<ServiceWorkerFetchRequest>& requests,
                  const BackgroundFetchOptions& options,
                  blink::mojom::BackgroundFetchService::FetchCallback callback);

  // Returns a vector with the ids of the active fetches for the given |origin|
  // and |service_worker_registration_id|.
  std::vector<std::string> GetActiveIdsForServiceWorkerRegistration(
      int64_t service_worker_registration_id,
      const url::Origin& origin) const;

  // Returns the JobController that is handling the |registration_id|, or a
  // nullptr if it does not exist. Must be immediately used by the caller.
  BackgroundFetchJobController* GetActiveFetch(
      const BackgroundFetchRegistrationId& registration_id) const;

 private:
  friend class base::DeleteHelper<BackgroundFetchContext>;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
  friend class base::RefCountedThreadSafe<BackgroundFetchContext,
                                          BrowserThread::DeleteOnIOThread>;

  ~BackgroundFetchContext();

  // Creates a new Job Controller for the given |registration_id| and |options|,
  // which will start fetching the files that are part of the registration.
  void CreateController(const BackgroundFetchRegistrationId& registration_id,
                        const BackgroundFetchOptions& options);

  // Called when a new registration has been created by the data manager.
  void DidCreateRegistration(
      const BackgroundFetchRegistrationId& registration_id,
      const BackgroundFetchOptions& options,
      blink::mojom::BackgroundFetchService::FetchCallback callback,
      blink::mojom::BackgroundFetchError error);

  // Called when the given |controller| has finished processing its job.
  void DidCompleteJob(BackgroundFetchJobController* controller);

  // Called when the sequence of settled fetches for |registration_id| have been
  // retrieved from storage, and the Service Worker event can be invoked.
  void DidGetSettledFetches(
      const BackgroundFetchRegistrationId& registration_id,
      blink::mojom::BackgroundFetchError error,
      bool background_fetch_succeeded,
      std::vector<BackgroundFetchSettledFetch> settled_fetches,
      std::vector<std::unique_ptr<BlobHandle>> blob_handles);

  // Called when all processing for the |registration_id| has been finished and
  // the job, including its associated data, is ready to be deleted.
  void DeleteRegistration(
      const BackgroundFetchRegistrationId& registration_id,
      const std::vector<std::unique_ptr<BlobHandle>>& blob_handles);

  // |this| is owned, indirectly, by the BrowserContext.
  BrowserContext* browser_context_;

  std::unique_ptr<BackgroundFetchDataManager> data_manager_;
  std::unique_ptr<BackgroundFetchEventDispatcher> event_dispatcher_;
  std::unique_ptr<BackgroundFetchDelegate, BrowserThread::DeleteOnUIThread>
      delegate_;
  std::unique_ptr<BackgroundFetchDelegateProxy> delegate_proxy_;

  // Map of the Background Fetch fetches that are currently in-progress.
  std::map<BackgroundFetchRegistrationId,
           std::unique_ptr<BackgroundFetchJobController>>
      active_fetches_;

  base::WeakPtrFactory<BackgroundFetchContext> weak_factory_;  // Must be last.

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_CONTEXT_H_
