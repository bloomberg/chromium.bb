// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_data_manager.h"

#include "base/files/file_path.h"
#include "content/browser/background_fetch/background_fetch_context.h"
#include "content/browser/background_fetch/fetch_request.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kOrigin[] = "https://example.com/";
const char kResource[] = "https://example.com/resource.html";
const char kTag[] = "TestRequestTag";
constexpr int64_t kServiceWorkerRegistrationId = 9001;

}  // namespace

namespace content {

class BackgroundFetchDataManagerTest : public testing::Test {
 protected:
  BackgroundFetchDataManagerTest()
      : helper_(base::FilePath()),
        background_fetch_context_(
            new BackgroundFetchContext(&browser_context_,
                                       helper_.context_wrapper())) {}

  BackgroundFetchDataManager* GetDataManager() const {
    return background_fetch_context_->GetDataManagerForTesting();
  }

 private:
  TestBrowserThreadBundle browser_thread_bundle_;
  TestBrowserContext browser_context_;
  EmbeddedWorkerTestHelper helper_;

  scoped_refptr<BackgroundFetchContext> background_fetch_context_;
};

TEST_F(BackgroundFetchDataManagerTest, AddRequest) {
  BackgroundFetchDataManager* data_manager = GetDataManager();
  FetchRequest request(url::Origin(GURL(kOrigin)), GURL(kResource),
                       kServiceWorkerRegistrationId, kTag);

  data_manager->CreateRequest(request);

  // TODO(harkness) There's no output to test yet. Once there is, check that the
  // request was actually added.
}

}  // namespace content
