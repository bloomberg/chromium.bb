// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/version.h"
#include "chrome/browser/chromeos/policy/signin_profile_extensions_policy_test_base.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/update_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/switches.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {
class StoragePartition;
}

namespace policy {

namespace {

// Parameters for the several extensions and apps that are used by the tests in
// this file (note that the paths are given relative to the src/chrome/test/data
// directory):
// * The manual testing app which is whitelisted for running in the sign-in
//   profile:
const char kWhitelistedAppId[] = "bjaiihebfngildkcjkjckolinodhliff";
const char kWhitelistedAppUpdateManifestPath[] =
    "/extensions/signin_screen_manual_test_app/crx/update_manifest.xml";
// * A trivial test app which is NOT whitelisted for running in the sign-in
//   profile:
const char kNotWhitelistedAppId[] = "mockapnacjbcdncmpkjngjalkhphojek";
const char kNotWhitelistedUpdateManifestPath[] =
    "/extensions/trivial_platform_app/update_manifest.xml";
// * A trivial test extension which is whitelisted for running in the sign-in
//   profile:
const char kWhitelistedExtensionId[] = "ngjobkbdodapjbbncmagbccommkggmnj";
const char kWhitelistedExtensionUpdateManifestPath[] =
    "/extensions/signin_screen_manual_test_extension/update_manifest.xml";
// * A trivial test extension which is NOT whitelisted for running in the
//   sign-in profile:
const char kNotWhitelistedExtensionId[] = "mockepjebcnmhmhcahfddgfcdgkdifnc";
const char kNotWhitelistedExtensionUpdateManifestPath[] =
    "/extensions/trivial_extension/update_manifest.xml";
// * An extension which is NOT whitelisted for running in the sign-in profile
//   and that suppresses its immediate auto updates:
const char kNoImmediateUpdateExtensionId[] = "noidlplbgmdmbccnafgibfgokggdpncj";
const char kNoImmediateUpdateExtensionUpdateManifestPathFormat[] =
    "/extensions/no_immediate_update_extension/crx/%s/update_manifest.xml";
const char kNoImmediateUpdateExtensionLatestVersion[] = "2.0";
const char kNoImmediateUpdateExtensionOlderVersion[] = "1.0";

// Returns the update manifest path for the no_immediate_update_extension with
// the given version.
std::string GetNoImmediateUpdateExtensionUpdateManifestPath(
    const std::string& version) {
  return base::StringPrintf(kNoImmediateUpdateExtensionUpdateManifestPathFormat,
                            version.c_str());
}

// Observer that allows waiting for an installation failure of a specific
// extension/app.
// TODO(emaxx): Extract this into a more generic helper class for using in other
// tests.
class ExtensionInstallErrorObserver final {
 public:
  ExtensionInstallErrorObserver(const Profile* profile,
                                const std::string& extension_id)
      : profile_(profile),
        extension_id_(extension_id),
        notification_observer_(
            extensions::NOTIFICATION_EXTENSION_INSTALL_ERROR,
            base::Bind(&ExtensionInstallErrorObserver::IsNotificationRelevant,
                       base::Unretained(this))) {}

  void Wait() { notification_observer_.Wait(); }

 private:
  // Callback which is used for |WindowedNotificationObserver| for checking
  // whether the condition being awaited is met.
  bool IsNotificationRelevant(
      const content::NotificationSource& source,
      const content::NotificationDetails& details) const {
    extensions::CrxInstaller* const crx_installer =
        content::Source<extensions::CrxInstaller>(source).ptr();
    return crx_installer->profile() == profile_ &&
           crx_installer->extension()->id() == extension_id_;
  }

  const Profile* const profile_;
  const std::string extension_id_;
  content::WindowedNotificationObserver notification_observer_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallErrorObserver);
};

// Observer that allows waiting until the background page of the specified
// extension/app loads.
// TODO(emaxx): Extract this into a more generic helper class for using in other
// tests.
class ExtensionBackgroundPageReadyObserver final {
 public:
  explicit ExtensionBackgroundPageReadyObserver(const std::string& extension_id)
      : extension_id_(extension_id),
        notification_observer_(
            extensions::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
            base::Bind(
                &ExtensionBackgroundPageReadyObserver::IsNotificationRelevant,
                base::Unretained(this))) {}

  void Wait() { notification_observer_.Wait(); }

 private:
  // Callback which is used for |WindowedNotificationObserver| for checking
  // whether the condition being awaited is met.
  bool IsNotificationRelevant(
      const content::NotificationSource& source,
      const content::NotificationDetails& details) const {
    return content::Source<const extensions::Extension>(source)->id() ==
           extension_id_;
  }

