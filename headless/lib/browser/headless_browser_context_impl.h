// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_CONTEXT_IMPL_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_CONTEXT_IMPL_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/files/file_path.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "headless/lib/browser/headless_browser_context_options.h"
#include "headless/lib/browser/headless_url_request_context_getter.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_browser_context.h"

namespace headless {
class HeadlessBrowserImpl;
class HeadlessResourceContext;
class HeadlessWebContentsImpl;

class HeadlessBrowserContextImpl : public HeadlessBrowserContext,
                                   public content::BrowserContext {
 public:
  HeadlessBrowserContextImpl(HeadlessBrowserImpl* browser,
                             HeadlessBrowserContextOptions context_options);
  ~HeadlessBrowserContextImpl() override;

  static HeadlessBrowserContextImpl* From(
      HeadlessBrowserContext* browser_context);

  // HeadlessBrowserContext implementation:
  HeadlessWebContents::Builder CreateWebContentsBuilder() override;
  std::vector<HeadlessWebContents*> GetAllWebContents() override;

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

  void RegisterWebContents(HeadlessWebContentsImpl*);
  void UnregisterWebContents(HeadlessWebContentsImpl*);

  HeadlessBrowserImpl* browser() const;
  const HeadlessBrowserContextOptions* options() const;

 private:
  // Performs initialization of the HeadlessBrowserContextImpl while IO is still
  // allowed on the current thread.
  void InitWhileIOAllowed();

  HeadlessBrowserImpl* browser_;  // Not owned.
  HeadlessBrowserContextOptions context_options_;
  std::unique_ptr<HeadlessResourceContext> resource_context_;
  base::FilePath path_;

  // Web contents are owned by |HeadlessBrowser|, we keep track of
  // contents corresponding to this context to delete them when context goes
  // away.
  std::unordered_map<std::string, HeadlessWebContents*> web_contents_map_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessBrowserContextImpl);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_CONTEXT_IMPL_H_
