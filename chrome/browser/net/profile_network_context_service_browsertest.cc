// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/profile_network_context_service.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/network_service.mojom.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

enum class NetworkServiceState {
  kDisabled,
  kEnabled,
};

// Most tests for this class are in NetworkContextConfigurationBrowserTest.
class ProfileNetworkContextServiceBrowsertest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<NetworkServiceState> {
 public:
  ProfileNetworkContextServiceBrowsertest() {
    EXPECT_TRUE(embedded_test_server()->Start());
  }

  ~ProfileNetworkContextServiceBrowsertest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    if (GetParam() == NetworkServiceState::kEnabled)
      feature_list_.InitAndEnableFeature(features::kNetworkService);
  }

  void SetUpOnMainThread() override {
    network_context_ = content::BrowserContext::GetDefaultStoragePartition(
                           browser()->profile())
                           ->GetNetworkContext();
    network_context_->CreateURLLoaderFactory(MakeRequest(&loader_factory_), 0);
  }

  content::mojom::URLLoaderFactory* loader_factory() const {
    return loader_factory_.get();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  content::mojom::NetworkContext* network_context_ = nullptr;
  content::mojom::URLLoaderFactoryPtr loader_factory_;
};

IN_PROC_BROWSER_TEST_P(ProfileNetworkContextServiceBrowsertest,
                       DiskCacheLocation) {
  // Run a request that caches the response, to give the network service time to
  // create a cache directory.
  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<content::SimpleURLLoader> simple_loader =
      content::SimpleURLLoader::Create();
  content::ResourceRequest request;
  request.url = embedded_test_server()->GetURL("/cachetime");
  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      request, loader_factory(), TRAFFIC_ANNOTATION_FOR_TESTS,
      simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();
  ASSERT_TRUE(simple_loader_helper.response_body());

  base::FilePath expected_cache_path;
  chrome::GetUserCacheDirectory(browser()->profile()->GetPath(),
                                &expected_cache_path);
  expected_cache_path = expected_cache_path.Append(chrome::kCacheDirname);
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  EXPECT_TRUE(base::PathExists(expected_cache_path));
}

// Test subclass that adds switches::kDiskCacheDir to the command line, to make
// sure it's respected.
class ProfileNetworkContextServiceDiskCacheDirBrowsertest
    : public ProfileNetworkContextServiceBrowsertest {
 public:
  ProfileNetworkContextServiceDiskCacheDirBrowsertest() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  ~ProfileNetworkContextServiceDiskCacheDirBrowsertest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchPath(switches::kDiskCacheDir,
                                   temp_dir_.GetPath());
  }

  const base::FilePath& TempPath() { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
};

// Makes sure switches::kDiskCacheDir is hooked up correctly.
IN_PROC_BROWSER_TEST_P(ProfileNetworkContextServiceDiskCacheDirBrowsertest,
                       DiskCacheLocation) {
  // Make sure command line switch is hooked up to the pref.
  ASSERT_EQ(TempPath(), browser()->profile()->GetPrefs()->GetFilePath(
                            prefs::kDiskCacheDir));

  // Run a request that caches the response, to give the network service time to
  // create a cache directory.
  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<content::SimpleURLLoader> simple_loader =
      content::SimpleURLLoader::Create();
  content::ResourceRequest request;
  request.url = embedded_test_server()->GetURL("/cachetime");
  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      request, loader_factory(), TRAFFIC_ANNOTATION_FOR_TESTS,
      simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();
  ASSERT_TRUE(simple_loader_helper.response_body());

  // Cache directory should now exist.
  base::FilePath expected_cache_path =
      TempPath()
          .Append(browser()->profile()->GetPath().BaseName())
          .Append(chrome::kCacheDirname);
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  EXPECT_TRUE(base::PathExists(expected_cache_path));
}

INSTANTIATE_TEST_CASE_P(
    /* No test prefix */,
    ProfileNetworkContextServiceBrowsertest,
    ::testing::Values(NetworkServiceState::kDisabled,
                      NetworkServiceState::kEnabled));

INSTANTIATE_TEST_CASE_P(
    /* No test prefix */,
    ProfileNetworkContextServiceDiskCacheDirBrowsertest,
    ::testing::Values(NetworkServiceState::kDisabled,
                      NetworkServiceState::kEnabled));

}  // namespace
