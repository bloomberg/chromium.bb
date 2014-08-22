// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/shell/browser/cast_browser_context.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "chromecast/common/cast_paths.h"
#include "chromecast/shell/browser/url_request_context_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace chromecast {
namespace shell {

class CastBrowserContext::CastResourceContext :
    public content::ResourceContext {
 public:
  CastResourceContext(URLRequestContextFactory* url_request_context_factory) :
    url_request_context_factory_(url_request_context_factory) {}
  virtual ~CastResourceContext() {}

  // ResourceContext implementation:
  virtual net::HostResolver* GetHostResolver() OVERRIDE {
    return url_request_context_factory_->GetMainGetter()->
        GetURLRequestContext()->host_resolver();
  }

  virtual net::URLRequestContext* GetRequestContext() OVERRIDE {
    return url_request_context_factory_->GetMainGetter()->
        GetURLRequestContext();
  }

  virtual bool AllowMicAccess(const GURL& origin) OVERRIDE {
    return false;
  }

  virtual bool AllowCameraAccess(const GURL& origin) OVERRIDE {
    return false;
  }

 private:
  URLRequestContextFactory* url_request_context_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastResourceContext);
};

CastBrowserContext::CastBrowserContext(
    URLRequestContextFactory* url_request_context_factory)
    : url_request_context_factory_(url_request_context_factory),
      resource_context_(new CastResourceContext(url_request_context_factory)) {
  InitWhileIOAllowed();
}

CastBrowserContext::~CastBrowserContext() {
}

void CastBrowserContext::InitWhileIOAllowed() {
  // Chromecast doesn't support user profiles nor does it have
  // incognito mode.  This means that all of the persistent
  // data (currently only cookies and local storage) will be
  // shared in a single location as defined here.
  CHECK(PathService::Get(DIR_CAST_HOME, &path_));
}

base::FilePath CastBrowserContext::GetPath() const {
  return path_;
}

bool CastBrowserContext::IsOffTheRecord() const {
  return false;
}

net::URLRequestContextGetter* CastBrowserContext::GetRequestContext() {
  return GetDefaultStoragePartition(this)->GetURLRequestContext();
}

net::URLRequestContextGetter*
CastBrowserContext::GetRequestContextForRenderProcess(int renderer_child_id) {
  return GetRequestContext();
}

net::URLRequestContextGetter* CastBrowserContext::GetMediaRequestContext() {
  return url_request_context_factory_->GetMediaGetter();
}

net::URLRequestContextGetter*
CastBrowserContext::GetMediaRequestContextForRenderProcess(
    int renderer_child_id) {
  return GetMediaRequestContext();
}

net::URLRequestContextGetter*
CastBrowserContext::GetMediaRequestContextForStoragePartition(
    const base::FilePath& partition_path, bool in_memory) {
  return GetMediaRequestContext();
}

content::ResourceContext* CastBrowserContext::GetResourceContext() {
  return resource_context_.get();
}

content::DownloadManagerDelegate*
CastBrowserContext::GetDownloadManagerDelegate() {
  NOTIMPLEMENTED();
  return NULL;
}

content::BrowserPluginGuestManager* CastBrowserContext::GetGuestManager() {
  NOTIMPLEMENTED();
  return NULL;
}

storage::SpecialStoragePolicy* CastBrowserContext::GetSpecialStoragePolicy() {
  NOTIMPLEMENTED();
  return NULL;
}

content::PushMessagingService* CastBrowserContext::GetPushMessagingService() {
  NOTIMPLEMENTED();
  return NULL;
}

content::SSLHostStateDelegate* CastBrowserContext::GetSSLHostStateDelegate() {
  NOTIMPLEMENTED();
  return NULL;
}

}  // namespace shell
}  // namespace chromecast
