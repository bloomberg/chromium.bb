// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud/cloud_policy_client.h"
#include "chrome/browser/policy/cloud/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud/mock_cloud_policy_client.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_service.h"
#include "chrome/browser/policy/test/local_policy_test_server.h"
#include "chrome/browser/policy/test_utils.h"
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
#include "policy/policy_constants.h"
#include "policy/proto/chrome_settings.pb.h"
#include "policy/proto/cloud_policy.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/dbus/mock_cryptohome_client.h"
#include "chromeos/dbus/mock_dbus_thread_manager.h"
#include "chromeos/dbus/mock_session_manager_client.h"
#include "chromeos/dbus/mock_update_engine_client.h"
#else
#include "chrome/browser/policy/cloud/user_cloud_policy_manager.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#endif

using testing::AnyNumber;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::Return;
using testing::_;

namespace em = enterprise_management;

namespace policy {

namespace {

#if defined(OS_CHROMEOS)

const char kSanitizedUsername[] = "0123456789ABCDEF0123456789ABCDEF01234567";

ACTION(GetSanitizedUsername) {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(arg1, chromeos::DBUS_METHOD_CALL_SUCCESS, kSanitizedUsername));
}

ACTION_P(RetrieveUserPolicy, storage) {
  MessageLoop::current()->PostTask(FROM_HERE, base::Bind(arg0, *storage));
}

ACTION_P2(StoreUserPolicy, storage, user_policy_key_file) {
  // The session_manager stores a copy of the policy key at
  // /var/run/user_policy/$hash/policy.pub. Simulate that behavior here, so
  // that the policy signature can be validated.
  em::PolicyFetchResponse policy;
  ASSERT_TRUE(policy.ParseFromString(arg0));
  if (policy.has_new_public_key()) {
    ASSERT_TRUE(file_util::CreateDirectory(user_policy_key_file.DirName()));
    int result = file_util::WriteFile(
        user_policy_key_file,
        policy.new_public_key().data(),
        policy.new_public_key().size());
    ASSERT_EQ(static_cast<int>(policy.new_public_key().size()), result);
  }

  *storage = arg0;
  MessageLoop::current()->PostTask(FROM_HERE, base::Bind(arg1, true));
}

#endif

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
      "  \"policy_user\": \"%s\","
      "  \"current_key_index\": 0"
      "}";

  return base::StringPrintf(
      kEmptyPolicy, dm_protocol::kChromeUserPolicyType, GetTestUser());
}

std::string GetTestPolicy(int key_version) {
  const char kTestPolicy[] =
      "{"
      "  \"%s\": {"
      "    \"mandatory\": {"
      "      \"ShowHomeButton\": true,"
      "      \"RestoreOnStartup\": 4,"
      "      \"URLBlacklist\": [ \"dev.chromium.org\", \"youtube.com\" ]"
      "    },"
      "    \"recommended\": {"
      "      \"HomepageLocation\": \"google.com\""
      "    }"
      "  },"
      "  \"managed_users\": [ \"*\" ],"
      "  \"policy_user\": \"%s\","
      "  \"current_key_index\": %d"
      "}";

  return base::StringPrintf(kTestPolicy,
                            dm_protocol::kChromeUserPolicyType,
                            GetTestUser(),
                            key_version);
}

void GetExpectedTestPolicy(PolicyMap* expected) {
  expected->Set(key::kShowHomeButton, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                base::Value::CreateBooleanValue(true));
  expected->Set(key::kRestoreOnStartup, POLICY_LEVEL_MANDATORY,
                POLICY_SCOPE_USER, base::Value::CreateIntegerValue(4));
  base::ListValue list;
  list.AppendString("dev.chromium.org");
  list.AppendString("youtube.com");
  expected->Set(
      key::kURLBlacklist, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      list.DeepCopy());
  expected->Set(
      key::kHomepageLocation, POLICY_LEVEL_RECOMMENDED,
      POLICY_SCOPE_USER, base::Value::CreateStringValue("google.com"));
}

}  // namespace

// Tests the cloud policy stack(s).
class CloudPolicyTest : public InProcessBrowserTest {
 protected:
  CloudPolicyTest() {}
  virtual ~CloudPolicyTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_NO_FATAL_FAILURE(SetServerPolicy(GetEmptyPolicy()));

    test_server_.reset(new LocalPolicyTestServer(policy_file_path()));
    ASSERT_TRUE(test_server_->Start());

    std::string url = test_server_->GetServiceURL().spec();

    CommandLine* command_line = CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(switches::kDeviceManagementUrl, url);

#if defined(OS_CHROMEOS)
    PathService::Override(chrome::DIR_USER_POLICY_KEYS, user_policy_key_dir());