  const std::string extension_id_;
  content::WindowedNotificationObserver notification_observer_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionBackgroundPageReadyObserver);
};

// Observer that allows waiting until the specified version of the given
// extension/app gets available for an update.
class ExtensionUpdateAvailabilityObserver final
    : public extensions::UpdateObserver {
 public:
  ExtensionUpdateAvailabilityObserver(Profile* profile,
                                      const std::string& extension_id,
                                      const base::Version& awaited_version)
      : profile_(profile),
        extension_id_(extension_id),
        awaited_version_(awaited_version) {
    extensions::ExtensionSystem::Get(profile_)
        ->extension_service()
        ->AddUpdateObserver(this);
  }

  ExtensionUpdateAvailabilityObserver(
      const ExtensionUpdateAvailabilityObserver&) = delete;
  ExtensionUpdateAvailabilityObserver& operator=(
      const ExtensionUpdateAvailabilityObserver&) = delete;

  ~ExtensionUpdateAvailabilityObserver() override {
    extensions::ExtensionSystem::Get(profile_)
        ->extension_service()
        ->RemoveUpdateObserver(this);
  }

  // Should be called no more than once.
  void Wait() {
    // Note that the expected event could have already been observed before this
    // point, in which case the run loop will exit immediately.
    run_loop_.Run();
  }

  void OnAppUpdateAvailable(const extensions::Extension* extension) override {
    if (extension->id() == extension_id_ &&
        extension->version() == awaited_version_) {
      run_loop_.Quit();
    }
  }

  void OnChromeUpdateAvailable() override {}

 private:
  Profile* const profile_;
  const std::string extension_id_;
  const base::Version awaited_version_;
  base::RunLoop run_loop_;
};

// Class for testing sign-in profile apps/extensions that are installed via the
// device policy under different browser channels.
class SigninProfileExtensionsPolicyPerChannelTest
    : public SigninProfileExtensionsPolicyTestBase,
      public testing::WithParamInterface<version_info::Channel> {
 protected:
  SigninProfileExtensionsPolicyPerChannelTest();

 private:
  DISALLOW_COPY_AND_ASSIGN(SigninProfileExtensionsPolicyPerChannelTest);
};

SigninProfileExtensionsPolicyPerChannelTest::
    SigninProfileExtensionsPolicyPerChannelTest()
    : SigninProfileExtensionsPolicyTestBase(GetParam()) {}

content::StoragePartition* GetStoragePartitionForSigninExtension(
    Profile* profile,
    const std::string& extension_id) {
  return extensions::util::GetStoragePartitionForExtensionId(
      extension_id, profile, /*can_create=*/false);
}

}  // namespace

// Tests that a whitelisted app gets installed on any browser channel.
IN_PROC_BROWSER_TEST_P(SigninProfileExtensionsPolicyPerChannelTest,
                       WhitelistedAppInstallation) {
  Profile* profile = GetInitialProfile();

  extensions::TestExtensionRegistryObserver registry_observer(
      extensions::ExtensionRegistry::Get(profile), kWhitelistedAppId);

  AddExtensionForForceInstallation(kWhitelistedAppId,
                                   kWhitelistedAppUpdateManifestPath);

  registry_observer.WaitForExtensionLoaded();
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(
          kWhitelistedAppId);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(extension->is_platform_app());
}

// Tests that a non-whitelisted app is installed only when on Dev, Canary or
// "unknown" (trunk) channels, but not on Beta or Stable channels.
IN_PROC_BROWSER_TEST_P(SigninProfileExtensionsPolicyPerChannelTest,
                       NotWhitelistedAppInstallation) {
  Profile* profile = GetInitialProfile();

  extensions::TestExtensionRegistryObserver registry_observer(
      extensions::ExtensionRegistry::Get(profile), kNotWhitelistedAppId);
  ExtensionInstallErrorObserver install_error_observer(profile,
                                                       kNotWhitelistedAppId);

  AddExtensionForForceInstallation(kNotWhitelistedAppId,
                                   kNotWhitelistedUpdateManifestPath);

  switch (channel_) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV: {
      registry_observer.WaitForExtensionLoaded();
      const extensions::Extension* extension =
          extensions::ExtensionRegistry::Get(profile)
              ->enabled_extensions()
              .GetByID(kNotWhitelistedAppId);
      ASSERT_TRUE(extension);
      EXPECT_TRUE(extension->is_platform_app());
      break;
    }
    case version_info::Channel::BETA:
    case version_info::Channel::STABLE: {
      install_error_observer.Wait();
      EXPECT_FALSE(
          extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
              kNotWhitelistedAppId));
      break;
    }
  }
}

