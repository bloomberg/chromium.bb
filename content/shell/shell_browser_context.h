// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_SHELL_BROWSER_CONTEXT_H_
#define CONTENT_SHELL_SHELL_BROWSER_CONTEXT_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/browser_context.h"

class DownloadManager;
class DownloadManagerDelegate;
class DownloadStatusUpdater;
class GeolocationPermissionContext;
class HostZoomMap;
class SSLHostState;

namespace content {

class ResourceContext;
class ShellBrowserMainParts;

class ShellBrowserContext : public BrowserContext {
 public:
  explicit ShellBrowserContext(ShellBrowserMainParts* shell_main_parts);
  virtual ~ShellBrowserContext();

  // BrowserContext implementation.
  virtual FilePath GetPath() OVERRIDE;
  virtual bool IsOffTheRecord() OVERRIDE;
  virtual SSLHostState* GetSSLHostState() OVERRIDE;
  virtual DownloadManager* GetDownloadManager() OVERRIDE;
  virtual bool HasCreatedDownloadManager() const OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id) OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContextForMedia() OVERRIDE;
  virtual const ResourceContext& GetResourceContext() OVERRIDE;
  virtual HostZoomMap* GetHostZoomMap() OVERRIDE;
  virtual GeolocationPermissionContext*
      GetGeolocationPermissionContext() OVERRIDE;
  virtual bool DidLastSessionExitCleanly() OVERRIDE;
  virtual quota::QuotaManager* GetQuotaManager() OVERRIDE;
  virtual WebKitContext* GetWebKitContext() OVERRIDE;
  virtual webkit_database::DatabaseTracker* GetDatabaseTracker() OVERRIDE;
  virtual ChromeBlobStorageContext* GetBlobStorageContext() OVERRIDE;
  virtual ChromeAppCacheService* GetAppCacheService() OVERRIDE;
  virtual fileapi::FileSystemContext* GetFileSystemContext() OVERRIDE;

 private:
  void CreateQuotaManagerAndClients();

  scoped_ptr<ResourceContext> resource_context_;
  scoped_ptr<SSLHostState> ssl_host_state_;
  scoped_ptr<DownloadStatusUpdater> download_status_updater_;
  scoped_ptr<DownloadManagerDelegate> download_manager_delegate_;
  scoped_refptr<DownloadManager> download_manager_;
  scoped_refptr<net::URLRequestContextGetter> url_request_getter_;
  scoped_refptr<HostZoomMap> host_zoom_map_;
  scoped_refptr<GeolocationPermissionContext> geolocation_permission_context_;
  scoped_refptr<WebKitContext> webkit_context_;
  scoped_refptr<ChromeAppCacheService> appcache_service_;
  scoped_refptr<webkit_database::DatabaseTracker> db_tracker_;
  scoped_refptr<fileapi::FileSystemContext> file_system_context_;
  scoped_refptr<quota::QuotaManager> quota_manager_;
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;

  ShellBrowserMainParts* shell_main_parts_;

  DISALLOW_COPY_AND_ASSIGN(ShellBrowserContext);
};

}  // namespace content

#endif  // CONTENT_SHELL_SHELL_BROWSER_CONTEXT_H_
