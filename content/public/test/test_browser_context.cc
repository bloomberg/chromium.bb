// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_browser_context.h"

#include "base/files/file_path.h"
#include "base/test/null_task_runner.h"
#include "content/public/test/mock_resource_context.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/quota/special_storage_policy.h"

namespace {

class TestContextURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  TestContextURLRequestContextGetter()
      : null_task_runner_(new base::NullTaskRunner) {
  }

  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE {
    return &context_;
  }

  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const OVERRIDE {
    return null_task_runner_;
  }

 private:
  virtual ~TestContextURLRequestContextGetter() {}

  net::TestURLRequestContext context_;
  scoped_refptr<base::SingleThreadTaskRunner> null_task_runner_;
};

}  // namespace

namespace content {

TestBrowserContext::TestBrowserContext() {
  EXPECT_TRUE(browser_context_dir_.CreateUniqueTempDir());
}

TestBrowserContext::~TestBrowserContext() {
}

base::FilePath TestBrowserContext::TakePath() {
  return browser_context_dir_.Take();
}

void TestBrowserContext::SetSpecialStoragePolicy(
    quota::SpecialStoragePolicy* policy) {
  special_storage_policy_ = policy;
}

base::FilePath TestBrowserContext::GetPath() const {
  return browser_context_dir_.path();
}

bool TestBrowserContext::IsOffTheRecord() const {
  return false;
}

DownloadManagerDelegate* TestBrowserContext::GetDownloadManagerDelegate() {
  return NULL;
}

net::URLRequestContextGetter* TestBrowserContext::GetRequestContext() {
  if (!request_context_.get()) {
    request_context_ = new TestContextURLRequestContextGetter();
  }
  return request_context_.get();
}

net::URLRequestContextGetter*
TestBrowserContext::GetRequestContextForRenderProcess(int renderer_child_id) {
  return NULL;
}

net::URLRequestContextGetter* TestBrowserContext::GetMediaRequestContext() {
  return NULL;
}

net::URLRequestContextGetter*
TestBrowserContext::GetMediaRequestContextForRenderProcess(
    int renderer_child_id) {
  return NULL;
}

net::URLRequestContextGetter*
TestBrowserContext::GetMediaRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory) {
  return NULL;
}

ResourceContext* TestBrowserContext::GetResourceContext() {
  if (!resource_context_)
    resource_context_.reset(new MockResourceContext(
        GetRequestContext()->GetURLRequestContext()));
  return resource_context_.get();
}

BrowserPluginGuestManager* TestBrowserContext::GetGuestManager() {
  return NULL;
}

quota::SpecialStoragePolicy* TestBrowserContext::GetSpecialStoragePolicy() {
  return special_storage_policy_.get();
}

PushMessagingService* TestBrowserContext::GetPushMessagingService() {
  return NULL;
}

SSLHostStateDelegate* TestBrowserContext::GetSSLHostStateDelegate() {
  return NULL;
}

}  // namespace content
