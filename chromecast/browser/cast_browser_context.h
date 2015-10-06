// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_BROWSER_CONTEXT_H_
#define CHROMECAST_BROWSER_CAST_BROWSER_CONTEXT_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"

namespace chromecast {
namespace shell {

class CastDownloadManagerDelegate;
class URLRequestContextFactory;

// Chromecast does not currently support multiple profiles.  So there is a
// single BrowserContext for all chromecast renderers.
// There is no support for PartitionStorage.
class CastBrowserContext : public content::BrowserContext {
 public:
  explicit CastBrowserContext(
      URLRequestContextFactory* url_request_context_factory);
  ~CastBrowserContext() override;

  // BrowserContext implementation:
  scoped_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
  base::FilePath GetPath() const override;
  bool IsOffTheRecord() const override;
  net::URLRequestContextGetter* GetRequestContext() override;
  net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id) override;
  net::URLRequestContextGetter* GetMediaRequestContext() override;
  net::URLRequestContextGetter* GetMediaRequestContextForRenderProcess(
      int renderer_child_id) override;
  net::URLRequestContextGetter* GetMediaRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory) override;
  content::ResourceContext* GetResourceContext() override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  content::PermissionManager* GetPermissionManager() override;
  content::BackgroundSyncController* GetBackgroundSyncController() override;

  net::URLRequestContextGetter* GetSystemRequestContext();

 private:
  class CastResourceContext;

  // Performs initialization of the CastBrowserContext while IO is still
  // allowed on the current thread.
  void InitWhileIOAllowed();

  URLRequestContextFactory* const url_request_context_factory_;
  base::FilePath path_;
  scoped_ptr<CastResourceContext> resource_context_;
  scoped_ptr<CastDownloadManagerDelegate> download_manager_delegate_;
  scoped_ptr<content::PermissionManager> permission_manager_;

  DISALLOW_COPY_AND_ASSIGN(CastBrowserContext);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_BROWSER_CONTEXT_H_
