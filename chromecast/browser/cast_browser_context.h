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
  virtual ~CastBrowserContext();

  // BrowserContext implementation:
  virtual base::FilePath GetPath() const override;
  virtual bool IsOffTheRecord() const override;
  virtual net::URLRequestContextGetter* GetRequestContext() override;
  virtual net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id) override;
  virtual net::URLRequestContextGetter* GetMediaRequestContext() override;
  virtual net::URLRequestContextGetter* GetMediaRequestContextForRenderProcess(
      int renderer_child_id) override;
  virtual net::URLRequestContextGetter*
      GetMediaRequestContextForStoragePartition(
          const base::FilePath& partition_path,
          bool in_memory) override;
  virtual content::ResourceContext* GetResourceContext() override;
  virtual content::DownloadManagerDelegate*
      GetDownloadManagerDelegate() override;
  virtual content::BrowserPluginGuestManager* GetGuestManager() override;
  virtual storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  virtual content::PushMessagingService* GetPushMessagingService() override;
  virtual content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;

 private:
  class CastResourceContext;

  // Performs initialization of the CastBrowserContext while IO is still
  // allowed on the current thread.
  void InitWhileIOAllowed();

  URLRequestContextFactory* const url_request_context_factory_;
  base::FilePath path_;
  scoped_ptr<CastResourceContext> resource_context_;
  scoped_ptr<CastDownloadManagerDelegate> download_manager_delegate_;

  DISALLOW_COPY_AND_ASSIGN(CastBrowserContext);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_BROWSER_CONTEXT_H_
