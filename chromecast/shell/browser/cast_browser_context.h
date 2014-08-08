// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_SHELL_BROWSER_CAST_BROWSER_CONTEXT_H_
#define CHROMECAST_SHELL_BROWSER_CAST_BROWSER_CONTEXT_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"

namespace chromecast {
namespace shell {

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
  virtual base::FilePath GetPath() const OVERRIDE;
  virtual bool IsOffTheRecord() const OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id) OVERRIDE;
  virtual net::URLRequestContextGetter* GetMediaRequestContext() OVERRIDE;
  virtual net::URLRequestContextGetter* GetMediaRequestContextForRenderProcess(
      int renderer_child_id) OVERRIDE;
  virtual net::URLRequestContextGetter*
      GetMediaRequestContextForStoragePartition(
          const base::FilePath& partition_path,
          bool in_memory) OVERRIDE;
  virtual content::ResourceContext* GetResourceContext() OVERRIDE;
  virtual content::DownloadManagerDelegate*
      GetDownloadManagerDelegate() OVERRIDE;
  virtual content::BrowserPluginGuestManager* GetGuestManager() OVERRIDE;
  virtual quota::SpecialStoragePolicy* GetSpecialStoragePolicy() OVERRIDE;
  virtual content::PushMessagingService* GetPushMessagingService() OVERRIDE;
  virtual content::SSLHostStateDelegate* GetSSLHostStateDelegate() OVERRIDE;

 private:
  class CastResourceContext;

  // Performs initialization of the CastBrowserContext while IO is still
  // allowed on the current thread.
  void InitWhileIOAllowed();

  URLRequestContextFactory* const url_request_context_factory_;
  base::FilePath path_;
  scoped_ptr<CastResourceContext> resource_context_;

  DISALLOW_COPY_AND_ASSIGN(CastBrowserContext);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_SHELL_BROWSER_CAST_BROWSER_CONTEXT_H_