    mock_dbus_thread_manager_ = new chromeos::MockDBusThreadManager();
    chromeos::DBusThreadManager::InitializeForTesting(
        mock_dbus_thread_manager_);
    EXPECT_CALL(*mock_dbus_thread_manager_->mock_cryptohome_client(),
                GetSanitizedUsername(_, _))
        .WillRepeatedly(GetSanitizedUsername());
    EXPECT_CALL(*mock_dbus_thread_manager_->mock_session_manager_client(),
                StoreUserPolicy(_, _))
        .WillRepeatedly(StoreUserPolicy(&session_manager_user_policy_,
                                        user_policy_key_file()));
    EXPECT_CALL(*mock_dbus_thread_manager_->mock_session_manager_client(),
                RetrieveUserPolicy(_))
        .WillRepeatedly(RetrieveUserPolicy(&session_manager_user_policy_));
    EXPECT_CALL(*mock_dbus_thread_manager_->mock_update_engine_client(),
                GetLastStatus())
        .Times(1)
        .WillOnce(Return(chromeos::MockUpdateEngineClient::Status()));
#endif
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    ASSERT_TRUE(PolicyServiceIsEmpty(g_browser_process->policy_service()))
        << "Pre-existing policies in this machine will make this test fail.";

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
                            UserCloudPolicyManager::CreateCloudPolicyClient(
                                connector->device_management_service()).Pass());
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

#if defined(OS_CHROMEOS)
  base::FilePath user_policy_key_dir() {
    return temp_dir_.path().AppendASCII("user_policy");
  }

  base::FilePath user_policy_key_file() {
    return user_policy_key_dir().AppendASCII(kSanitizedUsername)
                                .AppendASCII("policy.pub");
  }
#endif

  void SetServerPolicy(const std::string& policy) {
    int result = file_util::WriteFile(policy_file_path(), policy.data(),
                                      policy.size());
    ASSERT_EQ(static_cast<int>(policy.size()), result);
  }

  base::FilePath policy_file_path() const {
    return temp_dir_.path().AppendASCII("policy.json");
  }

  base::ScopedTempDir temp_dir_;
  scoped_ptr<LocalPolicyTestServer> test_server_;

#if defined(OS_CHROMEOS)
  std::string session_manager_user_policy_;
  chromeos::MockDBusThreadManager* mock_dbus_thread_manager_;
#endif
};

IN_PROC_BROWSER_TEST_F(CloudPolicyTest, FetchPolicy) {
  PolicyService* policy_service = browser()->profile()->GetPolicyService();
  {
    base::RunLoop run_loop;
    // This does the initial fetch and stores the initial key.
    policy_service->RefreshPolicies(run_loop.QuitClosure());
    run_loop.Run();
  }

  PolicyMap empty;
  EXPECT_TRUE(empty.Equals(policy_service->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))));

  ASSERT_NO_FATAL_FAILURE(SetServerPolicy(GetTestPolicy(0)));
  PolicyMap expected;
  GetExpectedTestPolicy(&expected);
  {
    base::RunLoop run_loop;
    // This fetches the new policies, using the same key.
    policy_service->RefreshPolicies(run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_TRUE(expected.Equals(policy_service->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))));
}

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(CloudPolicyTest, FetchPolicyWithRotatedKey) {
  PolicyService* policy_service = browser()->profile()->GetPolicyService();
  {
    base::RunLoop run_loop;
    // This does the initial fetch and stores the initial key.
    policy_service->RefreshPolicies(run_loop.QuitClosure());
    run_loop.Run();
  }

  // Read the initial key.
  std::string initial_key;
  ASSERT_TRUE(
      file_util::ReadFileToString(user_policy_key_file(), &initial_key));

  PolicyMap empty;
  EXPECT_TRUE(empty.Equals(policy_service->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))));

  // Set the new policies and a new key at the server.
  ASSERT_NO_FATAL_FAILURE(SetServerPolicy(GetTestPolicy(1)));
  PolicyMap expected;
  GetExpectedTestPolicy(&expected);
  {
    base::RunLoop run_loop;
    // This fetches the new policies and does a key rotation.
    policy_service->RefreshPolicies(run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_TRUE(expected.Equals(policy_service->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))));

  // Verify that the key was rotated.
  std::string rotated_key;
  ASSERT_TRUE(
      file_util::ReadFileToString(user_policy_key_file(), &rotated_key));
  EXPECT_NE(rotated_key, initial_key);

  // Another refresh using the same key won't rotate it again.
  {
    base::RunLoop run_loop;
    policy_service->RefreshPolicies(run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_TRUE(expected.Equals(policy_service->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))));
  std::string current_key;
  ASSERT_TRUE(
      file_util::ReadFileToString(user_policy_key_file(), &current_key));
  EXPECT_EQ(rotated_key, current_key);
}
#endif

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
  chrome_settings.mutable_restoreonstartup()->set_restoreonstartup(4);
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
  cloud_policy.mutable_restoreonstartup()->set_value(4);
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
