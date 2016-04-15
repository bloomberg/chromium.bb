// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_CONTEXT_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_CONTEXT_H_

#include <memory>

#include "base/files/file_path.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "headless/lib/browser/headless_url_request_context_getter.h"
#include "headless/public/headless_browser.h"

namespace headless {
class HeadlessResourceContext;

class HeadlessBrowserContext : public content::BrowserContext {
 public:
  explicit HeadlessBrowserContext(const HeadlessBrowser::Options& options);
  ~HeadlessBrowserContext() override;

  // BrowserContext implementation:
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
  base::FilePath GetPath() const override;
  bool IsOffTheRecord() const override;
  content::ResourceContext* GetResourceContext() override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  content::PermissionManager* GetPermissionManager() override;
  content::BackgroundSyncController* GetBackgroundSyncController() override;
  net::URLRequestContextGetter* CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  net::URLRequestContextGetter* CreateMediaRequestContext() override;
  net::URLRequestContextGetter* CreateMediaRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory) override;

  const HeadlessBrowser::Options& options() const { return options_; }
  void SetOptionsForTesting(const HeadlessBrowser::Options& options);

 private:
  // Performs initialization of the HeadlessBrowserContext while IO is still
  // allowed on the current thread.
  void InitWhileIOAllowed();

  base::FilePath path_;
  std::unique_ptr<HeadlessResourceContext> resource_context_;
  HeadlessBrowser::Options options_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessBrowserContext);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_CONTEXT_H_
