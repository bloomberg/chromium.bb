// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_browser_context.h"

#include "base/file_path.h"
#include "content/public/test/mock_resource_context.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/quota/special_storage_policy.h"

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
  return NULL;
}

net::URLRequestContextGetter*
TestBrowserContext::GetRequestContextForRenderProcess(int renderer_child_id) {
  return NULL;
}

net::URLRequestContextGetter* TestBrowserContext::GetRequestContextForMedia() {
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
