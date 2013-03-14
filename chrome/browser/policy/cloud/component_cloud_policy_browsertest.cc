// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud/mock_cloud_policy_client.h"
#include "chrome/browser/policy/cloud/proto/chrome_extension_policy.pb.h"
#include "chrome/browser/policy/policy_service.h"
#include "chrome/browser/policy/test/local_policy_test_server.h"
#include "chrome/browser/policy/test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
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
#else
#include "chrome/browser/policy/cloud/user_cloud_policy_manager.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#endif

using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::_;

namespace em = enterprise_management;

namespace policy {

namespace {

const char kDMToken[] = "dmtoken";
const char kDeviceID[] = "deviceid";

const char kTestExtension[] = "kjmkgkdkpedkejedfhmfcenooemhbpbo";
const char kTestExtension2[] = "behllobkkfkfnphdnhnkndlbkcpglgmj";

const base::FilePath::CharType kTestExtensionPath[] =
    FILE_PATH_LITERAL("extensions/managed_extension");
const base::FilePath::CharType kTestExtension2Path[] =
    FILE_PATH_LITERAL("extensions/managed_extension2");

const char kTestPolicy[] =
    "{"
    "  \"Name\": {"
    "    \"Value\": \"disable_all_the_things\""
    "  }"
    "}";

const char kTestPolicyJSON[] = "{\"Name\":\"disable_all_the_things\"}";

const char kTestPolicy2[] =
    "{"
    "  \"Another\": {"
    "    \"Value\": \"turn_it_off\""
    "  }"
    "}";

const char kTestPolicy2JSON[] = "{\"Another\":\"turn_it_off\"}";

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

#else

const char kTestUser[] = "user@example.com";

#endif  // OS_CHROMEOS

}  // namespace

class ComponentCloudPolicyTest : public ExtensionBrowserTest {
 protected:
  ComponentCloudPolicyTest() {}
  virtual ~ComponentCloudPolicyTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
#if defined(OS_CHROMEOS)
    // ExtensionBrowserTest sets the login users to a non-managed value;
    // replace it. This is the default username sent in policy blobs from the
    // testserver.
    command_line->AppendSwitchASCII(switches::kLoginUser, "user@example.com");
#endif
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    test_server_.RegisterClient(kDMToken, kDeviceID);
    EXPECT_TRUE(test_server_.UpdatePolicyData(
        dm_protocol::kChromeExtensionPolicyType, kTestExtension, kTestPolicy));
    ASSERT_TRUE(test_server_.Start());

    std::string url = test_server_.GetServiceURL().spec();
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(switches::kDeviceManagementUrl, url);
    command_line->AppendSwitch(switches::kEnableComponentCloudPolicy);

#if defined(OS_CHROMEOS)
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath user_key_path =
        temp_dir_.path().AppendASCII(kSanitizedUsername)
                        .AppendASCII("policy.pub");
    PathService::Override(chrome::DIR_USER_POLICY_KEYS, temp_dir_.path());

    mock_dbus_thread_manager_ = new chromeos::MockDBusThreadManager();
    chromeos::DBusThreadManager::InitializeForTesting(
        mock_dbus_thread_manager_);
    EXPECT_CALL(*mock_dbus_thread_manager_->mock_cryptohome_client(),
                GetSanitizedUsername(_, _))
        .WillRepeatedly(GetSanitizedUsername());
    EXPECT_CALL(*mock_dbus_thread_manager_->mock_session_manager_client(),
                StoreUserPolicy(_, _))
        .WillRepeatedly(StoreUserPolicy(&session_manager_user_policy_,
                                        user_key_path));
    EXPECT_CALL(*mock_dbus_thread_manager_->mock_session_manager_client(),
                RetrieveUserPolicy(_))
        .WillRepeatedly(RetrieveUserPolicy(&session_manager_user_policy_));
#endif  // OS_CHROMEOS

    ExtensionBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    ASSERT_TRUE(PolicyServiceIsEmpty(g_browser_process->policy_service()))
        << "Pre-existing policies in this machine will make this test fail.";

    // Install the initial extension.
    ExtensionTestMessageListener ready_listener("ready", true);
    event_listener_.reset(new ExtensionTestMessageListener("event", true));
    extension_ = LoadExtension(kTestExtensionPath);
    ASSERT_TRUE(extension_);
    ASSERT_EQ(kTestExtension, extension_->id());
    EXPECT_TRUE(ready_listener.WaitUntilSatisfied());

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
    signin_manager->SetAuthenticatedUsername(kTestUser);