// Tests that a whitelisted extension is installed on any browser channel.
// Force-installed extensions on the sign-in screen should also automatically
// have the |login_screen_extension| type.
IN_PROC_BROWSER_TEST_P(SigninProfileExtensionsPolicyPerChannelTest,
                       WhitelistedExtensionInstallation) {
  Profile* profile = GetInitialProfile();

  extensions::TestExtensionRegistryObserver registry_observer(
      extensions::ExtensionRegistry::Get(profile), kWhitelistedExtensionId);

  AddExtensionForForceInstallation(kWhitelistedExtensionId,
                                   kWhitelistedExtensionUpdateManifestPath);

  registry_observer.WaitForExtensionLoaded();
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(
          kWhitelistedExtensionId);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(extension->is_login_screen_extension());
}

// Tests that a non-whitelisted extension (as opposed to an app) is forbidden
// from installation regardless of the browser channel.
IN_PROC_BROWSER_TEST_P(SigninProfileExtensionsPolicyPerChannelTest,
                       NotWhitelistedExtensionInstallation) {
  Profile* profile = GetInitialProfile();

  ExtensionInstallErrorObserver install_error_observer(
      profile, kNotWhitelistedExtensionId);
  AddExtensionForForceInstallation(kNotWhitelistedExtensionId,
                                   kNotWhitelistedExtensionUpdateManifestPath);
  install_error_observer.Wait();
  EXPECT_FALSE(
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          kNotWhitelistedExtensionId));
}

INSTANTIATE_TEST_SUITE_P(All,
                         SigninProfileExtensionsPolicyPerChannelTest,
                         testing::Values(version_info::Channel::UNKNOWN,
                                         version_info::Channel::CANARY,
                                         version_info::Channel::DEV,
                                         version_info::Channel::BETA,
                                         version_info::Channel::STABLE));

namespace {

// Class for testing sign-in profile apps/extensions under the "unknown" browser
// channel, which allows to bypass the troublesome whitelist checks.
class SigninProfileExtensionsPolicyTest
    : public SigninProfileExtensionsPolicyTestBase {
 protected:
  SigninProfileExtensionsPolicyTest()
      : SigninProfileExtensionsPolicyTestBase(version_info::Channel::UNKNOWN) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SigninProfileExtensionsPolicyTest);
};

}  // namespace

// Tests that the extension system enables non-standard extensions in the
// sign-in profile.
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsPolicyTest, ExtensionsEnabled) {
  EXPECT_TRUE(extensions::ExtensionSystem::Get(GetInitialProfile())
                  ->extension_service()
                  ->extensions_enabled());
}

// Tests that a background page is created for the installed sign-in profile
// app.
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsPolicyTest, BackgroundPage) {
  EXPECT_FALSE(
      chromeos::ProfileHelper::SigninProfileHasLoginScreenExtensions());
  ExtensionBackgroundPageReadyObserver page_observer(kNotWhitelistedAppId);
  AddExtensionForForceInstallation(kNotWhitelistedAppId,
                                   kNotWhitelistedUpdateManifestPath);
  page_observer.Wait();
  EXPECT_TRUE(chromeos::ProfileHelper::SigninProfileHasLoginScreenExtensions());
}

// Tests installation of multiple sign-in profile apps.
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsPolicyTest, MultipleApps) {
  Profile* profile = GetInitialProfile();

  extensions::TestExtensionRegistryObserver registry_observer1(
      extensions::ExtensionRegistry::Get(profile), kWhitelistedAppId);
  extensions::TestExtensionRegistryObserver registry_observer2(
      extensions::ExtensionRegistry::Get(profile), kNotWhitelistedAppId);

  AddExtensionForForceInstallation(kWhitelistedAppId,
                                   kWhitelistedAppUpdateManifestPath);
  AddExtensionForForceInstallation(kNotWhitelistedAppId,
                                   kNotWhitelistedUpdateManifestPath);

  registry_observer1.WaitForExtensionLoaded();
  registry_observer2.WaitForExtensionLoaded();
}

