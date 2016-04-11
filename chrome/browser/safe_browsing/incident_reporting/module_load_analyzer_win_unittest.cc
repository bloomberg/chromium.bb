// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/module_load_analyzer.h"

#include <string>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_vector.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_util.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_receiver.h"
#include "chrome/browser/safe_browsing/incident_reporting/mock_incident_receiver.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "components/safe_browsing_db/database_manager.h"
#include "components/safe_browsing_db/test_database_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;

namespace safe_browsing {

namespace {

const char kWhitelistedModuleName[] = "USER32.dll";

const char kNonWhitelistedModuleName[] = "blacklist_test_dll_1.dll";

class MockSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager() {}

  MOCK_METHOD1(MatchModuleWhitelistString, bool(const std::string&));

 private:
  ~MockSafeBrowsingDatabaseManager() override {}

  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingDatabaseManager);
};

class ModuleLoadAnalayzerTest : public testing::Test {
 protected:
  ModuleLoadAnalayzerTest()
      : mock_incident_receiver_(
            new StrictMock<safe_browsing::MockIncidentReceiver>()),
        mock_safe_browsing_database_manager_(
            new MockSafeBrowsingDatabaseManager()) {
    // Accept all dlls except kNonWhitelistedModuleName.
    EXPECT_CALL(*mock_safe_browsing_database_manager_,
                MatchModuleWhitelistString(_))
        .WillRepeatedly(Return(true));
  }

  void ExpectIncident(const std::string& module_to_load) {
    base::FilePath current_dir;
    ASSERT_TRUE(PathService::Get(base::DIR_EXE, &current_dir));
    base::ScopedNativeLibrary dll1(current_dir.AppendASCII(module_to_load));

    std::unique_ptr<Incident> incident;
    EXPECT_CALL(*mock_incident_receiver_, DoAddIncidentForProcess(_))
        .WillOnce(TakeIncident(&incident));

    VerifyModuleLoadState(mock_safe_browsing_database_manager_,
                          base::WrapUnique(mock_incident_receiver_));

    base::RunLoop().RunUntilIdle();
    content::RunAllBlockingPoolTasksUntilIdle();

    ASSERT_TRUE(incident);
    std::unique_ptr<ClientIncidentReport_IncidentData> incident_data =
        incident->TakePayload();
    ASSERT_TRUE(incident_data->has_suspicious_module());
    const ClientIncidentReport_IncidentData_SuspiciousModuleIncident&
        suspicious_module_incident = incident_data->suspicious_module();
    EXPECT_TRUE(suspicious_module_incident.has_digest());
    EXPECT_TRUE(base::EndsWith(suspicious_module_incident.path(),
                               module_to_load, base::CompareCase::SENSITIVE));
  }

  void ExpectNoIncident(const std::string& module_to_load) {
    base::FilePath current_dir;
    ASSERT_TRUE(PathService::Get(base::DIR_EXE, &current_dir));
    base::ScopedNativeLibrary dll1(current_dir.AppendASCII(module_to_load));

    EXPECT_CALL(*mock_incident_receiver_, DoAddIncidentForProcess(_)).Times(0);

    VerifyModuleLoadState(mock_safe_browsing_database_manager_,
                          base::WrapUnique(mock_incident_receiver_));

    base::RunLoop().RunUntilIdle();
    content::RunAllBlockingPoolTasksUntilIdle();
  }

  content::TestBrowserThreadBundle browser_thread_bundle_;
  StrictMock<safe_browsing::MockIncidentReceiver>* mock_incident_receiver_;
  scoped_refptr<MockSafeBrowsingDatabaseManager>
      mock_safe_browsing_database_manager_;
};

}  // namespace

TEST_F(ModuleLoadAnalayzerTest, TestWhitelistedDLLs) {
  ExpectNoIncident(kWhitelistedModuleName);
}

TEST_F(ModuleLoadAnalayzerTest, TestNonWhitelistedDLLs) {
  EXPECT_CALL(*mock_safe_browsing_database_manager_,
              MatchModuleWhitelistString(kNonWhitelistedModuleName))
      .WillOnce(Return(false));

  ExpectIncident(kNonWhitelistedModuleName);
}

}  // namespace safe_browsing
