// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_browser_context.h"

#include "base/file_path.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "content/browser/mock_resource_context.h"

TestBrowserContext::TestBrowserContext() {
}

TestBrowserContext::~TestBrowserContext() {
}

FilePath TestBrowserContext::GetPath() {
  return FilePath();
}

bool TestBrowserContext::IsOffTheRecord() {
  return false;
}

webkit_database::DatabaseTracker* TestBrowserContext::GetDatabaseTracker() {
  return NULL;
}

SSLHostState* TestBrowserContext::GetSSLHostState() {
  return NULL;
}

DownloadManager* TestBrowserContext::GetDownloadManager() {
  return NULL;
}

bool TestBrowserContext::HasCreatedDownloadManager() const {
  return false;
}

quota::QuotaManager* TestBrowserContext::GetQuotaManager() {
  return NULL;
}

net::URLRequestContextGetter* TestBrowserContext::GetRequestContext() {
  return NULL;
}

net::URLRequestContextGetter*
TestBrowserContext::GetRequestContextForRenderProcess(int renderer_child_id) {
  return NULL;
}

net::URLRequestContextGetter* TestBrowserContext::GetRequestContextForMedia() {
  return NULL;
}

const content::ResourceContext& TestBrowserContext::GetResourceContext() {
  // TODO(phajdan.jr): Get rid of this nasty global.
  return *content::MockResourceContext::GetInstance();
}

HostZoomMap* TestBrowserContext::GetHostZoomMap() {
  return NULL;
}

GeolocationPermissionContext*
TestBrowserContext::GetGeolocationPermissionContext() {
  return NULL;
}

bool TestBrowserContext::DidLastSessionExitCleanly() {
  return true;
}

WebKitContext* TestBrowserContext::GetWebKitContext() {
  if (webkit_context_ == NULL) {
    webkit_context_ = new WebKitContext(
          IsOffTheRecord(), GetPath(),
          NULL, false, NULL, NULL);
  }
  return webkit_context_;
}

ChromeBlobStorageContext* TestBrowserContext::GetBlobStorageContext() {
  return NULL;
}
