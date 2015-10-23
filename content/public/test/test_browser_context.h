// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_BROWSER_CONTEXT_H_
#define CONTENT_PUBLIC_TEST_TEST_BROWSER_CONTEXT_H_

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_context.h"

namespace content {
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

 private:
  base::ScopedTempDir browser_context_dir_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;
  scoped_ptr<MockResourceContext> resource_context_;
  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;
  scoped_ptr<MockSSLHostStateDelegate> ssl_host_state_delegate_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserContext);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_BROWSER_CONTEXT_H_
