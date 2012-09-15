// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_browser_context.h"

#include "base/file_path.h"
#include "content/public/test/mock_resource_context.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/quota/special_storage_policy.h"

namespace {

// A silly class to satisfy net::URLRequestsContextGetter requirement
// for a task runner. Threading requirements don't matter for this
// test scaffolding.
class AnyThreadNonTaskRunner : public base::SingleThreadTaskRunner {
 public:
  virtual bool RunsTasksOnCurrentThread() const OVERRIDE {
    return true;
  }

  virtual bool PostDelayedTask(const tracked_objects::Location& from_here,
                               const base::Closure& task,
                               base::TimeDelta delay) OVERRIDE {
    NOTREACHED();
    return false;
  }

  virtual bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay) OVERRIDE {
    NOTREACHED();
    return false;
  }

 private:
  virtual ~AnyThreadNonTaskRunner() {}
};

class TestContextURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  TestContextURLRequestContextGetter(net::URLRequestContext* context)
      : context_(context),
        any_thread_non_task_runner_(new AnyThreadNonTaskRunner) {
  }

  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE {
    return context_;
  }

  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const OVERRIDE {
    return any_thread_non_task_runner_;
  }

 private:
  virtual ~TestContextURLRequestContextGetter() {}

  net::URLRequestContext* context_;
  scoped_refptr<base::SingleThreadTaskRunner> any_thread_non_task_runner_;
};

}

namespace content {

TestBrowserContext::TestBrowserContext() {
  EXPECT_TRUE(browser_context_dir_.CreateUniqueTempDir());
}

TestBrowserContext::~TestBrowserContext() {
}

FilePath TestBrowserContext::TakePath() {
  return browser_context_dir_.Take();
}

void TestBrowserContext::SetSpecialStoragePolicy(
    quota::SpecialStoragePolicy* policy) {
  special_storage_policy_ = policy;
}

FilePath TestBrowserContext::GetPath() {
  return browser_context_dir_.path();
}

bool TestBrowserContext::IsOffTheRecord() const {
  return false;
}

DownloadManagerDelegate* TestBrowserContext::GetDownloadManagerDelegate() {
  return NULL;
}

net::URLRequestContextGetter* TestBrowserContext::GetRequestContext() {
  return new TestContextURLRequestContextGetter(
      GetResourceContext()->GetRequestContext());
}

net::URLRequestContextGetter*
TestBrowserContext::GetRequestContextForRenderProcess(int renderer_child_id) {
  return NULL;
}


net::URLRequestContextGetter*
TestBrowserContext::GetRequestContextForStoragePartition(
    const std::string& partition_id) {
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

ResourceContext* TestBrowserContext::GetResourceContext() {
  if (!resource_context_.get())
    resource_context_.reset(new MockResourceContext());
  return resource_context_.get();
}

GeolocationPermissionContext*
    TestBrowserContext::GetGeolocationPermissionContext() {
  return NULL;
}

SpeechRecognitionPreferences*
    TestBrowserContext::GetSpeechRecognitionPreferences() {
  return NULL;
}

bool TestBrowserContext::DidLastSessionExitCleanly() {
  return true;
}

quota::SpecialStoragePolicy* TestBrowserContext::GetSpecialStoragePolicy() {
  return special_storage_policy_.get();
}

}  // namespace content
