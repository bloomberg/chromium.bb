// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud_policy_client.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_service.h"
#include "chrome/browser/policy/proto/chrome_settings.pb.h"
#include "chrome/browser/policy/proto/cloud_policy.pb.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_utils.h"
#include "googleurl/src/gurl.h"
#include "net/test/test_server.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/policy/user_cloud_policy_manager_chromeos.h"
#else
#include "chrome/browser/policy/user_cloud_policy_manager.h"
#include "chrome/browser/policy/user_cloud_policy_manager_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#endif

using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::_;

namespace em = enterprise_management;

namespace policy {

namespace {

class MockCloudPolicyClientObserver : public CloudPolicyClient::Observer {
 public:
  MockCloudPolicyClientObserver() {}
  virtual ~MockCloudPolicyClientObserver() {}

  MOCK_METHOD1(OnPolicyFetched, void(CloudPolicyClient*));
  MOCK_METHOD1(OnRegistrationStateChanged, void(CloudPolicyClient*));
  MOCK_METHOD1(OnClientError, void(CloudPolicyClient*));
};

const char* GetTestUser() {
#if defined(OS_CHROMEOS)
  return chromeos::UserManager::kStubUser;
#else
  return "user@example.com";
#endif
}

std::string GetEmptyPolicy() {
  const char kEmptyPolicy[] =
      "{"
      "  \"%s\": {"
      "    \"mandatory\": {},"
      "    \"recommended\": {}"
      "  },"
      "  \"managed_users\": [ \"*\" ],"
      "  \"policy_user\": \"%s\""
      "}";

  return base::StringPrintf(kEmptyPolicy, dm_protocol::kChromeUserPolicyType,
                            GetTestUser());
}

std::string GetTestPolicy() {
  const char kTestPolicy[] =
      "{"
      "  \"%s\": {"
      "    \"mandatory\": {"
      "      \"ShowHomeButton\": true,"
      "      \"MaxConnectionsPerProxy\": 42,"
      "      \"URLBlacklist\": [ \"dev.chromium.org\", \"youtube.com\" ]"
      "    },"
      "    \"recommended\": {"
      "      \"HomepageLocation\": \"google.com\""
      "    }"
      "  },"
      "  \"managed_users\": [ \"*\" ],"
      "  \"policy_user\": \"%s\""
      "}";

  return base::StringPrintf(kTestPolicy, dm_protocol::kChromeUserPolicyType,
                            GetTestUser());
}

}  // namespace

// Tests the cloud policy stack(s).
class CloudPolicyTest : public InProcessBrowserTest {
 protected:
  CloudPolicyTest() {}
  virtual ~CloudPolicyTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    // The TestServer wants the docroot as a path relative to the source dir.
    FilePath source;
    ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &source));
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDirUnderPath(source));
    ASSERT_NO_FATAL_FAILURE(SetServerPolicy(GetEmptyPolicy()));

    test_server_.reset(
        new net::TestServer(
            net::TestServer::TYPE_HTTP,
            net::TestServer::kLocalhost,
            temp_dir_.path().BaseName()));
    ASSERT_TRUE(test_server_->Start());

    std::string url = test_server_->GetURL("device_management").spec();

    CommandLine* command_line = CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(switches::kDeviceManagementUrl, url);
    command_line->AppendSwitch(switches::kLoadCloudPolicyOnSignin);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    // Checks that no policies have been loaded by the other providers before
    // setting up the cloud connection. Other policies configured in the test
    // machine will interfere with these tests.
    const PolicyMap& map = g_browser_process->policy_service()->GetPolicies(
        POLICY_DOMAIN_CHROME, "");
    if (!map.empty()) {
      base::DictionaryValue dict;
      for (PolicyMap::const_iterator it = map.begin(); it != map.end(); ++it)
        dict.SetWithoutPathExpansion(it->first, it->second.value->DeepCopy());
      ADD_FAILURE()
          << "There are pre-existing policies in this machine that will "
          << "interfere with these tests. Policies found: " << dict;
    }

    BrowserPolicyConnector* connector =
        g_browser_process->browser_policy_connector();
    connector->ScheduleServiceInitialization(0);

#if defined(OS_CHROMEOS)
    UserCloudPolicyManagerChromeOS* policy_manager =
        connector->GetUserCloudPolicyManager();
    ASSERT_TRUE(policy_manager);
#else
    // Mock a signed-in user. This is used by the UserCloudPolicyStore to pass
    // the username to the UserCloudPolicyValidator.
    SigninManager* signin_manager =
        SigninManagerFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(signin_manager);
    signin_manager->SetAuthenticatedUsername(GetTestUser());

    UserCloudPolicyManager* policy_manager =
        UserCloudPolicyManagerFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(policy_manager);
    policy_manager->Connect(g_browser_process->local_state(),
                            connector->device_management_service());
