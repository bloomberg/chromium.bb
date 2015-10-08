// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_BROWSER_BLIMP_BROWSER_CONTEXT_H_
#define BLIMP_ENGINE_BROWSER_BLIMP_BROWSER_CONTEXT_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "blimp/engine/browser/blimp_url_request_context_getter.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "net/url_request/url_request_job_factory.h"

namespace net {
class NetLog;
}

namespace blimp {
namespace engine {

class BlimpResourceContext;
class PermissionManager;

class BlimpBrowserContext : public content::BrowserContext {
 public:
  // Caller owns |net_log| and ensures it out-lives this browser context.
  BlimpBrowserContext(bool off_the_record, net::NetLog* net_log);
  ~BlimpBrowserContext() override;

  // content::BrowserContext implementation.
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

  // The content of |protocol_handlers| is swapped into the returned instance.
  // Caller should take a reference to the returned instance via scoped_refptr.
  const scoped_refptr<BlimpURLRequestContextGetter>& CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors);

 private:
  // Performs initialization of the BlimpBrowserContext while IO is still
  // allowed on the current thread.
  void InitWhileIOAllowed();

  scoped_ptr<BlimpResourceContext> resource_context_;
  bool ignore_certificate_errors_;
  scoped_ptr<content::PermissionManager> permission_manager_;
  bool off_the_record_;
  net::NetLog* net_log_;
  base::FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(BlimpBrowserContext);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_BROWSER_BLIMP_BROWSER_CONTEXT_H_
