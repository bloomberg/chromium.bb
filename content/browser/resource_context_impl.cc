// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/resource_context_impl.h"

#include "base/logging.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/fileapi/browser_file_system_helper.h"
#include "content/browser/fileapi/chrome_blob_storage_context.h"
#include "content/browser/histogram_internals_request_job.h"
#include "content/browser/host_zoom_map_impl.h"
#include "content/browser/in_process_webkit/indexed_db_context_impl.h"
#include "content/browser/net/view_blob_internals_job_factory.h"
#include "content/browser/net/view_http_cache_job_factory.h"
#include "content/browser/renderer_host/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/resource_request_info_impl.h"
#include "content/browser/tcmalloc_internals_request_job.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "webkit/appcache/appcache_service.h"
#include "webkit/appcache/view_appcache_internals_job.h"
#include "webkit/blob/blob_data.h"
#include "webkit/blob/blob_url_request_job_factory.h"
#include "webkit/database/database_tracker.h"
#include "webkit/fileapi/file_system_url_request_job_factory.h"

// Key names on ResourceContext.
static const char* kAppCacheServiceKeyName = "content_appcache_service_tracker";
static const char* kBlobStorageContextKeyName = "content_blob_storage_context";
static const char* kHostZoomMapKeyName = "content_host_zoom_map";

using appcache::AppCacheService;
using base::UserDataAdapter;
using content::BrowserThread;
using fileapi::FileSystemContext;
using webkit_blob::BlobStorageController;
using webkit_database::DatabaseTracker;

namespace content {

namespace {

class NonOwningZoomData : public base::SupportsUserData::Data {
 public:
  explicit NonOwningZoomData(HostZoomMap* hzm) : host_zoom_map_(hzm) {}
  HostZoomMap* host_zoom_map() { return host_zoom_map_; }

 private:
  HostZoomMap* host_zoom_map_;
};

class BlobProtocolHandler : public webkit_blob::BlobProtocolHandler {
 public:
  BlobProtocolHandler(
      webkit_blob::BlobStorageController* blob_storage_controller,
      base::MessageLoopProxy* loop_proxy)
      : webkit_blob::BlobProtocolHandler(blob_storage_controller,
                                         loop_proxy) {}

  virtual ~BlobProtocolHandler() {}

 private:
  virtual scoped_refptr<webkit_blob::BlobData>
      LookupBlobData(net::URLRequest* request) const {
    const ResourceRequestInfoImpl* info =
        ResourceRequestInfoImpl::ForRequest(request);
    if (!info)
      return NULL;
    return info->requested_blob_data();
  }

  DISALLOW_COPY_AND_ASSIGN(BlobProtocolHandler);
};

// Adds a bunch of debugging urls. We use an interceptor instead of a protocol
// handler because we want to reuse the chrome://scheme (everyone is familiar
// with it, and no need to expose the content/chrome separation through our UI).
class DeveloperProtocolHandler
    : public net::URLRequestJobFactory::Interceptor {
 public:
  DeveloperProtocolHandler(
      AppCacheService* appcache_service,
      BlobStorageController* blob_storage_controller)
      : appcache_service_(appcache_service),
        blob_storage_controller_(blob_storage_controller) {}
  virtual ~DeveloperProtocolHandler() {}

  virtual net::URLRequestJob* MaybeIntercept(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    // Check for chrome://view-http-cache/*, which uses its own job type.
    if (ViewHttpCacheJobFactory::IsSupportedURL(request->url()))
      return ViewHttpCacheJobFactory::CreateJobForRequest(request,
                                                          network_delegate);

    // Next check for chrome://appcache-internals/, which uses its own job type.
    if (request->url().SchemeIs(chrome::kChromeUIScheme) &&
        request->url().host() == chrome::kChromeUIAppCacheInternalsHost) {
      return appcache::ViewAppCacheInternalsJobFactory::CreateJobForRequest(
          request, network_delegate, appcache_service_);
    }

    // Next check for chrome://blob-internals/, which uses its own job type.
    if (ViewBlobInternalsJobFactory::IsSupportedURL(request->url())) {
      return ViewBlobInternalsJobFactory::CreateJobForRequest(
          request, network_delegate, blob_storage_controller_);
    }

#if defined(USE_TCMALLOC)
    // Next check for chrome://tcmalloc/, which uses its own job type.
    if (request->url().SchemeIs(chrome::kChromeUIScheme) &&
        request->url().host() == chrome::kChromeUITcmallocHost) {
      return new TcmallocInternalsRequestJob(request, network_delegate);
    }
#endif

    // Next check for chrome://histograms/, which uses its own job type.
    if (request->url().SchemeIs(chrome::kChromeUIScheme) &&
        request->url().host() == chrome::kChromeUIHistogramHost) {
      return new HistogramInternalsRequestJob(request, network_delegate);
    }

    return NULL;
  }

  virtual net::URLRequestJob* MaybeInterceptRedirect(
        const GURL& location,
        net::URLRequest* request,
        net::NetworkDelegate* network_delegate) const OVERRIDE {
    return NULL;
  }

  virtual net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    return NULL;
  }

