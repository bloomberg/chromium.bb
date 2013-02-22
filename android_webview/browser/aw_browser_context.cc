// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_context.h"

#include "android_webview/browser/aw_quota_manager_bridge.h"
#include "android_webview/browser/jni_dependency_factory.h"
#include "android_webview/browser/net/aw_url_request_context_getter.h"
#include "components/visitedlink/browser/visitedlink_master.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request_context.h"

namespace android_webview {

namespace {

class AwResourceContext : public content::ResourceContext {
 public:
  explicit AwResourceContext(net::URLRequestContextGetter* getter)
      : getter_(getter) {}
  virtual ~AwResourceContext() {}

  // content::ResourceContext implementation.
  virtual net::HostResolver* GetHostResolver() OVERRIDE {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    return getter_->GetURLRequestContext()->host_resolver();
  }
  virtual net::URLRequestContext* GetRequestContext() OVERRIDE {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    return getter_->GetURLRequestContext();
  }

 private:
  net::URLRequestContextGetter* getter_;

  DISALLOW_COPY_AND_ASSIGN(AwResourceContext);
};

}  // namespace

AwBrowserContext::AwBrowserContext(
    const base::FilePath path,
    JniDependencyFactory* native_factory)
    : context_storage_path_(path),
      native_factory_(native_factory) {
}

AwBrowserContext::~AwBrowserContext() {
}

// static
AwBrowserContext* AwBrowserContext::FromWebContents(
    content::WebContents* web_contents) {
  // This is safe; this is the only implementation of the browser context.
  return static_cast<AwBrowserContext*>(web_contents->GetBrowserContext());
}

void AwBrowserContext::InitializeBeforeThreadCreation() {
  DCHECK(!url_request_context_getter_);
  url_request_context_getter_ = new AwURLRequestContextGetter(this);
}

void AwBrowserContext::PreMainMessageLoopRun() {
  visitedlink_master_.reset(
      new components::VisitedLinkMaster(this, this, false));
  visitedlink_master_->Init();
}

void AwBrowserContext::AddVisitedURLs(const std::vector<GURL>& urls) {
  DCHECK(visitedlink_master_);
  visitedlink_master_->AddURLs(urls);
}

net::URLRequestContextGetter* AwBrowserContext::CreateRequestContext(
    scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
        blob_protocol_handler,
    scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
        file_system_protocol_handler,
    scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
        developer_protocol_handler,
    scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
        chrome_protocol_handler,
    scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
        chrome_devtools_protocol_handler) {
  CHECK(url_request_context_getter_);
  url_request_context_getter_->SetProtocolHandlers(
      blob_protocol_handler.Pass(), file_system_protocol_handler.Pass(),
      developer_protocol_handler.Pass(), chrome_protocol_handler.Pass(),
      chrome_devtools_protocol_handler.Pass());
  return url_request_context_getter_.get();
}

net::URLRequestContextGetter*
AwBrowserContext::CreateRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory,
    scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
        blob_protocol_handler,
    scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
        file_system_protocol_handler,
    scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
        developer_protocol_handler,
    scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
        chrome_protocol_handler,
    scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
        chrome_devtools_protocol_handler) {
  CHECK(url_request_context_getter_);
  return url_request_context_getter_.get();
}

AwQuotaManagerBridge* AwBrowserContext::GetQuotaManagerBridge() {
  if (!quota_manager_bridge_) {
    quota_manager_bridge_.reset(
        native_factory_->CreateAwQuotaManagerBridge(this));
  }
  return quota_manager_bridge_.get();
}

base::FilePath AwBrowserContext::GetPath() {
  return context_storage_path_;
}

bool AwBrowserContext::IsOffTheRecord() const {
  // Android WebView does not support off the record profile yet.
  return false;
}

net::URLRequestContextGetter* AwBrowserContext::GetRequestContext() {
  return GetDefaultStoragePartition(this)->GetURLRequestContext();
}

net::URLRequestContextGetter*
AwBrowserContext::GetRequestContextForRenderProcess(
    int renderer_child_id) {
  return GetRequestContext();
}

net::URLRequestContextGetter* AwBrowserContext::GetMediaRequestContext() {
  return GetRequestContext();
}

net::URLRequestContextGetter*
AwBrowserContext::GetMediaRequestContextForRenderProcess(
    int renderer_child_id) {
  return GetRequestContext();
}

net::URLRequestContextGetter*
AwBrowserContext::GetMediaRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory) {
  return GetRequestContext();
}

content::ResourceContext* AwBrowserContext::GetResourceContext() {
  if (!resource_context_) {
    CHECK(url_request_context_getter_);
    resource_context_.reset(new AwResourceContext(
        url_request_context_getter_.get()));
  }
  return resource_context_.get();
}

content::DownloadManagerDelegate*
AwBrowserContext::GetDownloadManagerDelegate() {
  return &download_manager_delegate_;
}

content::GeolocationPermissionContext*
AwBrowserContext::GetGeolocationPermissionContext() {
  if (!geolocation_permission_context_) {
    geolocation_permission_context_ =
        native_factory_->CreateGeolocationPermission(this);
  }
  return geolocation_permission_context_;
}

content::SpeechRecognitionPreferences*
AwBrowserContext::GetSpeechRecognitionPreferences() {
  // By default allows profanities in speech recognition if return NULL.
  return NULL;
}

quota::SpecialStoragePolicy* AwBrowserContext::GetSpecialStoragePolicy() {
  // TODO(boliu): Implement this so we are not relying on default behavior.
  NOTIMPLEMENTED();
  return NULL;
}

void AwBrowserContext::RebuildTable(
    const scoped_refptr<URLEnumerator>& enumerator) {
  // Android WebView rebuilds from WebChromeClient.getVisitedHistory. The client
  // can change in the lifetime of this WebView and may not yet be set here.
  // Therefore this initialization path is not used.
  enumerator->OnComplete(true);
}

}  // namespace android_webview
