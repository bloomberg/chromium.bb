// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_H_

#include <set>
#include <string>

#include "base/callback_forward.h"
#include "content/public/browser/service_worker_usage_info.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerContextObserver;

enum class ServiceWorkerCapability {
  NO_SERVICE_WORKER,
  SERVICE_WORKER_NO_FETCH_HANDLER,
  SERVICE_WORKER_WITH_FETCH_HANDLER,
};

enum class StartServiceWorkerForNavigationHintResult {
  // The service worker started successfully.
  STARTED,
  // The service worker was already running.
  ALREADY_RUNNING,
  // There was no service worker registration for the given URL.
  NO_SERVICE_WORKER_REGISTRATION,
  // There was no active service worker for the given URL.
  NO_ACTIVE_SERVICE_WORKER_VERSION,
  // The service worker for the given URL had no fetch event handler.
  NO_FETCH_HANDLER,
  // Something failed.
  FAILED,
};

// Represents the per-StoragePartition ServiceWorker data.
class ServiceWorkerContext {
 public:
  // https://rawgithub.com/slightlyoff/ServiceWorker/master/spec/service_worker/index.html#url-scope:
  // roughly, must be of the form "<origin>/<path>/*".
  using Scope = GURL;

  using ResultCallback = base::Callback<void(bool success)>;

  using GetUsageInfoCallback = base::Callback<void(
      const std::vector<ServiceWorkerUsageInfo>& usage_info)>;

  using CheckHasServiceWorkerCallback =
      base::Callback<void(ServiceWorkerCapability capability)>;

  using CountExternalRequestsCallback =
      base::Callback<void(size_t external_request_count)>;

  using StartServiceWorkerForNavigationHintCallback =
      base::Callback<void(StartServiceWorkerForNavigationHintResult result)>;

  // Registers the header name which should not be passed to the ServiceWorker.
  // Must be called from the IO thread.
  CONTENT_EXPORT static void AddExcludedHeadersForFetchEvent(
      const std::set<std::string>& header_names);

  // Returns true if the header name should not be passed to the ServiceWorker.
  // Must be called from the IO thread.
  static bool IsExcludedHeaderNameForFetchEvent(const std::string& header_name);

  // Returns true if |url| is within the service worker |scope|.
  CONTENT_EXPORT static bool ScopeMatches(const GURL& scope, const GURL& url);

  // Observer methods are always dispatched on the UI thread.
  virtual void AddObserver(ServiceWorkerContextObserver* observer) = 0;
  virtual void RemoveObserver(ServiceWorkerContextObserver* observer) = 0;

  // Equivalent to calling navigator.serviceWorker.register(script_url, {scope:
  // pattern}) from a renderer, except that |pattern| is an absolute URL instead
  // of relative to some current origin.  |callback| is passed true when the JS
  // promise is fulfilled or false when the JS promise is rejected.
  //
  // The registration can fail if:
  //  * |script_url| is on a different origin from |pattern|
  //  * Fetching |script_url| fails.
  //  * |script_url| fails to parse or its top-level execution fails.
  //    TODO: The error message for this needs to be available to developers.
  //  * Something unexpected goes wrong, like a renderer crash or a full disk.
  //
  // This function can be called from any thread, but the callback will always
  // be called on the UI thread.
  virtual void RegisterServiceWorker(const Scope& pattern,
                                     const GURL& script_url,
                                     const ResultCallback& callback) = 0;

  // Mechanism for embedder to increment/decrement ref count of a service
  // worker.
  // Embedders can call StartingExternalRequest() while it is performing some
  // work with the worker. The worker is considered to be working until embedder
  // calls FinishedExternalRequest(). This ensures that content/ does not
  // shut the worker down while embedder is expecting the worker to be kept
  // alive.
  //
  // Must be called from the IO thread. Returns whether or not changing the ref
  // count succeeded.
  virtual bool StartingExternalRequest(int64_t service_worker_version_id,
                                       const std::string& request_uuid) = 0;
  virtual bool FinishedExternalRequest(int64_t service_worker_version_id,
                                       const std::string& request_uuid) = 0;

  // Equivalent to calling navigator.serviceWorker.unregister(pattern) from a
  // renderer, except that |pattern| is an absolute URL instead of relative to
  // some current origin.  |callback| is passed true when the JS promise is
  // fulfilled or false when the JS promise is rejected.
  //
  // Unregistration can fail if:
  //  * No Service Worker was registered for |pattern|.
  //  * Something unexpected goes wrong, like a renderer crash.
  //
  // This function can be called from any thread, but the callback will always
  // be called on the UI thread.
  virtual void UnregisterServiceWorker(const Scope& pattern,
                                       const ResultCallback& callback) = 0;

  // Methods used in response to browsing data and quota manager requests.

  // Must be called from the IO thread.
  virtual void GetAllOriginsInfo(const GetUsageInfoCallback& callback) = 0;

  // This function can be called from any thread, but the callback will always
  // be called on the IO thread.
  virtual void DeleteForOrigin(const GURL& origin_url,
                               const ResultCallback& callback) = 0;

  // Returns ServiceWorkerCapability describing existence and properties of a
  // Service Worker registration matching |url|. Found service worker
  // registration must also encompass the |other_url|, otherwise it will be
  // considered non existent by this method. Note that the longest matching
  // registration for |url| is described, which is not necessarily the longest
  // matching registration for |other_url|. In case the service worker is being
  // installed as of calling this method, it will wait for the installation to
  // finish before coming back with the result.
  //
  // This function can be called from any thread, but the callback will always
  // be called on the UI thread.
  virtual void CheckHasServiceWorker(
      const GURL& url,
      const GURL& other_url,
      const CheckHasServiceWorkerCallback& callback) = 0;

  // Returns the pending external request count for the worker with the
  // specified |origin| via |callback|.
  virtual void CountExternalRequestsForTest(
      const GURL& origin,
      const CountExternalRequestsCallback& callback) = 0;

  // Stops all running workers on the given |origin|.
  //
  // This function can be called from any thread.
  virtual void StopAllServiceWorkersForOrigin(const GURL& origin) = 0;

  // Stops all running service workers and unregisters all service worker
  // registrations. This method is used in LayoutTests to make sure that the
  // existing service worker will not affect the succeeding tests.
  //
  // This function can be called from any thread, but the callback will always
  // be called on the UI thread.
  virtual void ClearAllServiceWorkersForTest(const base::Closure& callback) = 0;

  // Starts the service worker for |document_url|. Called when a navigation to
  // that URL is predicted to occur soon. Must be called from the UI thread. The
  // |callback| will always be called on the UI thread.
  virtual void StartServiceWorkerForNavigationHint(
      const GURL& document_url,
      const StartServiceWorkerForNavigationHintCallback& callback) = 0;

 protected:
  ServiceWorkerContext() {}
  virtual ~ServiceWorkerContext() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_H_
