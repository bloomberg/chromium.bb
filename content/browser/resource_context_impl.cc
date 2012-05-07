// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/resource_context_impl.h"

#include "base/logging.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/fileapi/browser_file_system_helper.h"
#include "content/browser/fileapi/chrome_blob_storage_context.h"
#include "content/browser/host_zoom_map_impl.h"
#include "content/browser/in_process_webkit/indexed_db_context_impl.h"
#include "content/browser/net/view_blob_internals_job_factory.h"
#include "content/browser/net/view_http_cache_job_factory.h"
#include "content/browser/renderer_host/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/resource_request_info_impl.h"
#include "content/browser/tcmalloc_internals_request_job.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
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
static const char* kAppCacheServicKeyName = "content_appcache_service_tracker";
static const char* kBlobStorageContextKeyName = "content_blob_storage_context";
static const char* kDatabaseTrackerKeyName = "content_database_tracker";
static const char* kFileSystemContextKeyName = "content_file_system_context";
static const char* kIndexedDBContextKeyName = "content_indexed_db_context";
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
        net::URLRequest* request) const OVERRIDE {
    // Check for chrome://view-http-cache/*, which uses its own job type.
    if (ViewHttpCacheJobFactory::IsSupportedURL(request->url()))
      return ViewHttpCacheJobFactory::CreateJobForRequest(request);

    // Next check for chrome://appcache-internals/, which uses its own job type.
    if (request->url().SchemeIs(chrome::kChromeUIScheme) &&
        request->url().host() == chrome::kChromeUIAppCacheInternalsHost) {
      return appcache::ViewAppCacheInternalsJobFactory::CreateJobForRequest(
          request, appcache_service_);
    }

    // Next check for chrome://blob-internals/, which uses its own job type.
    if (ViewBlobInternalsJobFactory::IsSupportedURL(request->url())) {
      return ViewBlobInternalsJobFactory::CreateJobForRequest(
          request, blob_storage_controller_);
    }

#if defined(USE_TCMALLOC)
    // Next check for chrome://tcmalloc/, which uses its own job type.
    if (request->url().SchemeIs(chrome::kChromeUIScheme) &&
        request->url().host() == chrome::kChromeUITcmallocHost) {
      return new TcmallocInternalsRequestJob(request);
    }
#endif

    return NULL;
  }

  virtual net::URLRequestJob* MaybeInterceptRedirect(
        const GURL& location,
        net::URLRequest* request) const OVERRIDE {
    return NULL;
  }

  virtual net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request) const OVERRIDE {
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
    ResourceContext* resource_context,
    scoped_refptr<net::URLRequestContextGetter> context_getter) {
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
          GetBlobStorageControllerForResourceContext(resource_context),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE)));
  DCHECK(set_protocol);
  set_protocol = job_factory->SetProtocolHandler(
      chrome::kFileSystemScheme,
      CreateFileSystemProtocolHandler(
          GetFileSystemContextForResourceContext(resource_context)));
  DCHECK(set_protocol);

  job_factory->AddInterceptor(new DeveloperProtocolHandler(
      ResourceContext::GetAppCacheService(resource_context),
      GetBlobStorageControllerForResourceContext(resource_context)));

  // TODO(jam): Add the ProtocolHandlerRegistryIntercepter here!
}

}  // namespace

AppCacheService* ResourceContext::GetAppCacheService(ResourceContext* context) {
  return UserDataAdapter<ChromeAppCacheService>::Get(
      context, kAppCacheServicKeyName);
}

ResourceContext::~ResourceContext() {
  if (ResourceDispatcherHostImpl::Get())
    ResourceDispatcherHostImpl::Get()->CancelRequestsForContext(this);
}

BlobStorageController* GetBlobStorageControllerForResourceContext(
    ResourceContext* resource_context) {
  return GetChromeBlobStorageContextForResourceContext(resource_context)->
      controller();
}

DatabaseTracker* GetDatabaseTrackerForResourceContext(
    ResourceContext* resource_context) {
  return UserDataAdapter<DatabaseTracker>::Get(
      resource_context, kDatabaseTrackerKeyName);
}

FileSystemContext* GetFileSystemContextForResourceContext(
    ResourceContext* resource_context) {
  return UserDataAdapter<FileSystemContext>::Get(
      resource_context, kFileSystemContextKeyName);
}

IndexedDBContextImpl* GetIndexedDBContextForResourceContext(
    ResourceContext* resource_context) {
  return UserDataAdapter<IndexedDBContextImpl>::Get(
      resource_context, kIndexedDBContextKeyName);
}

ChromeBlobStorageContext* GetChromeBlobStorageContextForResourceContext(
    ResourceContext* resource_context) {
  return UserDataAdapter<ChromeBlobStorageContext>::Get(
      resource_context, kBlobStorageContextKeyName);
}

HostZoomMap* GetHostZoomMapForResourceContext(ResourceContext* context) {
  return static_cast<NonOwningZoomData*>(
      context->GetUserData(kHostZoomMapKeyName))->host_zoom_map();
}

void InitializeResourceContext(BrowserContext* browser_context) {
  ResourceContext* resource_context = browser_context->GetResourceContext();
  DCHECK(!resource_context->GetUserData(kIndexedDBContextKeyName));

  resource_context->SetUserData(
      kIndexedDBContextKeyName,
      new UserDataAdapter<IndexedDBContextImpl>(
          static_cast<IndexedDBContextImpl*>(
              BrowserContext::GetIndexedDBContext(browser_context))));
  resource_context->SetUserData(
      kDatabaseTrackerKeyName,
      new UserDataAdapter<webkit_database::DatabaseTracker>(
          BrowserContext::GetDatabaseTracker(browser_context)));
  resource_context->SetUserData(
      kAppCacheServicKeyName,
      new UserDataAdapter<ChromeAppCacheService>(
          static_cast<ChromeAppCacheService*>(
              BrowserContext::GetAppCacheService(browser_context))));
  resource_context->SetUserData(
      kFileSystemContextKeyName,
      new UserDataAdapter<FileSystemContext>(
          BrowserContext::GetFileSystemContext(browser_context)));
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

  // Add content's URLRequestContext's hooks.
  // Check first to avoid memory leak in unittests.
  if (BrowserThread::IsMessageLoopValid(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&InitializeRequestContext,
                   resource_context,
                   make_scoped_refptr(browser_context->GetRequestContext())));
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&InitializeRequestContext,
                   resource_context,
                   make_scoped_refptr(
                       browser_context->GetRequestContextForMedia())));
  }
}

}  // namespace content
