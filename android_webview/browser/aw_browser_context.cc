// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_context.h"

#include "android_webview/browser/net/aw_url_request_context_getter.h"

namespace android_webview {

AwBrowserContext::AwBrowserContext(
    const FilePath path,
    GeolocationPermissionFactoryFn* geolocation_permission_factory)
    : context_storage_path_(path),
      geolocation_permission_factory_(geolocation_permission_factory) {
}

AwBrowserContext::~AwBrowserContext() {
}

void AwBrowserContext::InitializeBeforeThreadCreation() {
  DCHECK(!url_request_context_getter_);
  url_request_context_getter_ = new AwURLRequestContextGetter(this);
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

}  // namespace android_webview
