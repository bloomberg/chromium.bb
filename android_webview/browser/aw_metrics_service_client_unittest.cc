// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_metrics_service_client.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_path_override.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {
namespace {

// For client ID format, see:
// https://en.wikipedia.org/wiki/Universally_unique_identifier#Version_4_(random)
const char kValidClientId[] = "01234567-89ab-40cd-80ef-0123456789ab";
const char kInvalidClientId[] = "foo";

class TestClient : public AwMetricsServiceClient {
 public:
  TestClient() {}
  ~TestClient() override {}

  void RunUntilInitialized() {
    if (init_finished_)
      return;
    run_loop_.Run();
    ASSERT_TRUE(init_finished_);
  }

  bool IsRecordingActive() {
    auto* service = GetMetricsService();
    if (service)
      return service->recording_active();
    return false;
  }

 protected:
  void InitializeWithClientId() override {
    AwMetricsServiceClient::InitializeWithClientId();
    init_finished_ = true;
    run_loop_.Quit();
  }

  bool IsInSample() override { return true; }

 private:
  base::RunLoop run_loop_;
  bool init_finished_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestClient);
};

std::unique_ptr<TestingPrefServiceSimple> CreateTestPrefs() {
  auto prefs = std::make_unique<TestingPrefServiceSimple>();
  metrics::MetricsService::RegisterPrefs(prefs->registry());
  return prefs;
}

std::unique_ptr<TestClient> CreateAndInitTestClient(PrefService* prefs) {
  auto client = std::make_unique<TestClient>();
  client->Initialize(prefs);
  client->RunUntilInitialized();
  return client;
}

}  // namespace

class AwMetricsServiceClientTest : public testing::Test {
 public:
  AwMetricsServiceClientTest() : task_runner_(new base::TestSimpleTaskRunner) {
    // Required by MetricsService.
    base::SetRecordActionTaskRunner(task_runner_);
  }

 protected:
  ~AwMetricsServiceClientTest() override {}

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(AwMetricsServiceClientTest);
};

TEST_F(AwMetricsServiceClientTest, TestSetConsentTrueBeforeInit) {
  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();
  client->SetHaveMetricsConsent(true);
  client->Initialize(prefs.get());
  client->RunUntilInitialized();
  ASSERT_TRUE(client->IsRecordingActive());
  ASSERT_TRUE(prefs->HasPrefPath(::metrics::prefs::kMetricsClientID));
}

TEST_F(AwMetricsServiceClientTest, TestSetConsentFalseBeforeInit) {
  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();
  client->SetHaveMetricsConsent(false);
  client->Initialize(prefs.get());
  client->RunUntilInitialized();
  ASSERT_FALSE(client->IsRecordingActive());
  ASSERT_FALSE(prefs->HasPrefPath(::metrics::prefs::kMetricsClientID));
}

TEST_F(AwMetricsServiceClientTest, TestSetConsentTrueDuringInit) {
  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();
  client->Initialize(prefs.get());
  client->SetHaveMetricsConsent(true);
  client->RunUntilInitialized();
  ASSERT_TRUE(client->IsRecordingActive());
  ASSERT_TRUE(prefs->HasPrefPath(::metrics::prefs::kMetricsClientID));
}

TEST_F(AwMetricsServiceClientTest, TestSetConsentFalseDuringInit) {
  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();
  client->Initialize(prefs.get());
  client->SetHaveMetricsConsent(false);
  client->RunUntilInitialized();
  ASSERT_FALSE(client->IsRecordingActive());
  ASSERT_FALSE(prefs->HasPrefPath(::metrics::prefs::kMetricsClientID));
}

TEST_F(AwMetricsServiceClientTest, TestSetConsentTrueAfterInit) {
  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true);
  ASSERT_TRUE(client->IsRecordingActive());
  ASSERT_TRUE(prefs->HasPrefPath(::metrics::prefs::kMetricsClientID));
}

TEST_F(AwMetricsServiceClientTest, TestSetConsentFalseAfterInit) {
  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(false);
  ASSERT_FALSE(client->IsRecordingActive());
  ASSERT_FALSE(prefs->HasPrefPath(::metrics::prefs::kMetricsClientID));
}

// If there is already a valid client ID, it should be reused.
TEST_F(AwMetricsServiceClientTest, TestKeepValidClientId) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(::metrics::prefs::kMetricsClientID, kValidClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true);
  ASSERT_TRUE(client->IsRecordingActive());
  ASSERT_TRUE(prefs->HasPrefPath(::metrics::prefs::kMetricsClientID));
  ASSERT_EQ(kValidClientId,
            prefs->GetString(::metrics::prefs::kMetricsClientID));
}

TEST_F(AwMetricsServiceClientTest, TestSetConsentFalseClearsClientId) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(::metrics::prefs::kMetricsClientID, kValidClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(false);
  ASSERT_FALSE(client->IsRecordingActive());
  ASSERT_FALSE(prefs->HasPrefPath(::metrics::prefs::kMetricsClientID));
}

// TODO(crbug/939002): Remove this after ~all clients have migrated the ID.
TEST_F(AwMetricsServiceClientTest, TestLoadAndDeleteLegacyClientId) {
  // Write a valid client ID to the legacy client ID file.
  base::ScopedPathOverride app_data_override(base::DIR_ANDROID_APP_DATA);
  base::FilePath legacy_file_path;
  ASSERT_TRUE(internal::GetLegacyClientIdPath(&legacy_file_path));
  constexpr int len = base::size(kValidClientId) - 1;
  ASSERT_EQ(len, base::WriteFile(legacy_file_path, kValidClientId, len));

  // Exercise AwMetricsServiceClient.
  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true);
  ASSERT_TRUE(client->IsRecordingActive());
  // The valid ID should have been stored in prefs.
  ASSERT_TRUE(prefs->HasPrefPath(::metrics::prefs::kMetricsClientID));
  ASSERT_EQ(kValidClientId,
            prefs->GetString(::metrics::prefs::kMetricsClientID));
  // The legacy file should have been deleted.
  ASSERT_FALSE(base::PathExists(legacy_file_path));
}

// TODO(crbug/939002): Remove this after ~all clients have migrated the ID.
TEST_F(AwMetricsServiceClientTest, TestDeleteInvalidLegacyClientId) {
  // Write an invalid client ID to the legacy client ID file.
  base::ScopedPathOverride app_data_override(base::DIR_ANDROID_APP_DATA);
  base::FilePath legacy_file_path;
  ASSERT_TRUE(internal::GetLegacyClientIdPath(&legacy_file_path));
  constexpr int len = base::size(kInvalidClientId) - 1;
  ASSERT_EQ(len, base::WriteFile(legacy_file_path, kInvalidClientId, len));

  // Exercise AwMetricsServiceClient.
  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true);
  ASSERT_TRUE(client->IsRecordingActive());
  // A new ID should have been generated and stored in prefs.
  ASSERT_TRUE(prefs->HasPrefPath(::metrics::prefs::kMetricsClientID));
  ASSERT_NE(kInvalidClientId,
            prefs->GetString(::metrics::prefs::kMetricsClientID));
  // The legacy file should have been deleted.
  ASSERT_FALSE(base::PathExists(legacy_file_path));
}

}  // namespace android_webview
