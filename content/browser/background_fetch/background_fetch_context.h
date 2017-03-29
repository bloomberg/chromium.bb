// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_CONTEXT_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_CONTEXT_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/WebKit/public/platform/modules/background_fetch/background_fetch.mojom.h"

namespace url {
class Origin;
}

namespace content {

class BackgroundFetchDataManager;
class BackgroundFetchJobController;
struct BackgroundFetchOptions;
class BackgroundFetchRegistrationId;
class BrowserContext;
class ServiceWorkerContextWrapper;
struct ServiceWorkerFetchRequest;
class StoragePartitionImpl;

// The BackgroundFetchContext is the central moderator of ongoing background
// fetch requests from the Mojo service and from other callers.
// Background Fetch requests function similar to normal fetches except that
// they are persistent across Chromium or service worker shutdown.
class CONTENT_EXPORT BackgroundFetchContext
    : public base::RefCountedThreadSafe<BackgroundFetchContext,
                                        BrowserThread::DeleteOnUIThread> {
 public:
  // The BackgroundFetchContext will watch the ServiceWorkerContextWrapper so
  // that it can respond to service worker events such as unregister.
  BackgroundFetchContext(
      BrowserContext* browser_context,
      StoragePartitionImpl* storage_partition,
      const scoped_refptr<ServiceWorkerContextWrapper>& context);

  // Shutdown must be called before deleting this. Call on the UI thread.
  void Shutdown();

  // Starts a Background Fetch for the |registration_id|. The |requests| will be
  // asynchronously fetched. The |callback| will be invoked when the fetch has
  // been registered, or an error occurred that avoids it from doing so.
  void StartFetch(
      const BackgroundFetchRegistrationId& registration_id,
      const std::vector<ServiceWorkerFetchRequest>& requests,
      const BackgroundFetchOptions& options,
      const blink::mojom::BackgroundFetchService::FetchCallback& callback);

  // Returns a vector with the tags of the active fetches for the given |origin|
  // and |service_worker_registration_id|.
  std::vector<std::string> GetActiveTagsForServiceWorkerRegistration(
      int64_t service_worker_registration_id,
      const url::Origin& origin) const;

  // Returns the JobController that is handling the |registration_id|, or a
  // nullptr if it does not exist. Must be immediately used by the caller.
  BackgroundFetchJobController* GetActiveFetch(
      const BackgroundFetchRegistrationId& registration_id) const;

 private:
  friend class base::DeleteHelper<BackgroundFetchContext>;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class base::RefCountedThreadSafe<BackgroundFetchContext,
                                          BrowserThread::DeleteOnUIThread>;

  ~BackgroundFetchContext();

  // Shuts down the active Job Controllers on the IO thread.
  void ShutdownOnIO();

  // Creates a new Job Controller for the given |registration_id| and |options|,
  // which will start fetching the files that are part of the registration.
  void CreateController(const BackgroundFetchRegistrationId& registration_id,
                        const BackgroundFetchOptions& options);

  // Called when a new registration has been created by the data manager.
  void DidCreateRegistration(
      const BackgroundFetchRegistrationId& registration_id,
      const BackgroundFetchOptions& options,
      const blink::mojom::BackgroundFetchService::FetchCallback& callback,
      blink::mojom::BackgroundFetchError error);

  // Called when a the given |controller| has finished processing its job.
  void DidCompleteJob(BackgroundFetchJobController* controller);

  // |this| is owned by the BrowserContext via the StoragePartitionImpl.
  BrowserContext* browser_context_;
  StoragePartitionImpl* storage_partition_;

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  std::unique_ptr<BackgroundFetchDataManager> background_fetch_data_manager_;

  // Map of the Background Fetch fetches that are currently in-progress.
  std::map<BackgroundFetchRegistrationId,
           std::unique_ptr<BackgroundFetchJobController>>
      active_fetches_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_CONTEXT_H_
