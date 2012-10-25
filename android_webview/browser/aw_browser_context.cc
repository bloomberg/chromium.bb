// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_context.h"

#include "android_webview/browser/net/aw_url_request_context_getter.h"

namespace android_webview {

AwBrowserContext::AwBrowserContext(const FilePath path)
    : context_storage_path_(path) {
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
    const std::string& partition_id) {
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
        const std::string& partition_id) {
  return GetRequestContext();
}

content::ResourceContext* AwBrowserContext::GetResourceContext() {
  return url_request_context_getter_->GetResourceContext();
}

content::DownloadManagerDelegate*
AwBrowserContext::GetDownloadManagerDelegate() {
  // TODO(boliu): Implement intercepting downloads for DownloadListener and
  // maybe put a NOTREACHED if it is indeed never needed.
  NOTIMPLEMENTED();
  return NULL;
}

content::GeolocationPermissionContext*
AwBrowserContext::GetGeolocationPermissionContext() {
  // TODO(boliu): Implement this to power WebSettings.setGeolocationEnabled
  // setting.
  NOTIMPLEMENTED();
  return NULL;
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
