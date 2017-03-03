// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/files/dir_reader_posix.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/null_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/extensions/signin_screen_policy_provider.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/policy/test/local_policy_test_server.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.cc"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/core/common/cloud/resource_cache.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/proto/chrome_extension_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/rsa_private_key.h"
#include "crypto/sha2.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_notification_observer.h"
#include "extensions/test/result_catcher.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

class DeviceCloudPolicyBrowserTest : public InProcessBrowserTest {
 public:
  DeviceCloudPolicyBrowserTest() : mock_client_(new MockCloudPolicyClient) {
  }

  MockCloudPolicyClient* mock_client_;
};

IN_PROC_BROWSER_TEST_F(DeviceCloudPolicyBrowserTest, Initializer) {
  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  // Initializer exists at first.
  EXPECT_TRUE(connector->GetDeviceCloudPolicyInitializer());

  // Initializer is deleted when the manager connects.
  connector->GetDeviceCloudPolicyManager()->StartConnection(
      base::WrapUnique(mock_client_), connector->GetInstallAttributes());
  EXPECT_FALSE(connector->GetDeviceCloudPolicyInitializer());

  // Initializer is restarted when the manager disconnects.
  connector->GetDeviceCloudPolicyManager()->Disconnect();
  EXPECT_TRUE(connector->GetDeviceCloudPolicyInitializer());
}

// This class is the base class for the tests of the behavior regarding
// extensions installed on the signin screen (which is generally possible only
// through an admin policy, but the tests may install the extensions directly
// for the test purposes).
class SigninExtensionsDeviceCloudPolicyBrowserTestBase
    : public DevicePolicyCrosBrowserTest {
 protected:
  static constexpr const char* kTestExtensionId =
      "baogpbmpccplckhhehfipokjaflkmbno";
  static constexpr const char* kTestExtensionSourceDir =
      "extensions/signin_screen_managed_storage";
  static constexpr const char* kTestExtensionVersion = "1.0";
  static constexpr const char* kTestExtensionTestPage = "test.html";
  static constexpr const char* kFakePolicyUrl =
      "http://example.org/test-policy.json";
  static constexpr const char* kFakePolicy =
      "{\"string-policy\": {\"Value\": \"value\"}}";
  static constexpr int kFakePolicyPublicKeyVersion = 1;
  static constexpr const char* kPolicyProtoCacheKey = "signinextension-policy";
  static constexpr const char* kPolicyDataCacheKey =
      "signinextension-policy-data";

  SigninExtensionsDeviceCloudPolicyBrowserTestBase() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
    // This flag is required for the successful installation of the test
    // extension into the signin profile, due to some checks in
    // |ExtensionService|.
    command_line->AppendSwitchASCII(::switches::kDisableExtensionsExcept,
                                    GetTestExtensionSourcePath().value());
  }

  void SetUpInProcessBrowserTestFixture() override {
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();
    InstallOwnerKey();
    MarkAsEnterpriseOwned();
    SetFakeDevicePolicy();
  }

  void SetUpOnMainThread() override {
    DevicePolicyCrosBrowserTest::SetUpOnMainThread();

    BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    connector->device_management_service()->ScheduleInitialization(0);

    ExtensionService* service =
        extensions::ExtensionSystem::Get(GetSigninProfile())
            ->extension_service();
    service->set_extensions_enabled(true);
  }

  static base::FilePath GetTestExtensionSourcePath() {
    base::FilePath test_data_dir;
    EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    return test_data_dir.AppendASCII(kTestExtensionSourceDir);
  }

  static Profile* GetSigninProfile() {
    Profile* signin_profile =
        chromeos::ProfileHelper::GetSigninProfile()->GetOriginalProfile();
    EXPECT_TRUE(signin_profile);
    return signin_profile;
  }

  static const extensions::Extension* GetTestExtension() {
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(GetSigninProfile());
    const extensions::Extension* extension =
        registry->enabled_extensions().GetByID(kTestExtensionId);
    EXPECT_TRUE(extension);
    return extension;
  }

  static enterprise_management::PolicyFetchResponse BuildTestComponentPolicy() {
    ComponentPolicyBuilder builder;
    MakeTestComponentPolicyBuilder(&builder);
    return builder.policy();
  }

  static enterprise_management::ExternalPolicyData
  BuildTestComponentPolicyPayload() {
    ComponentPolicyBuilder builder;
    MakeTestComponentPolicyBuilder(&builder);
    return builder.payload();
  }

 private:
  void SetFakeDevicePolicy() {
    device_policy()->policy_data().set_username(PolicyBuilder::kFakeUsername);
    device_policy()->policy_data().set_public_key_version(
        kFakePolicyPublicKeyVersion);
    device_policy()->Build();
    session_manager_client()->set_device_policy(device_policy()->GetBlob());
  }

  static void MakeTestComponentPolicyBuilder(ComponentPolicyBuilder* builder) {
    builder->policy_data().set_policy_type(
        dm_protocol::kChromeSigninExtensionPolicyType);
    builder->policy_data().set_username(PolicyBuilder::kFakeUsername);
    builder->policy_data().set_settings_entity_id(kTestExtensionId);
    builder->policy_data().set_public_key_version(kFakePolicyPublicKeyVersion);
    builder->payload().set_download_url(kFakePolicyUrl);
    builder->payload().set_secure_hash(crypto::SHA256HashString(kFakePolicy));
    builder->Build();
  }

  DISALLOW_COPY_AND_ASSIGN(SigninExtensionsDeviceCloudPolicyBrowserTestBase);
};