// Tests that a sign-in profile app or a sign-in profile extension has isolated
// storage, i.e. that it does not reuse the Profile's default StoragePartition.
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsPolicyTest,
                       IsolatedStoragePartition) {
  Profile* profile = GetInitialProfile();

  ExtensionBackgroundPageReadyObserver page_observer_for_app(kWhitelistedAppId);
  ExtensionBackgroundPageReadyObserver page_observer_for_extension(
      kWhitelistedExtensionId);

  AddExtensionForForceInstallation(kWhitelistedAppId,
                                   kWhitelistedAppUpdateManifestPath);
  AddExtensionForForceInstallation(kWhitelistedExtensionId,
                                   kWhitelistedExtensionUpdateManifestPath);

  page_observer_for_app.Wait();
  page_observer_for_extension.Wait();

  content::StoragePartition* storage_partition_for_app =
      GetStoragePartitionForSigninExtension(profile, kWhitelistedAppId);
  content::StoragePartition* storage_partition_for_extension =
      GetStoragePartitionForSigninExtension(profile, kWhitelistedExtensionId);
  content::StoragePartition* default_storage_partition =
      content::BrowserContext::GetDefaultStoragePartition(profile);

  ASSERT_TRUE(storage_partition_for_app);
  ASSERT_TRUE(storage_partition_for_extension);
  ASSERT_TRUE(default_storage_partition);

  EXPECT_NE(default_storage_partition, storage_partition_for_app);
  EXPECT_NE(default_storage_partition, storage_partition_for_extension);
  EXPECT_NE(storage_partition_for_app, storage_partition_for_extension);
}

// Class for testing the sign-in profile extensions with the simulated absence
// of network connectivity.
class SigninProfileExtensionsPolicyOfflineLaunchTest
    : public SigninProfileExtensionsPolicyTest {
 protected:
  void SetUpOnMainThread() override {
    SigninProfileExtensionsPolicyTest::SetUpOnMainThread();

    test_extension_registry_observer_ =
        std::make_unique<extensions::TestExtensionRegistryObserver>(
            extensions::ExtensionRegistry::Get(GetInitialProfile()),
            kWhitelistedAppId);

    AddExtensionForForceInstallation(kWhitelistedAppId,
                                     kWhitelistedAppUpdateManifestPath);

    // In the non-PRE test, this simulates inability to make network requests
    // for fetching the extension update manifest and CRX files. In the PRE test
    // the server is not shut down, in order to allow the initial installation
    // of the extension.
    if (!content::IsPreTest())
      EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  }

  void TearDownOnMainThread() override {
    test_extension_registry_observer_.reset();

    SigninProfileExtensionsPolicyTest::TearDownOnMainThread();
  }

  void WaitForTestExtensionLoaded() {
    test_extension_registry_observer_->WaitForExtensionLoaded();
  }

 private:
  std::unique_ptr<extensions::TestExtensionRegistryObserver>
      test_extension_registry_observer_;
};

// This is the preparation step for the actual test. Here the whitelisted app
// gets installed into the sign-in profile.
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsPolicyOfflineLaunchTest,
                       PRE_Test) {
  WaitForTestExtensionLoaded();
}

// Tests that the whitelisted app gets launched using the cached version even
// when there's no network connection (i.e., neither the extension update
// manifest nor the CRX file can be fetched during this browser execution).
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsPolicyOfflineLaunchTest, Test) {
  WaitForTestExtensionLoaded();
}

