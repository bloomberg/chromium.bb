// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/unverified_download_policy.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing_db/test_database_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {

namespace {

class FakeDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  bool IsSupported() const override { return true; }

  bool MatchDownloadWhitelistUrl(const GURL& url) override {
    return url.SchemeIsHTTPOrHTTPS() && url.host() == "supported.example.com";
  }

 protected:
  ~FakeDatabaseManager() override {}
};

class TestSafeBrowsingService : public SafeBrowsingService {
 public:
  SafeBrowsingDatabaseManager* CreateDatabaseManager() override {
    return new FakeDatabaseManager();
  }

 protected:
  ~TestSafeBrowsingService() override {}

  SafeBrowsingProtocolManagerDelegate* GetProtocolManagerDelegate() override {
    // Our FakeDatabaseManager doesn't implement this delegate.
    return NULL;
  }
};

class TestSafeBrowsingServiceFactory : public SafeBrowsingServiceFactory {
 public:
  SafeBrowsingService* CreateSafeBrowsingService() override {
    return new TestSafeBrowsingService();
  }
};

class CompletionCallback {
 public:
  CompletionCallback() {}

  UnverifiedDownloadCheckCompletionCallback GetCallback() {
    completed_ = false;
    completion_closure_.Reset();
    return base::Bind(&CompletionCallback::OnUnverifiedDownloadPolicyAvailable,
                      base::Unretained(this));
  }

  UnverifiedDownloadPolicy WaitForResult() {
    if (!completed_) {
      base::RunLoop run_loop;
      completion_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    return result_;
  }

 private:
  void OnUnverifiedDownloadPolicyAvailable(UnverifiedDownloadPolicy policy) {
    result_ = policy;
    completed_ = true;
    if (!completion_closure_.is_null())
      base::ResetAndReturn(&completion_closure_).Run();
  }

  bool completed_ = false;
  base::Closure completion_closure_;
  UnverifiedDownloadPolicy result_ = UnverifiedDownloadPolicy::DISALLOWED;
};

class UnverifiedDownloadPolicyTest : public ::testing::Test {
 public:
  void SetUp() override {
    SafeBrowsingService::RegisterFactory(&test_safe_browsing_service_factory_);
    testing_safe_browsing_service_ =
        SafeBrowsingService::CreateSafeBrowsingService();
    TestingBrowserProcess* browser_process = TestingBrowserProcess::GetGlobal();
    browser_process->SetSafeBrowsingService(
        testing_safe_browsing_service_.get());
    browser_process->safe_browsing_service()->Initialize();
    base::RunLoop().RunUntilIdle();

    testing_profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(testing_profile_manager_->SetUp());

    testing_profile_ = testing_profile_manager_->CreateTestingProfile("foo");
    testing_profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  }