// This class tests whether the component policy is successfully processed and
// passed to the extension that is installed into the signin profile after the
// initialization finishes.
//
// The fake component policy is injected by using the local policy test server.
// The test extension is installed into the profile manually by the test code
// (i.e. this class doesn't test the installation of the signin screen
// extensions through the admin policy).
class SigninExtensionsDeviceCloudPolicyBrowserTest
    : public SigninExtensionsDeviceCloudPolicyBrowserTestBase {
 protected:
  SigninExtensionsDeviceCloudPolicyBrowserTest()
      : fetcher_factory_(&fetcher_impl_factory_) {}

  const extensions::Extension* InstallAndLoadTestExtension() const {
    Profile* signin_profile = GetSigninProfile();
    ExtensionService* service =
        extensions::ExtensionSystem::Get(signin_profile)->extension_service();
    scoped_refptr<extensions::UnpackedInstaller> installer(
        extensions::UnpackedInstaller::Create(service));
    extensions::ExtensionTestNotificationObserver observer(signin_profile);
    installer->Load(GetTestExtensionSourcePath());
    observer.WaitForExtensionLoad();
    return GetTestExtension();
  }

 private:
  void SetUp() override {
    StartPolicyTestServer();
    DevicePolicyCrosBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SigninExtensionsDeviceCloudPolicyBrowserTestBase::SetUpCommandLine(
        command_line);
    command_line->AppendSwitchASCII(policy::switches::kDeviceManagementUrl,
                                    policy_test_server_.GetServiceURL().spec());
  }

  void SetUpInProcessBrowserTestFixture() override {
    SigninExtensionsDeviceCloudPolicyBrowserTestBase::
        SetUpInProcessBrowserTestFixture();
    signin_policy_provided_disabler_ =
        chromeos::GetScopedSigninScreenPolicyProviderDisablerForTesting();
    EXPECT_TRUE(PathService::Get(chromeos::DIR_SIGNIN_PROFILE_COMPONENT_POLICY,
                                 &component_policy_cache_dir_));
    PrepareFakeComponentPolicyResponse();
  }

  void TearDownInProcessBrowserTestFixture() override {
    // Check that the component policy cache was not cleared during browser
    // teardown.
    ResourceCache cache(component_policy_cache_dir_, new base::NullTaskRunner);
    std::string stub;
    EXPECT_TRUE(cache.Load(kPolicyProtoCacheKey, kTestExtensionId, &stub));
    EXPECT_TRUE(cache.Load(kPolicyDataCacheKey, kTestExtensionId, &stub));

    SigninExtensionsDeviceCloudPolicyBrowserTestBase::
        TearDownInProcessBrowserTestFixture();
  }

  void StartPolicyTestServer() {
    std::unique_ptr<crypto::RSAPrivateKey> signing_key(
        PolicyBuilder::CreateTestSigningKey());
    EXPECT_TRUE(policy_test_server_.SetSigningKeyAndSignature(
        signing_key.get(), PolicyBuilder::GetTestSigningKeySignature()));
    policy_test_server_.RegisterClient(PolicyBuilder::kFakeToken,
                                       PolicyBuilder::kFakeDeviceId);
    EXPECT_TRUE(policy_test_server_.Start());
  }

  void PrepareFakeComponentPolicyResponse() {
    EXPECT_TRUE(policy_test_server_.UpdatePolicy(
        dm_protocol::kChromeSigninExtensionPolicyType, kTestExtensionId,
        BuildTestComponentPolicyPayload().SerializeAsString()));
    fetcher_factory_.SetFakeResponse(GURL(kFakePolicyUrl), kFakePolicy,
                                     net::HTTP_OK,
                                     net::URLRequestStatus::SUCCESS);
  }

  LocalPolicyTestServer policy_test_server_;
  net::URLFetcherImplFactory fetcher_impl_factory_;
  net::FakeURLFetcherFactory fetcher_factory_;
  base::FilePath component_policy_cache_dir_;
  std::unique_ptr<base::AutoReset<bool>> signin_policy_provided_disabler_;
};

