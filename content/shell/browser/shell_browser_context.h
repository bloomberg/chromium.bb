// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_BROWSER_CONTEXT_H_
#define CONTENT_SHELL_BROWSER_SHELL_BROWSER_CONTEXT_H_

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "content/shell/browser/shell_url_request_context_getter.h"
#include "net/url_request/url_request_job_factory.h"

namespace net {
class NetLog;
}

namespace content {

class BackgroundSyncController;
class DownloadManagerDelegate;
class PermissionManager;
class ShellDownloadManagerDelegate;
class ZoomLevelDelegate;

class ShellBrowserContext : public BrowserContext {
 public:
  ShellBrowserContext(bool off_the_record, net::NetLog* net_log);
  ~ShellBrowserContext() override;

  void set_guest_manager_for_testing(
      BrowserPluginGuestManager* guest_manager) {
    guest_manager_ = guest_manager;
  }

  // BrowserContext implementation.
  base::FilePath GetPath() const override;
  scoped_ptr<ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
  bool IsOffTheRecord() const override;
  DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  net::URLRequestContextGetter* GetRequestContext() override;
  net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id) override;
  net::URLRequestContextGetter* GetMediaRequestContext() override;
  net::URLRequestContextGetter* GetMediaRequestContextForRenderProcess(
      int renderer_child_id) override;
  net::URLRequestContextGetter* GetMediaRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory) override;
  ResourceContext* GetResourceContext() override;
  BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  PushMessagingService* GetPushMessagingService() override;
  SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  PermissionManager* GetPermissionManager() override;
  BackgroundSyncController* GetBackgroundSyncController() override;

  net::URLRequestContextGetter* CreateRequestContext(
      ProtocolHandlerMap* protocol_handlers,
      URLRequestInterceptorScopedVector request_interceptors);
  net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory,
      ProtocolHandlerMap* protocol_handlers,
      URLRequestInterceptorScopedVector request_interceptors);

 protected:
  // Contains URLRequestContextGetter required for resource loading.
  class ShellResourceContext : public ResourceContext {
   public:
    ShellResourceContext();
    ~ShellResourceContext() override;

    // ResourceContext implementation:
    net::HostResolver* GetHostResolver() override;
    net::URLRequestContext* GetRequestContext() override;

    void set_url_request_context_getter(ShellURLRequestContextGetter* getter) {
      getter_ = getter;
    }

  private:
    ShellURLRequestContextGetter* getter_;

    DISALLOW_COPY_AND_ASSIGN(ShellResourceContext);
  };

  ShellURLRequestContextGetter* url_request_context_getter() {
    return url_request_getter_.get();
  }

  // Used by ShellBrowserContext to initiate and set different types of
  // URLRequestContextGetter.
  virtual ShellURLRequestContextGetter* CreateURLRequestContextGetter(
      ProtocolHandlerMap* protocol_handlers,
      URLRequestInterceptorScopedVector request_interceptors);
  void set_url_request_context_getter(ShellURLRequestContextGetter* getter) {
    url_request_getter_ = getter;
  }

  bool ignore_certificate_errors() const { return ignore_certificate_errors_; }
  net::NetLog* net_log() const { return net_log_; }

  scoped_ptr<ShellResourceContext> resource_context_;
  bool ignore_certificate_errors_;
  scoped_ptr<ShellDownloadManagerDelegate> download_manager_delegate_;
  scoped_ptr<PermissionManager> permission_manager_;

 private:
  // Performs initialization of the ShellBrowserContext while IO is still
  // allowed on the current thread.
  void InitWhileIOAllowed();

  bool off_the_record_;
  net::NetLog* net_log_;
  base::FilePath path_;
  BrowserPluginGuestManager* guest_manager_;
  scoped_refptr<ShellURLRequestContextGetter> url_request_getter_;

  DISALLOW_COPY_AND_ASSIGN(ShellBrowserContext);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_BROWSER_CONTEXT_H_
