// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_BROWSER_CONTEXT_H_
#define CONTENT_PUBLIC_TEST_TEST_BROWSER_CONTEXT_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_context.h"

namespace content {

class MockBackgroundSyncController;
class MockResourceContext;
class MockSSLHostStateDelegate;
class ZoomLevelDelegate;

class TestBrowserContext : public BrowserContext {
 public:
  TestBrowserContext();
  ~TestBrowserContext() override;

  // Takes ownership of the temporary directory so that it's not deleted when
  // this object is destructed.
  base::FilePath TakePath();

  void SetSpecialStoragePolicy(storage::SpecialStoragePolicy* policy);
  void SetPermissionManager(
      std::unique_ptr<PermissionManager> permission_manager);
  net::URLRequestContextGetter* GetRequestContext();

  base::FilePath GetPath() const override;
  std::unique_ptr<ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
  bool IsOffTheRecord() const override;
  DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  ResourceContext* GetResourceContext() override;
  BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  PushMessagingService* GetPushMessagingService() override;
  SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  PermissionManager* GetPermissionManager() override;
  BackgroundSyncController* GetBackgroundSyncController() override;
  net::URLRequestContextGetter* CreateRequestContext(
      ProtocolHandlerMap* protocol_handlers,
      URLRequestInterceptorScopedVector request_interceptors) override;
  net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory,
      ProtocolHandlerMap* protocol_handlers,
      URLRequestInterceptorScopedVector request_interceptors)  override;
  net::URLRequestContextGetter* CreateMediaRequestContext() override;
  net::URLRequestContextGetter* CreateMediaRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory) override;

 private:
  base::ScopedTempDir browser_context_dir_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;
  std::unique_ptr<MockResourceContext> resource_context_;
  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;
  std::unique_ptr<MockSSLHostStateDelegate> ssl_host_state_delegate_;
  std::unique_ptr<PermissionManager> permission_manager_;
  std::unique_ptr<MockBackgroundSyncController> background_sync_controller_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserContext);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_BROWSER_CONTEXT_H_