IN_PROC_BROWSER_TEST_F(SigninExtensionsDeviceCloudPolicyBrowserTest,
                       InstallAndRunInWindow) {
  const extensions::Extension* extension = InstallAndLoadTestExtension();
  ASSERT_TRUE(extension);
  Browser* browser = CreateBrowser(GetSigninProfile());
  extensions::ResultCatcher result_catcher;
  ui_test_utils::NavigateToURL(
      browser, extension->GetResourceURL(kTestExtensionTestPage));
  EXPECT_TRUE(result_catcher.GetNextResult());
  CloseBrowserSynchronously(browser);
}

// This class tests that the cached component policy is successfully loaded and
// passed to the extension that is already installed into the signin profile.
//
// To accomplish this, the class prefills the signin profile with, first, the
// installed test extension, and, second, with the cached component policy.
class PreinstalledSigninExtensionsDeviceCloudPolicyBrowserTest
    : public SigninExtensionsDeviceCloudPolicyBrowserTestBase {
 protected:
  static constexpr const char* kFakeProfileSourceDir =
      "extensions/profiles/signin_screen_managed_storage";

  bool SetUpUserDataDirectory() override {
    PrefillSigninProfile();
    PrefillComponentPolicyCache();
    return DevicePolicyCrosBrowserTest::SetUpUserDataDirectory();
  }

  void SetUpInProcessBrowserTestFixture() override {
    SigninExtensionsDeviceCloudPolicyBrowserTestBase::
        SetUpInProcessBrowserTestFixture();
    signin_policy_provided_disabler_ =
        chromeos::GetScopedSigninScreenPolicyProviderDisablerForTesting();
  }

 private:
  static void PrefillSigninProfile() {
    base::FilePath profile_source_path;
    EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &profile_source_path));
    profile_source_path = profile_source_path.AppendASCII(kFakeProfileSourceDir)
                              .AppendASCII(chrome::kInitialProfile);

    base::FilePath profile_target_path;
    EXPECT_TRUE(PathService::Get(chrome::DIR_USER_DATA, &profile_target_path));

    EXPECT_TRUE(
        base::CopyDirectory(profile_source_path, profile_target_path, true));

    base::FilePath extension_target_path =
        profile_target_path.AppendASCII(chrome::kInitialProfile)
            .AppendASCII(extensions::kInstallDirectoryName)
            .AppendASCII(kTestExtensionId)
            .AppendASCII(kTestExtensionVersion);
    EXPECT_TRUE(base::CopyDirectory(GetTestExtensionSourcePath(),
                                    extension_target_path, true));
  }

  void PrefillComponentPolicyCache() {
    base::FilePath user_data_dir;
    EXPECT_TRUE(PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
    chromeos::RegisterStubPathOverrides(user_data_dir);

    base::FilePath cache_dir;
    EXPECT_TRUE(PathService::Get(chromeos::DIR_SIGNIN_PROFILE_COMPONENT_POLICY,
                                 &cache_dir));

    ResourceCache cache(cache_dir, new base::NullTaskRunner);
    EXPECT_TRUE(cache.Store(kPolicyProtoCacheKey, kTestExtensionId,
                            BuildTestComponentPolicy().SerializeAsString()));
    EXPECT_TRUE(
        cache.Store(kPolicyDataCacheKey, kTestExtensionId, kFakePolicy));
  }

  std::unique_ptr<base::AutoReset<bool>> signin_policy_provided_disabler_;
};

IN_PROC_BROWSER_TEST_F(PreinstalledSigninExtensionsDeviceCloudPolicyBrowserTest,
                       OfflineStart) {
  const extensions::Extension* extension = GetTestExtension();
  ASSERT_TRUE(extension);
  Browser* browser = CreateBrowser(GetSigninProfile());
  extensions::ResultCatcher result_catcher;
  ui_test_utils::NavigateToURL(
      browser, extension->GetResourceURL(kTestExtensionTestPage));
  EXPECT_TRUE(result_catcher.GetNextResult());
  CloseBrowserSynchronously(browser);
}

}  // namespace policy
