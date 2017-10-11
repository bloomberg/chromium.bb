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
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_delegate_proxy.h"
#include "content/browser/background_fetch/background_fetch_event_dispatcher.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/WebKit/public/platform/modules/background_fetch/background_fetch.mojom.h"

namespace content {

class BackgroundFetchJobController;
struct BackgroundFetchOptions;
class BackgroundFetchRegistrationId;
class BackgroundFetchRegistrationNotifier;
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

  // Returns the JobController that is handling the |unique_id|, or a nullptr if
  // it does not exist. Must be immediately used by the caller.
  BackgroundFetchJobController* GetActiveFetch(
      const std::string& unique_id) const;

  // Registers the |observer| to be notified of progress events for the
  // registration identified by |unique_id| whenever they happen. The observer
  // will unregister itself when the Mojo endpoint goes away.
  void AddRegistrationObserver(
      const std::string& unique_id,
      blink::mojom::BackgroundFetchRegistrationObserverPtr observer);

  BackgroundFetchDataManager& data_manager() { return data_manager_; }

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

  BackgroundFetchDataManager data_manager_;
  BackgroundFetchEventDispatcher event_dispatcher_;
  BackgroundFetchDelegateProxy delegate_proxy_;

  // Map from background fetch registration |unique_id|s to active job
  // controllers. Must be destroyed before |data_manager_|.
  std::map<std::string, std::unique_ptr<BackgroundFetchJobController>>
      active_fetches_;

  std::unique_ptr<BackgroundFetchRegistrationNotifier> registration_notifier_;

  base::WeakPtrFactory<BackgroundFetchContext> weak_factory_;  // Must be last.

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_CONTEXT_H_
