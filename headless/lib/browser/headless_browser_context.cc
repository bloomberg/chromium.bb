// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_context.h"

#include "base/path_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/storage_partition.h"
#include "headless/lib/browser/headless_url_request_context_getter.h"
#include "net/url_request/url_request_context.h"

namespace headless {

// Contains net::URLRequestContextGetter required for resource loading.
// Must be destructed on the IO thread as per content::ResourceContext
// requirements.
class HeadlessResourceContext : public content::ResourceContext {
 public:
  HeadlessResourceContext();
  ~HeadlessResourceContext() override;

  // ResourceContext implementation:
  net::HostResolver* GetHostResolver() override;
  net::URLRequestContext* GetRequestContext() override;

  // Configure the URL request context getter to be used for resource fetching.
  // Must be called before any of the other methods of this class are used. Must
  // be called on the browser UI thread.
  void set_url_request_context_getter(
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    url_request_context_getter_ = std::move(url_request_context_getter);
  }

 private:
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessResourceContext);
};

HeadlessResourceContext::HeadlessResourceContext() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

HeadlessResourceContext::~HeadlessResourceContext() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
}

net::HostResolver* HeadlessResourceContext::GetHostResolver() {
  CHECK(url_request_context_getter_);
  return url_request_context_getter_->GetURLRequestContext()->host_resolver();
}

net::URLRequestContext* HeadlessResourceContext::GetRequestContext() {
  CHECK(url_request_context_getter_);
  return url_request_context_getter_->GetURLRequestContext();
}

HeadlessBrowserContext::HeadlessBrowserContext(
    const HeadlessBrowser::Options& options)
    : resource_context_(new HeadlessResourceContext), options_(options) {
  InitWhileIOAllowed();
}

HeadlessBrowserContext::~HeadlessBrowserContext() {
  if (resource_context_) {
    content::BrowserThread::DeleteSoon(content::BrowserThread::IO, FROM_HERE,
                                       resource_context_.release());
  }
}

void HeadlessBrowserContext::InitWhileIOAllowed() {
  // TODO(skyostil): Allow the embedder to override this.
  PathService::Get(base::DIR_EXE, &path_);
}

scoped_ptr<content::ZoomLevelDelegate>
HeadlessBrowserContext::CreateZoomLevelDelegate(
    const base::FilePath& partition_path) {
  return scoped_ptr<content::ZoomLevelDelegate>();
}

base::FilePath HeadlessBrowserContext::GetPath() const {
  return path_;
}

bool HeadlessBrowserContext::IsOffTheRecord() const {
  return false;
}

net::URLRequestContextGetter* HeadlessBrowserContext::GetRequestContext() {
  return GetDefaultStoragePartition(this)->GetURLRequestContext();
}

net::URLRequestContextGetter*
HeadlessBrowserContext::GetRequestContextForRenderProcess(
    int renderer_child_id) {
  return GetRequestContext();
}

net::URLRequestContextGetter* HeadlessBrowserContext::GetMediaRequestContext() {
  return GetRequestContext();
}

net::URLRequestContextGetter*
HeadlessBrowserContext::GetMediaRequestContextForRenderProcess(
    int renderer_child_id) {
  return GetRequestContext();
}

net::URLRequestContextGetter*
HeadlessBrowserContext::GetMediaRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory) {
  return GetRequestContext();
}

content::ResourceContext* HeadlessBrowserContext::GetResourceContext() {
  return resource_context_.get();
}

content::DownloadManagerDelegate*
HeadlessBrowserContext::GetDownloadManagerDelegate() {
  return nullptr;
}

content::BrowserPluginGuestManager* HeadlessBrowserContext::GetGuestManager() {
  // TODO(altimin): Should be non-null? (is null in content/shell).
  return nullptr;
}

storage::SpecialStoragePolicy*
HeadlessBrowserContext::GetSpecialStoragePolicy() {
  return nullptr;
}

content::PushMessagingService*
HeadlessBrowserContext::GetPushMessagingService() {
  return nullptr;
}

content::SSLHostStateDelegate*
HeadlessBrowserContext::GetSSLHostStateDelegate() {
  return nullptr;
}

content::PermissionManager* HeadlessBrowserContext::GetPermissionManager() {
  return nullptr;
}

content::BackgroundSyncController*
HeadlessBrowserContext::GetBackgroundSyncController() {
  return nullptr;
}

void HeadlessBrowserContext::SetURLRequestContextGetter(
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter) {
  resource_context_->set_url_request_context_getter(url_request_context_getter);
}

}  // namespace headless
