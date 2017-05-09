// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_JOB_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_JOB_H_

#include <memory>

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/threading/non_thread_safe.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace net {
class NetworkDelegate;
class URLRequestJob;
}

namespace content {

class AppCacheEntry;
class AppCacheHost;
class AppCacheRequest;
class AppCacheStorage;
class URLRequestJob;

// Interface for an AppCache job. This is used to send data stored in the
// AppCache to networking consumers.
// Subclasses implement this interface to wrap custom job objects like
// URLRequestJob, URLLoaderJob, etc to ensure that these dependencies stay out
// of the AppCache code.
class CONTENT_EXPORT AppCacheJob
    : NON_EXPORTED_BASE(public base::NonThreadSafe),
      public base::SupportsWeakPtr<AppCacheJob> {
 public:
  // Callback that will be invoked before the request is restarted. The caller
  // can use this opportunity to grab state from the job to determine how it
  // should behave when the request is restarted.
  // TODO(ananta)
  // This applies only to the URLRequestJob at the moment. Look into taking
  // this knowledge out of this class.
  using OnPrepareToRestartCallback = base::Closure;

  // Factory function to create the AppCacheJob instance for the |request|
  // passed in. The |job_type| parameter controls the type of job which is
  // created.
  static std::unique_ptr<AppCacheJob> Create(
      bool is_main_resource,
      AppCacheHost* host,
      AppCacheStorage* storage,
      AppCacheRequest* request,
      net::NetworkDelegate* network_delegate,
      const OnPrepareToRestartCallback& restart_callback);

  virtual ~AppCacheJob();

  // Kills the job.
  virtual void Kill() = 0;

  // Returns true if the job was started.
  virtual bool IsStarted() const = 0;

  // Returns true if the job is waiting for instructions.
  virtual bool IsWaiting() const = 0;

  // Returns true if the job is delivering a response from the cache.
  virtual bool IsDeliveringAppCacheResponse() const = 0;

  // Returns true if the job is delivering a response from the network.
  virtual bool IsDeliveringNetworkResponse() const = 0;

  // Returns true if the job is delivering an error response.
  virtual bool IsDeliveringErrorResponse() const = 0;

  // Returns true if the cache entry was not found in the cache.
  virtual bool IsCacheEntryNotFound() const = 0;

  // Informs the job of what response it should deliver. Only one of these
  // methods should be called, and only once per job. A job will sit idle and
  // wait indefinitely until one of the deliver methods is called.
  virtual void DeliverAppCachedResponse(const GURL& manifest_url,
                                        int64_t cache_id,
                                        const AppCacheEntry& entry,
                                        bool is_fallback) = 0;

  // Informs the job that it should deliver the response from the network. This
  // is generally controlled by the entries in the manifest file.
  virtual void DeliverNetworkResponse() = 0;

  // Informs the job that it should deliver an error response.
  virtual void DeliverErrorResponse() = 0;

  // Returns a weak pointer reference to the job.
  virtual base::WeakPtr<AppCacheJob> GetWeakPtr();

  // Returns the URL of the job.
  virtual const GURL& GetURL() const = 0;

  // Returns the underlying URLRequestJob if any. This only applies to
  // AppCaches loaded via the URLRequest mechanism.
  virtual net::URLRequestJob* AsURLRequestJob();

 protected:
  AppCacheJob();

  base::WeakPtrFactory<AppCacheJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_JOB_H_