  virtual bool WillHandleProtocol(const std::string& protocol) const {
    return protocol == chrome::kChromeUIScheme;
  }

 private:
  AppCacheService* appcache_service_;
  BlobStorageController* blob_storage_controller_;
};

void InitializeRequestContext(
    scoped_refptr<net::URLRequestContextGetter> context_getter,
    AppCacheService* appcache_service,
    FileSystemContext* file_system_context,
    ChromeBlobStorageContext* blob_storage_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!context_getter)
    return;  // tests.
  net::URLRequestContext* context = context_getter->GetURLRequestContext();
  net::URLRequestJobFactory* job_factory =
      const_cast<net::URLRequestJobFactory*>(context->job_factory());
  if (job_factory->IsHandledProtocol(chrome::kBlobScheme))
    return;  // Already initialized this RequestContext.

  bool set_protocol = job_factory->SetProtocolHandler(
      chrome::kBlobScheme,
      new BlobProtocolHandler(
          blob_storage_context->controller(),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE)));
  DCHECK(set_protocol);
  set_protocol = job_factory->SetProtocolHandler(
      chrome::kFileSystemScheme,
      CreateFileSystemProtocolHandler(file_system_context));
  DCHECK(set_protocol);

  job_factory->AddInterceptor(
      new DeveloperProtocolHandler(appcache_service,
                                   blob_storage_context->controller()));

  // TODO(jam): Add the ProtocolHandlerRegistryIntercepter here!
}

}  // namespace

AppCacheService* ResourceContext::GetAppCacheService(ResourceContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return UserDataAdapter<ChromeAppCacheService>::Get(
      context, kAppCacheServiceKeyName);
}

ResourceContext::ResourceContext() {
  if (ResourceDispatcherHostImpl::Get())
    ResourceDispatcherHostImpl::Get()->AddResourceContext(this);
}

ResourceContext::~ResourceContext() {
  ResourceDispatcherHostImpl* rdhi = ResourceDispatcherHostImpl::Get();
  if (rdhi) {
    rdhi->CancelRequestsForContext(this);
    rdhi->RemoveResourceContext(this);
  }
}

BlobStorageController* GetBlobStorageControllerForResourceContext(
    ResourceContext* resource_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return GetChromeBlobStorageContextForResourceContext(resource_context)->
      controller();
}
ChromeBlobStorageContext* GetChromeBlobStorageContextForResourceContext(
    ResourceContext* resource_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return UserDataAdapter<ChromeBlobStorageContext>::Get(
      resource_context, kBlobStorageContextKeyName);
}

HostZoomMap* GetHostZoomMapForResourceContext(ResourceContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return static_cast<NonOwningZoomData*>(
      context->GetUserData(kHostZoomMapKeyName))->host_zoom_map();
}

void InitializeResourceContext(BrowserContext* browser_context) {
  ResourceContext* resource_context = browser_context->GetResourceContext();
  DCHECK(!resource_context->GetUserData(kHostZoomMapKeyName));

  resource_context->SetUserData(
      kAppCacheServiceKeyName,
      new UserDataAdapter<ChromeAppCacheService>(
          static_cast<ChromeAppCacheService*>(
              BrowserContext::GetAppCacheService(browser_context))));
  resource_context->SetUserData(
      kBlobStorageContextKeyName,
      new UserDataAdapter<ChromeBlobStorageContext>(
          ChromeBlobStorageContext::GetFor(browser_context)));

  // This object is owned by the BrowserContext and not ResourceContext, so
  // store a non-owning pointer here.
  resource_context->SetUserData(
      kHostZoomMapKeyName,
      new NonOwningZoomData(
          HostZoomMap::GetForBrowserContext(browser_context)));
  resource_context->DetachUserDataThread();

  // Add content's URLRequestContext's hooks.
  // Check first to avoid memory leak in unittests.
  // TODO(creis): Do equivalent initializations for isolated app and isolated
  // media request contexts.
  if (BrowserThread::IsMessageLoopValid(BrowserThread::IO)) {
    // TODO(ajwong): Move this whole block into
    // StoragePartitionImplMap::PostCreateInitialization after we're certain
    // this is safe to happen before InitializeResourceContext, and after we've
    // found the right URLRequestContext for a storage partition.  Otherwise,
    // our isolated URLRequestContext getters will do the wrong thing for blobs,
    // and FileSystemContext.
    //
    // http://crbug.com/85121

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &InitializeRequestContext,
            make_scoped_refptr(browser_context->GetRequestContext()),
            BrowserContext::GetAppCacheService(browser_context),
            make_scoped_refptr(
                BrowserContext::GetFileSystemContext(browser_context)),
            make_scoped_refptr(
                ChromeBlobStorageContext::GetFor(browser_context))));
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &InitializeRequestContext,
            make_scoped_refptr(browser_context->GetMediaRequestContext()),
            BrowserContext::GetAppCacheService(browser_context),
            make_scoped_refptr(
                BrowserContext::GetFileSystemContext(browser_context)),
            make_scoped_refptr(
                ChromeBlobStorageContext::GetFor(browser_context))));
  }
}

}  // namespace content
