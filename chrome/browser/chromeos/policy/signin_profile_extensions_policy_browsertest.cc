// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/features/feature_channel.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

namespace {

// Parameters for the several extensions and apps that are used by the tests in
// this file (note that the paths are given relative to the src/chrome/test/data
// directory):
// * The manual testing app which is whitelisted for running in the sign-in
//   profile:
const char kWhitelistedAppId[] = "bjaiihebfngildkcjkjckolinodhliff";
const char kWhitelistedAppUpdateManifestPath[] =
    "/extensions/signin_screen_manual_test_app/update_manifest.xml";
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

// Returns the initial profile, which is the original profile of the sign-in
// profile. The apps/extensions specified in the policy will be installed into
// the initial profile, and then become available in both.
Profile* GetProfile() {
  // Intentionally not using the |chromeos::ProfileHelper::GetSigninProfile|
  // method here, as it performs the lazy construction of the profile, while for
  // the testing purposes it's better to assert that it has been created before.
  Profile* const profile =
      g_browser_process->profile_manager()->GetProfileByPath(
          chromeos::ProfileHelper::GetSigninProfileDir());
  DCHECK(profile);
  return profile;
}

}  // namespace

namespace {

// Base class for testing sign-in profile apps/extensions that are installed via
// the device policy.
class SigninProfileExtensionsPolicyTestBase
    : public DevicePolicyCrosBrowserTest {
 protected:
  explicit SigninProfileExtensionsPolicyTestBase(version_info::Channel channel)
      : channel_(channel), scoped_current_channel_(channel) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
  }

  void SetUpOnMainThread() override {
    DevicePolicyCrosBrowserTest::SetUpOnMainThread();

    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &SigninProfileExtensionsPolicyTestBase::InterceptMockHttp,
        base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void AddExtensionForForceInstallation(
      const std::string& extension_id,
      const std::string& update_manifest_relative_path) {
    const GURL update_manifest_url =
        embedded_test_server()->GetURL(update_manifest_relative_path);
    const std::string policy_item_value = base::ReplaceStringPlaceholders(
        "$1;$2", {extension_id, update_manifest_url.spec()}, nullptr);
    device_policy()
        ->payload()
        .mutable_device_login_screen_extensions()
        ->add_device_login_screen_extensions(policy_item_value);
    RefreshDevicePolicy();
  }

  const version_info::Channel channel_;

 private:
  // Replace "mock.http" with "127.0.0.1:<port>" on "update_manifest.xml" files.
  // Host resolver doesn't work here because the test file doesn't know the
  // correct port number.
  std::unique_ptr<net::test_server::HttpResponse> InterceptMockHttp(
      const net::test_server::HttpRequest& request) {
    const std::string kFileNameToIntercept = "update_manifest.xml";
    if (request.GetURL().ExtractFileName() != kFileNameToIntercept)
      return nullptr;

    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    // Remove the leading '/'.
    std::string relative_manifest_path = request.GetURL().path().substr(1);
    std::string manifest_response;
    CHECK(base::ReadFileToString(test_data_dir.Append(relative_manifest_path),
                                 &manifest_response));

    base::ReplaceSubstringsAfterOffset(
        &manifest_response, 0, "mock.http",
        embedded_test_server()->host_port_pair().ToString());

    std::unique_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse());
    response->set_content_type("text/xml");
    response->set_content(manifest_response);
    return response;
  }

  const extensions::ScopedCurrentChannel scoped_current_channel_;

  DISALLOW_COPY_AND_ASSIGN(SigninProfileExtensionsPolicyTestBase);
};

// Class for testing sign-in profile apps/extensions under different browser
// channels.
class SigninProfileExtensionsPolicyPerChannelTest
    : public SigninProfileExtensionsPolicyTestBase,
      public testing::WithParamInterface<version_info::Channel> {
 protected:
  SigninProfileExtensionsPolicyPerChannelTest()
      : SigninProfileExtensionsPolicyTestBase(GetParam()) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SigninProfileExtensionsPolicyPerChannelTest);
};

}  // namespace

// Tests that a whitelisted app gets installed on any browser channel.
IN_PROC_BROWSER_TEST_P(SigninProfileExtensionsPolicyPerChannelTest,
                       WhitelistedAppInstallation) {
  extensions::TestExtensionRegistryObserver registry_observer(
      extensions::ExtensionRegistry::Get(GetProfile()), kWhitelistedAppId);

  AddExtensionForForceInstallation(kWhitelistedAppId,
                                   kWhitelistedAppUpdateManifestPath);

  registry_observer.WaitForExtensionLoaded();
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(GetProfile())
          ->enabled_extensions()
          .GetByID(kWhitelistedAppId);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(extension->is_platform_app());
}

