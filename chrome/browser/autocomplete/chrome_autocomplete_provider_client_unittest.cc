// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"

#include "base/memory/ptr_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/mock_service_worker_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_storage_partition.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;

class ChromeAutocompleteProviderClientTest : public testing::Test {
 public:
  void SetUp() override {
    client_ = base::MakeUnique<ChromeAutocompleteProviderClient>(&profile_);
    storage_partition_.set_service_worker_context(&service_worker_context_);
    client_->set_storage_partition(&storage_partition_);
  }

  // Replaces the client with one using an incognito profile. Note that this is
  // a one-way operation. Once a TEST_F calls this, all interactions with
  // |client_| will be off the record.
  void GoOffTheRecord() {
    client_ = base::MakeUnique<ChromeAutocompleteProviderClient>(
        profile_.GetOffTheRecordProfile());
  }

 protected:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  std::unique_ptr<ChromeAutocompleteProviderClient> client_;
  content::MockServiceWorkerContext service_worker_context_;

 private:
  TestingProfile profile_;
  content::TestStoragePartition storage_partition_;
};

TEST_F(ChromeAutocompleteProviderClientTest, StartServiceWorker) {
  GURL destination_url("https://google.com/search?q=puppies");

  EXPECT_CALL(service_worker_context_,
              StartServiceWorkerForNavigationHint(destination_url, _))
      .Times(1);
  client_->StartServiceWorker(destination_url);
}

TEST_F(ChromeAutocompleteProviderClientTest,
       DontStartServiceWorkerInIncognito) {
  GURL destination_url("https://google.com/search?q=puppies");

  GoOffTheRecord();
  EXPECT_CALL(service_worker_context_,
              StartServiceWorkerForNavigationHint(destination_url, _))
      .Times(0);
  client_->StartServiceWorker(destination_url);
}