    UserCloudPolicyManager* policy_manager =
        UserCloudPolicyManagerFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(policy_manager);
    policy_manager->Connect(g_browser_process->local_state(),
                            UserCloudPolicyManager::CreateCloudPolicyClient(
                                connector->device_management_service()).Pass());
#endif  // defined(OS_CHROMEOS)

    // Register the cloud policy client.
    ASSERT_TRUE(policy_manager->core()->client());
    base::RunLoop run_loop;
    MockCloudPolicyClientObserver observer;
    EXPECT_CALL(observer, OnRegistrationStateChanged(_))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    policy_manager->core()->client()->AddObserver(&observer);
    policy_manager->core()->client()->SetupRegistration(kDMToken, kDeviceID);
    run_loop.Run();
    Mock::VerifyAndClearExpectations(&observer);
    policy_manager->core()->client()->RemoveObserver(&observer);

    // The extension will receive an update event.
    EXPECT_TRUE(event_listener_->WaitUntilSatisfied());

    ExtensionBrowserTest::SetUpOnMainThread();
  }

  scoped_refptr<const extensions::Extension> LoadExtension(
      const base::FilePath::CharType* path) {
    base::FilePath full_path;
    if (!PathService::Get(chrome::DIR_TEST_DATA, &full_path)) {
      ADD_FAILURE();
      return NULL;
    }
    scoped_refptr<const extensions::Extension> extension(
        ExtensionBrowserTest::LoadExtension(full_path.Append(path)));
    if (!extension) {
      ADD_FAILURE();
      return NULL;
    }
    return extension;
  }

  void RefreshPolicies() {
    PolicyService* policy_service = browser()->profile()->GetPolicyService();
    base::RunLoop run_loop;
    policy_service->RefreshPolicies(run_loop.QuitClosure());
    run_loop.Run();
  }

  LocalPolicyTestServer test_server_;
  scoped_refptr<const extensions::Extension> extension_;
  scoped_ptr<ExtensionTestMessageListener> event_listener_;

#if defined(OS_CHROMEOS)
  base::ScopedTempDir temp_dir_;
  std::string session_manager_user_policy_;
  chromeos::MockDBusThreadManager* mock_dbus_thread_manager_;
#endif
};

// TODO(joaodasilva): enable these for other platforms once ready.
#if defined(OS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(ComponentCloudPolicyTest, FetchExtensionPolicy) {
  // Read the initial policy.
  ExtensionTestMessageListener policy_listener(kTestPolicyJSON, true);
  event_listener_->Reply("get-policy-Name");
  EXPECT_TRUE(policy_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ComponentCloudPolicyTest, UpdateExtensionPolicy) {
  // Read the initial policy.
  ExtensionTestMessageListener policy_listener(kTestPolicyJSON, true);
  event_listener_->Reply("get-policy-Name");
  EXPECT_TRUE(policy_listener.WaitUntilSatisfied());

  // Update the policy at the server and reload policy.
  event_listener_.reset(new ExtensionTestMessageListener("event", true));
  policy_listener.Reply("idle");
  EXPECT_TRUE(test_server_.UpdatePolicyData(
      dm_protocol::kChromeExtensionPolicyType, kTestExtension, kTestPolicy2));
  RefreshPolicies();

  // Check that the update event was received, and verify the new policy
  // values.
  EXPECT_TRUE(event_listener_->WaitUntilSatisfied());

  // This policy was removed.
  ExtensionTestMessageListener policy_listener1("{}", true);
  event_listener_->Reply("get-policy-Name");
  EXPECT_TRUE(policy_listener1.WaitUntilSatisfied());

  ExtensionTestMessageListener policy_listener2(kTestPolicy2JSON, true);
  policy_listener1.Reply("get-policy-Another");
  EXPECT_TRUE(policy_listener2.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ComponentCloudPolicyTest, InstallNewExtension) {
  EXPECT_TRUE(test_server_.UpdatePolicyData(
      dm_protocol::kChromeExtensionPolicyType, kTestExtension2, kTestPolicy2));

  ExtensionTestMessageListener result_listener("ok", true);
  result_listener.AlsoListenForFailureMessage("fail");
  scoped_refptr<const extensions::Extension> extension2 =
      LoadExtension(kTestExtension2Path);
  ASSERT_TRUE(extension2);
  ASSERT_EQ(kTestExtension2, extension2->id());

  // This extension only sends the 'policy' signal once it receives the policy,
  // and after verifying it has the expected value. Otherwise it sends 'fail'.
  EXPECT_TRUE(result_listener.WaitUntilSatisfied());
}

#endif  // OS_CHROMEOS

}  // namespace policy