// Tests that a non-whitelisted app is installed only when on Dev, Canary or
// "unknown" (trunk) channels, but not on Beta or Stable channels.
IN_PROC_BROWSER_TEST_P(SigninProfileExtensionsPolicyPerChannelTest,
                       NotWhitelistedAppInstallation) {
  extensions::TestExtensionRegistryObserver registry_observer(
      extensions::ExtensionRegistry::Get(GetProfile()), kNotWhitelistedAppId);
  ExtensionInstallErrorObserver install_error_observer(GetProfile(),
                                                       kNotWhitelistedAppId);

  AddExtensionForForceInstallation(kNotWhitelistedAppId,
                                   kNotWhitelistedUpdateManifestPath);

  switch (channel_) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV: {
      registry_observer.WaitForExtensionLoaded();
      const extensions::Extension* extension =
          extensions::ExtensionRegistry::Get(GetProfile())
              ->enabled_extensions()
              .GetByID(kNotWhitelistedAppId);
      ASSERT_TRUE(extension);
      EXPECT_TRUE(extension->is_platform_app());
      break;
    }
    case version_info::Channel::BETA:
    case version_info::Channel::STABLE: {
      install_error_observer.Wait();
      EXPECT_FALSE(extensions::ExtensionRegistry::Get(GetProfile())
                       ->GetInstalledExtension(kNotWhitelistedAppId));
      break;
    }
  }
}

// Tests that a whitelisted extension (as opposed to an app) gets installed when
// on Dev, Canary or "unknown" (trunk) channels, but not on Beta or Stable
// channels. Force-installed extensions on the sign-in screen should also
// automatically have the |login_screen_extension| type.
IN_PROC_BROWSER_TEST_P(SigninProfileExtensionsPolicyPerChannelTest,
                       WhitelistedExtensionInstallation) {
  extensions::TestExtensionRegistryObserver registry_observer(
      extensions::ExtensionRegistry::Get(GetProfile()),
      kWhitelistedExtensionId);
  ExtensionInstallErrorObserver install_error_observer(GetProfile(),
                                                       kWhitelistedExtensionId);

  AddExtensionForForceInstallation(kWhitelistedExtensionId,
                                   kWhitelistedExtensionUpdateManifestPath);

  switch (channel_) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV: {
      registry_observer.WaitForExtensionLoaded();
      const extensions::Extension* extension =
          extensions::ExtensionRegistry::Get(GetProfile())
              ->enabled_extensions()
              .GetByID(kWhitelistedExtensionId);
      ASSERT_TRUE(extension);
      EXPECT_TRUE(extension->is_login_screen_extension());
      break;
    }
    case version_info::Channel::BETA:
    case version_info::Channel::STABLE: {
      install_error_observer.Wait();
      EXPECT_FALSE(extensions::ExtensionRegistry::Get(GetProfile())
                       ->GetInstalledExtension(kWhitelistedExtensionId));
      break;
    }
  }
}

// Tests that a non-whitelisted extension (as opposed to an app) is forbidden
// from installation regardless of the browser channel.
IN_PROC_BROWSER_TEST_P(SigninProfileExtensionsPolicyPerChannelTest,
                       NotWhitelistedExtensionInstallation) {
  ExtensionInstallErrorObserver install_error_observer(
      GetProfile(), kNotWhitelistedExtensionId);
  AddExtensionForForceInstallation(kNotWhitelistedExtensionId,
                                   kNotWhitelistedExtensionUpdateManifestPath);
  install_error_observer.Wait();
  EXPECT_FALSE(extensions::ExtensionRegistry::Get(GetProfile())
                   ->GetInstalledExtension(kNotWhitelistedExtensionId));
}

INSTANTIATE_TEST_SUITE_P(,
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
  EXPECT_TRUE(extensions::ExtensionSystem::Get(GetProfile())
                  ->extension_service()
                  ->extensions_enabled());
}

// Tests that a background page is created for the installed sign-in profile
// app.
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsPolicyTest, BackgroundPage) {
  ExtensionBackgroundPageReadyObserver page_observer(kNotWhitelistedAppId);
  AddExtensionForForceInstallation(kNotWhitelistedAppId,
                                   kNotWhitelistedUpdateManifestPath);
  page_observer.Wait();
}

// Tests installation of multiple sign-in profile apps.
IN_PROC_BROWSER_TEST_F(SigninProfileExtensionsPolicyTest, MultipleApps) {
  extensions::TestExtensionRegistryObserver registry_observer1(
      extensions::ExtensionRegistry::Get(GetProfile()), kWhitelistedAppId);
  extensions::TestExtensionRegistryObserver registry_observer2(
      extensions::ExtensionRegistry::Get(GetProfile()), kNotWhitelistedAppId);

  AddExtensionForForceInstallation(kWhitelistedAppId,
                                   kWhitelistedAppUpdateManifestPath);
  AddExtensionForForceInstallation(kNotWhitelistedAppId,
                                   kNotWhitelistedUpdateManifestPath);

  registry_observer1.WaitForExtensionLoaded();
  registry_observer2.WaitForExtensionLoaded();
}

}  // namespace policy
