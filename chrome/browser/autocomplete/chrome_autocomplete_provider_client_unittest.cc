// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"

#include <memory>

#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/fake_service_worker_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_storage_partition.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class ChromeAutocompleteProviderClientTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    client_ =
        std::make_unique<ChromeAutocompleteProviderClient>(profile_.get());
    storage_partition_.set_service_worker_context(&service_worker_context_);
    client_->set_storage_partition(&storage_partition_);
  }

  // Replaces the client with one using an incognito profile. Note that this is
  // a one-way operation. Once a TEST_F calls this, all interactions with
  // |client_| will be off the record.
  void GoOffTheRecord() {
    client_ = std::make_unique<ChromeAutocompleteProviderClient>(
        profile_->GetOffTheRecordProfile());
  }

 protected:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ChromeAutocompleteProviderClient> client_;
  content::FakeServiceWorkerContext service_worker_context_;

 private:
  content::TestStoragePartition storage_partition_;
};

TEST_F(ChromeAutocompleteProviderClientTest, StartServiceWorker) {
  GURL destination_url("https://google.com/search?q=puppies");

  client_->StartServiceWorker(destination_url);
  EXPECT_TRUE(service_worker_context_
                  .start_service_worker_for_navigation_hint_called());
}

TEST_F(ChromeAutocompleteProviderClientTest,
       DontStartServiceWorkerInIncognito) {
  GURL destination_url("https://google.com/search?q=puppies");

  GoOffTheRecord();
  client_->StartServiceWorker(destination_url);
  EXPECT_FALSE(service_worker_context_
                   .start_service_worker_for_navigation_hint_called());
}

TEST_F(ChromeAutocompleteProviderClientTest,
       DontStartServiceWorkerIfSuggestDisabled) {
  GURL destination_url("https://google.com/search?q=puppies");

  profile_->GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled, false);
  client_->StartServiceWorker(destination_url);
  EXPECT_FALSE(service_worker_context_
                   .start_service_worker_for_navigation_hint_called());
}