#endif  // defined(OS_CHROMEOS)

    ASSERT_TRUE(policy_manager->core()->client());
    base::RunLoop run_loop;
    MockCloudPolicyClientObserver observer;
    EXPECT_CALL(observer, OnRegistrationStateChanged(_)).WillOnce(
        InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    policy_manager->core()->client()->AddObserver(&observer);

    // Give a bogus OAuth token to the |policy_manager|. This should make its
    // CloudPolicyClient fetch the DMToken.
    policy_manager->RegisterClient("bogus");
    run_loop.Run();
    Mock::VerifyAndClearExpectations(&observer);
    policy_manager->core()->client()->RemoveObserver(&observer);
  }

  void SetServerPolicy(const std::string& policy) {
    int result = file_util::WriteFile(
        temp_dir_.path().AppendASCII("device_management"),
        policy.data(), policy.size());
    ASSERT_EQ(static_cast<int>(policy.size()), result);
  }

  base::ScopedTempDir temp_dir_;
  scoped_ptr<net::TestServer> test_server_;
};

IN_PROC_BROWSER_TEST_F(CloudPolicyTest, FetchPolicy) {
  PolicyService* policy_service = browser()->profile()->GetPolicyService();
  {
    base::RunLoop run_loop;
    policy_service->RefreshPolicies(run_loop.QuitClosure());
    run_loop.Run();
  }

  PolicyMap empty;
  EXPECT_TRUE(
      empty.Equals(policy_service->GetPolicies(POLICY_DOMAIN_CHROME, "")));

  ASSERT_NO_FATAL_FAILURE(SetServerPolicy(GetTestPolicy()));
  PolicyMap expected;
  expected.Set(key::kShowHomeButton, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               base::Value::CreateBooleanValue(true));
  expected.Set(key::kMaxConnectionsPerProxy, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateIntegerValue(42));
  base::ListValue list;
  list.AppendString("dev.chromium.org");
  list.AppendString("youtube.com");
  expected.Set(
      key::kURLBlacklist, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      list.DeepCopy());
  expected.Set(
      key::kHomepageLocation, POLICY_LEVEL_RECOMMENDED,
      POLICY_SCOPE_USER, base::Value::CreateStringValue("google.com"));
  {
    base::RunLoop run_loop;
    policy_service->RefreshPolicies(run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_TRUE(
      expected.Equals(policy_service->GetPolicies(POLICY_DOMAIN_CHROME, "")));
}

TEST(CloudPolicyProtoTest, VerifyProtobufEquivalence) {
  // There are 2 protobufs that can be used for user cloud policy:
  // cloud_policy.proto and chrome_settings.proto. chrome_settings.proto is the
  // version used by the server, but generates one proto message per policy; to
  // save binary size on the client, the other version shares proto messages for
  // policies of the same type. They generate the same bytes on the wire though,
  // so they are compatible. This test verifies that that stays true.

  // Build a ChromeSettingsProto message with one policy of each supported type.
  em::ChromeSettingsProto chrome_settings;
  chrome_settings.mutable_homepagelocation()->set_homepagelocation(
      "chromium.org");
  chrome_settings.mutable_showhomebutton()->set_showhomebutton(true);
  chrome_settings.mutable_policyrefreshrate()->set_policyrefreshrate(100);
  em::StringList* list =
      chrome_settings.mutable_disabledschemes()->mutable_disabledschemes();
  list->add_entries("ftp");
  list->add_entries("mailto");
  // Try explicitly setting a policy mode too.
  chrome_settings.mutable_disablespdy()->set_disablespdy(false);
  chrome_settings.mutable_disablespdy()->mutable_policy_options()->set_mode(
      em::PolicyOptions::MANDATORY);
  chrome_settings.mutable_syncdisabled()->set_syncdisabled(true);
  chrome_settings.mutable_syncdisabled()->mutable_policy_options()->set_mode(
      em::PolicyOptions::RECOMMENDED);

  // Build an equivalent CloudPolicySettings message.
  em::CloudPolicySettings cloud_policy;
  cloud_policy.mutable_homepagelocation()->set_value("chromium.org");
  cloud_policy.mutable_showhomebutton()->set_value(true);
  cloud_policy.mutable_policyrefreshrate()->set_value(100);
  list = cloud_policy.mutable_disabledschemes()->mutable_value();
  list->add_entries("ftp");
  list->add_entries("mailto");
  cloud_policy.mutable_disablespdy()->set_value(false);
  cloud_policy.mutable_disablespdy()->mutable_policy_options()->set_mode(
      em::PolicyOptions::MANDATORY);
  cloud_policy.mutable_syncdisabled()->set_value(true);
  cloud_policy.mutable_syncdisabled()->mutable_policy_options()->set_mode(
      em::PolicyOptions::RECOMMENDED);

  // They should now serialize to the same bytes.
  std::string chrome_settings_serialized;
  std::string cloud_policy_serialized;
  ASSERT_TRUE(chrome_settings.SerializeToString(&chrome_settings_serialized));
  ASSERT_TRUE(cloud_policy.SerializeToString(&cloud_policy_serialized));
  EXPECT_EQ(chrome_settings_serialized, cloud_policy_serialized);
}

}  // namespace policy
