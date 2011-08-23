// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_BROWSER_CONTEXT_H_
#define CONTENT_TEST_TEST_BROWSER_CONTEXT_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/browser/browser_context.h"

class TestBrowserContext : public content::BrowserContext {
 public:
  TestBrowserContext();
  virtual ~TestBrowserContext();

  virtual FilePath GetPath() OVERRIDE;
  virtual bool IsOffTheRecord() OVERRIDE;
  virtual webkit_database::DatabaseTracker* GetDatabaseTracker() OVERRIDE;
  virtual SSLHostState* GetSSLHostState() OVERRIDE;
  virtual DownloadManager* GetDownloadManager() OVERRIDE;
  virtual bool HasCreatedDownloadManager() const OVERRIDE;
  virtual quota::QuotaManager* GetQuotaManager() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id) OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContextForMedia() OVERRIDE;
  virtual const content::ResourceContext& GetResourceContext() OVERRIDE;
  virtual HostZoomMap* GetHostZoomMap() OVERRIDE;
  virtual GeolocationPermissionContext* GetGeolocationPermissionContext()
      OVERRIDE;
  virtual bool DidLastSessionExitCleanly() OVERRIDE;
  virtual WebKitContext* GetWebKitContext() OVERRIDE;
  virtual ChromeBlobStorageContext* GetBlobStorageContext() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestBrowserContext);
};

#endif  // CONTENT_TEST_TEST_BROWSER_CONTEXT_H_