// Class for testing the auto update of the sign-in profile extensions.
class SigninProfileExtensionsAutoUpdatePolicyTest
    : public SigninProfileExtensionsPolicyTest {
 public:
  SigninProfileExtensionsAutoUpdatePolicyTest() {
    embedded_test_server()->RegisterRequestHandler(base::Bind(
        &SigninProfileExtensionsAutoUpdatePolicyTest::HandleTestServerRequest,
        base::Unretained(this)));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SigninProfileExtensionsPolicyTest::SetUpCommandLine(command_line);
    // Allow the test extension to be run on the login screen despite not being
    // conformant to the "signin_screen" behavior feature.
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        kNoImmediateUpdateExtensionId);
  }

  void SetUpOnMainThread() override {
    SigninProfileExtensionsPolicyTest::SetUpOnMainThread();

    test_extension_registry_observer_ =
        std::make_unique<extensions::TestExtensionRegistryObserver>(
            extensions::ExtensionRegistry::Get(GetInitialProfile()),
            kNoImmediateUpdateExtensionId);
    test_extension_latest_version_update_available_observer_ =
        std::make_unique<ExtensionUpdateAvailabilityObserver>(
            GetInitialProfile(), kNoImmediateUpdateExtensionId,
            base::Version(kNoImmediateUpdateExtensionLatestVersion));

    AddExtensionForForceInstallation(kNoImmediateUpdateExtensionId,
                                     kRedirectingUpdateManifestPath);
  }

  void TearDownOnMainThread() override {
    test_extension_latest_version_update_available_observer_.reset();
    test_extension_registry_observer_.reset();
    SigninProfileExtensionsPolicyTest::TearDownOnMainThread();
  }

  // Enables serving the test extension's update manifest at the specified
  // version.
  void StartServingTestExtension(const std::string& extension_version) {
    served_extension_version_ = extension_version;
  }

  void WaitForTestExtensionLoaded() {
    test_extension_registry_observer_->WaitForExtensionLoaded();
  }

  void WaitForTestExtensionLatestVersionUpdateAvailable() {
    test_extension_latest_version_update_available_observer_->Wait();
  }

  base::Version GetTestExtensionVersion() {
    const extensions::Extension* const extension =
        extensions::ExtensionRegistry::Get(GetInitialProfile())
            ->enabled_extensions()
            .GetByID(kNoImmediateUpdateExtensionId);
    if (!extension)
      return base::Version();
    return extension->version();
  }

 private:
  // Path on the embedded test server that redirects to the update manifest of
  // the test extension for the version that is currently served.
  const std::string kRedirectingUpdateManifestPath =
      "/redirecting-update-manifest-path.xml";

  // Handler for the embedded test server. Provides special behavior for the
  // test extension's update manifest URL in accordance to
  // |served_extension_version_|.
  std::unique_ptr<net::test_server::HttpResponse> HandleTestServerRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().path() != kRedirectingUpdateManifestPath)
      return nullptr;
    if (served_extension_version_.empty()) {
      // No extension is served now, so return an error.
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
      return response;
    }
    // Redirect to the XML file for the corresponding version.
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_TEMPORARY_REDIRECT);
    response->AddCustomHeader(
        "Location",
        embedded_test_server()
            ->GetURL(GetNoImmediateUpdateExtensionUpdateManifestPath(
                served_extension_version_))
            .spec());
    return response;
  }

  // Specifies which version of the test extension needs to be served. An empty
  // string means that no version is served.
  std::string served_extension_version_;

  std::unique_ptr<extensions::TestExtensionRegistryObserver>
      test_extension_registry_observer_;
  std::unique_ptr<ExtensionUpdateAvailabilityObserver>
      test_extension_latest_version_update_available_observer_;
};

// This is the first preparation step for the actual test. Here the old version
// of the app is served, and it gets installed into the sign-in profile.
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsAutoUpdatePolicyTest,
                       PRE_PRE_Test) {
  StartServingTestExtension(kNoImmediateUpdateExtensionOlderVersion);
  WaitForTestExtensionLoaded();
  EXPECT_EQ(GetTestExtensionVersion(),
            base::Version(kNoImmediateUpdateExtensionOlderVersion));
}

// This is the second preparation step for the actual test. Here the new version
// of the app is served, and it gets fetched and cached.
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsAutoUpdatePolicyTest, PRE_Test) {
  // Let the extensions system load the previously fetched version before
  // starting to serve the newer version, to avoid hitting flaky DCHECKs in the
  // extensions system internals (see https://crbug.com/810799).
  WaitForTestExtensionLoaded();
  EXPECT_EQ(GetTestExtensionVersion(),
            base::Version(kNoImmediateUpdateExtensionOlderVersion));

  // Start serving the newer version. The extensions system should eventually
  // fetch this version due to the retry mechanism when the fetch request to the
  // update servers was failing. We verify that the new version eventually gets
  // fetched and becomes available for an update.
  StartServingTestExtension(kNoImmediateUpdateExtensionLatestVersion);
  WaitForTestExtensionLatestVersionUpdateAvailable();

  // The running extension should stay at the older version, since it ignores
  // update notifications and never idles, and also the browser is expected to
  // not force immediate updates.
  // Note: There's no reliable way to test that the preliminary autoupdate
  // doesn't happen, but doing RunUntilIdle() at this point should make the test
  // at least flaky in case a bug is introduced somewhere.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetTestExtensionVersion(),
            base::Version(kNoImmediateUpdateExtensionOlderVersion));
}

// This is the actual test. Here we verify that the new version of the app, as
// fetched in the PRE_Test, gets launched even in the "offline" mode (since
// we're not serving any version of the extension in this part of the test).
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsAutoUpdatePolicyTest, Test) {
  WaitForTestExtensionLoaded();
  EXPECT_EQ(GetTestExtensionVersion(),
            base::Version(kNoImmediateUpdateExtensionLatestVersion));
}

}  // namespace policy
