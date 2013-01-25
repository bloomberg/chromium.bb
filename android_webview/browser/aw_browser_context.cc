// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_context.h"

#include "android_webview/browser/net/aw_url_request_context_getter.h"
#include "components/visitedlink/browser/visitedlink_master.h"
#include "content/public/browser/web_contents.h"

namespace android_webview {

AwBrowserContext::AwBrowserContext(
    const FilePath path,
    GeolocationPermissionFactoryFn* geolocation_permission_factory)
    : context_storage_path_(path),
      geolocation_permission_factory_(geolocation_permission_factory) {
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

void AwBrowserContext::AddVisitedURL(const GURL& url) {
  DCHECK(visitedlink_master_);
  visitedlink_master_->AddURL(url);
}

void AwBrowserContext::AddVisitedURLs(const std::vector<GURL>& urls) {
  DCHECK(visitedlink_master_);
  visitedlink_master_->AddURLs(urls);
}

FilePath AwBrowserContext::GetPath() {
  return context_storage_path_;
}

bool AwBrowserContext::IsOffTheRecord() const {
  // Android WebView does not support off the record profile yet.
  return false;
}

net::URLRequestContextGetter* AwBrowserContext::GetRequestContext() {
  DCHECK(url_request_context_getter_);
  return url_request_context_getter_;
}

net::URLRequestContextGetter*
AwBrowserContext::GetRequestContextForRenderProcess(
    int renderer_child_id) {
  return GetRequestContext();
}

net::URLRequestContextGetter*
AwBrowserContext::GetRequestContextForStoragePartition(
    const FilePath& partition_path,
    bool in_memory) {
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
    const FilePath& partition_path,
    bool in_memory) {
  return GetRequestContext();
}

content::ResourceContext* AwBrowserContext::GetResourceContext() {
  return url_request_context_getter_->GetResourceContext();
}

content::DownloadManagerDelegate*
AwBrowserContext::GetDownloadManagerDelegate() {
  return &download_manager_delegate_;
}

content::GeolocationPermissionContext*
AwBrowserContext::GetGeolocationPermissionContext() {
  if (!geolocation_permission_context_) {
    geolocation_permission_context_ = (*geolocation_permission_factory_)();
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
