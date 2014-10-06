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
#include "net/url_request/url_request_job_factory.h"

namespace net {
class NetLog;
}

namespace content {

class DownloadManagerDelegate;
class ResourceContext;
class ShellDownloadManagerDelegate;
class ShellURLRequestContextGetter;

class ShellBrowserContext : public BrowserContext {
 public:
  ShellBrowserContext(bool off_the_record, net::NetLog* net_log);
  virtual ~ShellBrowserContext();

  void set_guest_manager_for_testing(
      BrowserPluginGuestManager* guest_manager) {
    guest_manager_ = guest_manager;
  }

  // BrowserContext implementation.
  virtual base::FilePath GetPath() const override;
  virtual bool IsOffTheRecord() const override;
  virtual DownloadManagerDelegate* GetDownloadManagerDelegate() override;
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
  virtual ResourceContext* GetResourceContext() override;
  virtual BrowserPluginGuestManager* GetGuestManager() override;
  virtual storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  virtual PushMessagingService* GetPushMessagingService() override;
  virtual SSLHostStateDelegate* GetSSLHostStateDelegate() override;

  net::URLRequestContextGetter* CreateRequestContext(
      ProtocolHandlerMap* protocol_handlers,
      URLRequestInterceptorScopedVector request_interceptors);
  net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory,
      ProtocolHandlerMap* protocol_handlers,
      URLRequestInterceptorScopedVector request_interceptors);

 private:
  class ShellResourceContext;

  // Performs initialization of the ShellBrowserContext while IO is still
  // allowed on the current thread.
  void InitWhileIOAllowed();

  bool off_the_record_;
  net::NetLog* net_log_;
  bool ignore_certificate_errors_;
  base::FilePath path_;
  BrowserPluginGuestManager* guest_manager_;
  scoped_ptr<ShellResourceContext> resource_context_;
  scoped_ptr<ShellDownloadManagerDelegate> download_manager_delegate_;
  scoped_refptr<ShellURLRequestContextGetter> url_request_getter_;

  DISALLOW_COPY_AND_ASSIGN(ShellBrowserContext);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_BROWSER_CONTEXT_H_
