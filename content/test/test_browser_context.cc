// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_browser_context.h"

#include "base/file_path.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "content/browser/mock_resource_context.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::DownloadManager;
using content::HostZoomMap;

TestBrowserContext::TestBrowserContext() {
  EXPECT_TRUE(browser_context_dir_.CreateUniqueTempDir());
}

TestBrowserContext::~TestBrowserContext() {
}

FilePath TestBrowserContext::TakePath() {
  return browser_context_dir_.Take();
}

FilePath TestBrowserContext::GetPath() {
  return browser_context_dir_.path();
}

bool TestBrowserContext::IsOffTheRecord() {
  return false;
}

DownloadManager* TestBrowserContext::GetDownloadManager() {
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

content::ResourceContext* TestBrowserContext::GetResourceContext() {
  // TODO(phajdan.jr): Get rid of this nasty global.
  return content::MockResourceContext::GetInstance();
}

HostZoomMap* TestBrowserContext::GetHostZoomMap() {
  return NULL;
}

content::GeolocationPermissionContext*
TestBrowserContext::GetGeolocationPermissionContext() {
  return NULL;
}

content::SpeechInputPreferences*
TestBrowserContext::GetSpeechInputPreferences() {
  return NULL;
}

bool TestBrowserContext::DidLastSessionExitCleanly() {
  return true;
}

quota::SpecialStoragePolicy* TestBrowserContext::GetSpecialStoragePolicy() {
  return NULL;
}