  void TearDown() override {
    TestingBrowserProcess* browser_process = TestingBrowserProcess::GetGlobal();
    browser_process->safe_browsing_service()->ShutDown();
    browser_process->SetSafeBrowsingService(nullptr);
    testing_safe_browsing_service_ = nullptr;
    SafeBrowsingService::RegisterFactory(nullptr);
    base::RunLoop().RunUntilIdle();
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  TestSafeBrowsingServiceFactory test_safe_browsing_service_factory_;
  scoped_ptr<TestingProfileManager> testing_profile_manager_;
  scoped_refptr<SafeBrowsingService> testing_safe_browsing_service_;
  TestingProfile* testing_profile_ = nullptr;
};

}  // namespace

// Verify that SafeBrowsing whitelists can override field trials.
TEST_F(UnverifiedDownloadPolicyTest, Whitelist) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisallowUncheckedDangerousDownloads);

  base::FilePath test_file_path(FILE_PATH_LITERAL("foo.exe"));
  std::vector<base::FilePath::StringType> test_extensions;

  CompletionCallback completion_callback;
  CheckUnverifiedDownloadPolicy(GURL(), test_file_path, test_extensions,
                                completion_callback.GetCallback());
  EXPECT_EQ(UnverifiedDownloadPolicy::DISALLOWED,
            completion_callback.WaitForResult());

  // Not http/s and hence isn't covered by the whitelist.
  CheckUnverifiedDownloadPolicy(GURL("ftp://supported.example.com/foo/bar"),
                                test_file_path, test_extensions,
                                completion_callback.GetCallback());
  EXPECT_EQ(UnverifiedDownloadPolicy::DISALLOWED,
            completion_callback.WaitForResult());

  CheckUnverifiedDownloadPolicy(GURL("http://supported.example.com/foo/bar"),
                                test_file_path, test_extensions,
                                completion_callback.GetCallback());
  EXPECT_EQ(UnverifiedDownloadPolicy::ALLOWED,
            completion_callback.WaitForResult());

  CheckUnverifiedDownloadPolicy(GURL("http://unsupported.example.com/foo/bar"),
                                test_file_path, test_extensions,
                                completion_callback.GetCallback());
  EXPECT_EQ(UnverifiedDownloadPolicy::DISALLOWED,
            completion_callback.WaitForResult());

  CheckUnverifiedDownloadPolicy(GURL("https://supported.example.com/foo/bar"),
                                test_file_path, test_extensions,
                                completion_callback.GetCallback());
  EXPECT_EQ(UnverifiedDownloadPolicy::ALLOWED,
            completion_callback.WaitForResult());
}

TEST_F(UnverifiedDownloadPolicyTest, AlternateExtensions) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisallowUncheckedDangerousDownloads);
  CompletionCallback completion_callback;

  base::FilePath test_file_path(FILE_PATH_LITERAL("foo.txt"));
  std::vector<base::FilePath::StringType> test_extensions;

  CheckUnverifiedDownloadPolicy(GURL(), test_file_path, test_extensions,
                                completion_callback.GetCallback());
  EXPECT_EQ(UnverifiedDownloadPolicy::ALLOWED,
            completion_callback.WaitForResult());

  test_extensions.push_back(FILE_PATH_LITERAL(".exe"));
  CheckUnverifiedDownloadPolicy(GURL(), test_file_path, test_extensions,
                                completion_callback.GetCallback());
  EXPECT_EQ(UnverifiedDownloadPolicy::DISALLOWED,
            completion_callback.WaitForResult());

  test_extensions.clear();
  test_extensions.push_back(FILE_PATH_LITERAL(".txt"));
  test_extensions.push_back(FILE_PATH_LITERAL(".exe"));
  CheckUnverifiedDownloadPolicy(GURL(), test_file_path, test_extensions,
                                completion_callback.GetCallback());
  EXPECT_EQ(UnverifiedDownloadPolicy::DISALLOWED,
            completion_callback.WaitForResult());

  test_extensions.clear();
  test_extensions.push_back(FILE_PATH_LITERAL("e"));
  CheckUnverifiedDownloadPolicy(GURL(), test_file_path, test_extensions,
                                completion_callback.GetCallback());
  EXPECT_EQ(UnverifiedDownloadPolicy::ALLOWED,
            completion_callback.WaitForResult());
}

// Verify behavior when the SafeBrowsing service is disabled.
TEST_F(UnverifiedDownloadPolicyTest, ServiceDisabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisallowUncheckedDangerousDownloads);

  testing_profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  base::RunLoop().RunUntilIdle();

  base::FilePath test_file_path(FILE_PATH_LITERAL("foo.exe"));
  std::vector<base::FilePath::StringType> test_extensions;

  CompletionCallback completion_callback;
  CheckUnverifiedDownloadPolicy(GURL("http://supported.example.com/foo/bar"),
                                test_file_path, test_extensions,
                                completion_callback.GetCallback());
  EXPECT_EQ(UnverifiedDownloadPolicy::ALLOWED,
            completion_callback.WaitForResult());
}

}  // namespace safe_browsing
