// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop_proxy.h"
#include "chrome/browser/apps/ephemeral_app_launcher.h"
#include "chrome/browser/extensions/extension_install_checker.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/extensions/test_blacklist.h"
#include "chrome/browser/extensions/webstore_installer_test.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/management_policy.h"

using extensions::Extension;
using extensions::ExtensionPrefs;
using extensions::ExtensionRegistry;
using extensions::ExtensionSystem;
using extensions::InstallTracker;
namespace webstore_install = extensions::webstore_install;

namespace {

const char kWebstoreDomain[] = "cws.com";
const char kAppDomain[] = "app.com";
const char kNonAppDomain[] = "nonapp.com";
const char kTestDataPath[] = "extensions/platform_apps/ephemeral_launcher";

const char kExtensionId[] = "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeid";
const char kExtensionTestPath[] = "extension";
const char kLegacyAppId[] = "lnbochkobjfnhbnbljgfgokadhmbahcn";
const char kLegacyAppTestPath[] = "legacy_app";
const char kNonExistentId[] = "baaaaaaaaaaaaaaaaaaaaaaaaaaaadid";
const char kDefaultAppId[] = "kbiancnbopdghkfedjhfdoegjadfjeal";
const char kDefaultAppCrxFilename[] = "app.crx";
const char kDefaultAppTestPath[] = "app";
const char kAppWithPermissionsId[] = "mbfcnecjknjpipkfkoangpfnhhlpamki";
const char kAppWithPermissionsFilename[] = "app_with_permissions.crx";
const char kHostedAppId[] = "haaaaaaaaaaaaaaaaaaaaaaaaaaappid";
const char kHostedAppLaunchUrl[] = "http://foo.bar.com";

class ExtensionInstallCheckerMock : public extensions::ExtensionInstallChecker {
 public:
  ExtensionInstallCheckerMock(Profile* profile,
                              const std::string& requirements_error)
      : extensions::ExtensionInstallChecker(profile),
        requirements_error_(requirements_error) {}

  virtual ~ExtensionInstallCheckerMock() {}

 private:
  virtual void CheckRequirements() OVERRIDE {
    // Simulate an asynchronous operation.
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(&ExtensionInstallCheckerMock::RequirementsErrorCheckDone,
                   base::Unretained(this),
                   current_sequence_number()));
  }

  void RequirementsErrorCheckDone(int sequence_number) {
    std::vector<std::string> errors;
    errors.push_back(requirements_error_);
    OnRequirementsCheckDone(sequence_number, errors);
  }

  std::string requirements_error_;
};

class EphemeralAppLauncherForTest : public EphemeralAppLauncher {
 public:
  EphemeralAppLauncherForTest(const std::string& id,
                              Profile* profile,
                              const LaunchCallback& callback)
      : EphemeralAppLauncher(id, profile, NULL, callback),
        install_initiated_(false),
        install_prompt_created_(false) {}

  EphemeralAppLauncherForTest(const std::string& id, Profile* profile)
      : EphemeralAppLauncher(id, profile, NULL, LaunchCallback()),
        install_initiated_(false),
        install_prompt_created_(false) {}

  bool install_initiated() const { return install_initiated_; }
  bool install_prompt_created() const { return install_prompt_created_; }

  void set_requirements_error(const std::string& error) {
    requirements_check_error_ = error;
  }

 private:
  // Override necessary functions for testing.

  virtual scoped_ptr<extensions::ExtensionInstallChecker> CreateInstallChecker()
      OVERRIDE {
    if (requirements_check_error_.empty()) {
      return EphemeralAppLauncher::CreateInstallChecker();
    } else {
      return scoped_ptr<extensions::ExtensionInstallChecker>(
          new ExtensionInstallCheckerMock(profile(),
                                          requirements_check_error_));
    }
  }

  virtual scoped_ptr<ExtensionInstallPrompt> CreateInstallUI() OVERRIDE {
    install_prompt_created_ = true;
    return EphemeralAppLauncher::CreateInstallUI();
  }

  virtual scoped_ptr<extensions::WebstoreInstaller::Approval> CreateApproval()
      const OVERRIDE {
    install_initiated_ = true;
    return EphemeralAppLauncher::CreateApproval();
  }

 private:
  virtual ~EphemeralAppLauncherForTest() {}
  friend class base::RefCountedThreadSafe<EphemeralAppLauncherForTest>;

  mutable bool install_initiated_;
  std::string requirements_check_error_;
  bool install_prompt_created_;
};

class LaunchObserver {
 public:
  LaunchObserver()
      : done_(false),
        waiting_(false),
        result_(webstore_install::UNKNOWN_ERROR) {}

  webstore_install::Result result() const { return result_; }
  const std::string& error() const { return error_; }

  void OnLaunchCallback(webstore_install::Result result,
                        const std::string& error) {
    result_ = result;
    error_ = error;
    done_ = true;
    if (waiting_) {
      waiting_ = false;
      base::MessageLoopForUI::current()->Quit();
    }
  }

  void Wait() {
    if (done_)
      return;

    waiting_ = true;
    content::RunMessageLoop();
  }

 private:
  bool done_;
  bool waiting_;
  webstore_install::Result result_;
  std::string error_;
};

class ManagementPolicyMock : public extensions::ManagementPolicy::Provider {
 public:
  ManagementPolicyMock() {}

  virtual std::string GetDebugPolicyProviderName() const OVERRIDE {
    return "ManagementPolicyMock";
  }

  virtual bool UserMayLoad(const Extension* extension,
                           base::string16* error) const OVERRIDE {
    return false;
  }
};

}  // namespace

class EphemeralAppLauncherTest : public WebstoreInstallerTest {
 public:
  EphemeralAppLauncherTest()
      : WebstoreInstallerTest(kWebstoreDomain,
                              kTestDataPath,
                              kDefaultAppCrxFilename,
                              kAppDomain,
                              kNonAppDomain) {}

  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE {
    WebstoreInstallerTest::SetUpCommandLine(command_line);

    // Enable ephemeral apps flag.
    command_line->AppendSwitch(switches::kEnableEphemeralApps);
  }

  base::FilePath GetTestPath(const char* test_name) {
    return test_data_dir_.AppendASCII("platform_apps/ephemeral_launcher")
        .AppendASCII(test_name);
  }

  const Extension* GetInstalledExtension(const std::string& id) {
    return ExtensionRegistry::Get(profile())
        ->GetExtensionById(id, ExtensionRegistry::EVERYTHING);
  }

  void SetCrxFilename(const std::string& filename) {
    GURL crx_url = GenerateTestServerUrl(kWebstoreDomain, filename);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAppsGalleryUpdateURL, crx_url.spec());
  }

  void StartLauncherAndCheckResult(EphemeralAppLauncherForTest* launcher,
                                   webstore_install::Result expected_result,
                                   bool expect_install_initiated) {
    ExtensionTestMessageListener launched_listener("launched", false);
    LaunchObserver launch_observer;

    launcher->launch_callback_ = base::Bind(&LaunchObserver::OnLaunchCallback,
                                            base::Unretained(&launch_observer));
    launcher->Start();
    launch_observer.Wait();

    // Verify the launch result.
    EXPECT_EQ(expected_result, launch_observer.result());
    EXPECT_EQ(expect_install_initiated, launcher->install_initiated());

    // Verify that the app was actually launched if the launcher succeeded.
    if (launch_observer.result() == webstore_install::SUCCESS)
      EXPECT_TRUE(launched_listener.WaitUntilSatisfied());
    else
      EXPECT_FALSE(launched_listener.was_satisfied());

    // Check the reference count to ensure the launcher instance will not be
    // leaked.
    EXPECT_TRUE(launcher->HasOneRef());
  }

  void RunLaunchTest(const std::string& id,
                     webstore_install::Result expected_result,
                     bool expect_install_initiated) {
    InstallTracker* tracker = InstallTracker::Get(profile());
    ASSERT_TRUE(tracker);
    bool was_install_active = !!tracker->GetActiveInstall(id);

    scoped_refptr<EphemeralAppLauncherForTest> launcher(
        new EphemeralAppLauncherForTest(id, profile()));
    StartLauncherAndCheckResult(
        launcher.get(), expected_result, expect_install_initiated);

    // Verify that the install was deregistered from the InstallTracker.
    EXPECT_EQ(was_install_active, !!tracker->GetActiveInstall(id));
  }

  void ValidateAppInstalledEphemerally(const std::string& id) {
    EXPECT_TRUE(GetInstalledExtension(id));
    EXPECT_TRUE(extensions::util::IsEphemeralApp(id, profile()));
  }

  const Extension* InstallAndDisableApp(
      const char* test_path,
      Extension::DisableReason disable_reason) {
    const Extension* app = InstallExtension(GetTestPath(test_path), 1);
    EXPECT_TRUE(app);
    if (!app)
      return NULL;

    if (disable_reason == Extension::DISABLE_GREYLIST) {
      ExtensionPrefs::Get(profile())->SetExtensionBlacklistState(
          app->id(), extensions::BLACKLISTED_MALWARE);
    }

    ExtensionService* service =
        ExtensionSystem::Get(profile())->extension_service();
    service->DisableExtension(app->id(), disable_reason);

    if (disable_reason == Extension::DISABLE_PERMISSIONS_INCREASE) {
      // When an extension is disabled due to a permissions increase, this
      // flag needs to be set too, for some reason.
      ExtensionPrefs::Get(profile())
          ->SetDidExtensionEscalatePermissions(app, true);
    }

    EXPECT_FALSE(
        ExtensionRegistry::Get(profile())->enabled_extensions().Contains(
            app->id()));
    return app;
  }
};

class EphemeralAppLauncherTestDisabled : public EphemeralAppLauncherTest {
 public:
  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE {
    // Skip EphemeralAppLauncherTest as it enables the feature.
    WebstoreInstallerTest::SetUpCommandLine(command_line);
  }
};

// Verifies that an ephemeral app will not be installed and launched if the
// feature is disabled.
IN_PROC_BROWSER_TEST_F(EphemeralAppLauncherTestDisabled, FeatureDisabled) {
  RunLaunchTest(
      kDefaultAppCrxFilename, webstore_install::LAUNCH_FEATURE_DISABLED, false);
  EXPECT_FALSE(GetInstalledExtension(kDefaultAppId));
}

// Verifies that an app with no permission warnings will be installed
// ephemerally and launched without prompting the user.
IN_PROC_BROWSER_TEST_F(EphemeralAppLauncherTest,
                       LaunchAppWithNoPermissionWarnings) {
  scoped_refptr<EphemeralAppLauncherForTest> launcher(
      new EphemeralAppLauncherForTest(kDefaultAppId, profile()));
  StartLauncherAndCheckResult(launcher.get(), webstore_install::SUCCESS, true);
  ValidateAppInstalledEphemerally(kDefaultAppId);

  // Apps with no permission warnings should not result in a prompt.
  EXPECT_FALSE(launcher->install_prompt_created());

  // After an app has been installed ephemerally, it can be launched again
  // without installing from the web store.
  RunLaunchTest(kDefaultAppId, webstore_install::SUCCESS, false);
}

// Verifies that an app with permission warnings will be installed
// ephemerally and launched if accepted by the user.
IN_PROC_BROWSER_TEST_F(EphemeralAppLauncherTest,
                       LaunchAppWithPermissionsWarnings) {
  SetCrxFilename(kAppWithPermissionsFilename);
  AutoAcceptInstall();

  scoped_refptr<EphemeralAppLauncherForTest> launcher(
      new EphemeralAppLauncherForTest(kAppWithPermissionsId, profile()));
  StartLauncherAndCheckResult(launcher.get(), webstore_install::SUCCESS, true);
  ValidateAppInstalledEphemerally(kAppWithPermissionsId);
  EXPECT_TRUE(launcher->install_prompt_created());
}

// Verifies that an app with permission warnings will not be installed
// ephemerally if cancelled by the user.
IN_PROC_BROWSER_TEST_F(EphemeralAppLauncherTest,
                       CancelInstallAppWithPermissionWarnings) {
  SetCrxFilename(kAppWithPermissionsFilename);
  AutoCancelInstall();

  scoped_refptr<EphemeralAppLauncherForTest> launcher(
      new EphemeralAppLauncherForTest(kAppWithPermissionsId, profile()));
  StartLauncherAndCheckResult(
      launcher.get(), webstore_install::USER_CANCELLED, false);
  EXPECT_FALSE(GetInstalledExtension(kAppWithPermissionsId));
  EXPECT_TRUE(launcher->install_prompt_created());
}

// Verifies that an extension will not be installed ephemerally.
IN_PROC_BROWSER_TEST_F(EphemeralAppLauncherTest, InstallExtension) {
  RunLaunchTest(
      kExtensionId, webstore_install::LAUNCH_UNSUPPORTED_EXTENSION_TYPE, false);
  EXPECT_FALSE(GetInstalledExtension(kExtensionId));
}

// Verifies that an already installed extension will not be launched.
IN_PROC_BROWSER_TEST_F(EphemeralAppLauncherTest, LaunchExtension) {
  const Extension* extension =
      InstallExtension(GetTestPath(kExtensionTestPath), 1);
  ASSERT_TRUE(extension);
  RunLaunchTest(extension->id(),
                webstore_install::LAUNCH_UNSUPPORTED_EXTENSION_TYPE,
                false);
}

// Verifies that a legacy packaged app will not be installed ephemerally.
IN_PROC_BROWSER_TEST_F(EphemeralAppLauncherTest, InstallLegacyApp) {
  RunLaunchTest(
      kLegacyAppId, webstore_install::LAUNCH_UNSUPPORTED_EXTENSION_TYPE, false);
  EXPECT_FALSE(GetInstalledExtension(kLegacyAppId));
}

// Verifies that a legacy packaged app that is already installed can be
// launched.
IN_PROC_BROWSER_TEST_F(EphemeralAppLauncherTest, LaunchLegacyApp) {
  const Extension* extension =
      InstallExtension(GetTestPath(kLegacyAppTestPath), 1);
  ASSERT_TRUE(extension);
  RunLaunchTest(extension->id(), webstore_install::SUCCESS, false);
}

// Verifies that a hosted app is not installed. Launch succeeds because we
// navigate to its launch url.
IN_PROC_BROWSER_TEST_F(EphemeralAppLauncherTest, LaunchHostedApp) {
  LaunchObserver launch_observer;

  scoped_refptr<EphemeralAppLauncherForTest> launcher(
      new EphemeralAppLauncherForTest(
          kHostedAppId,
          profile(),
          base::Bind(&LaunchObserver::OnLaunchCallback,
                     base::Unretained(&launch_observer))));
  launcher->Start();
  launch_observer.Wait();

  EXPECT_EQ(webstore_install::SUCCESS, launch_observer.result());
  EXPECT_FALSE(launcher->install_initiated());
  EXPECT_FALSE(GetInstalledExtension(kHostedAppId));

  // Verify that a navigation to the launch url was attempted.
  Browser* browser =
      FindBrowserWithProfile(profile(), chrome::GetActiveDesktop());
  ASSERT_TRUE(browser);
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_EQ(GURL(kHostedAppLaunchUrl), web_contents->GetVisibleURL());
}

// Verifies that the EphemeralAppLauncher handles non-existent extension ids.
IN_PROC_BROWSER_TEST_F(EphemeralAppLauncherTest, NonExistentExtensionId) {
  RunLaunchTest(
      kNonExistentId, webstore_install::WEBSTORE_REQUEST_ERROR, false);
  EXPECT_FALSE(GetInstalledExtension(kNonExistentId));
}

// Verifies that an app blocked by management policy is not installed
// ephemerally.
IN_PROC_BROWSER_TEST_F(EphemeralAppLauncherTest, BlockedByPolicy) {
  // Register a provider that blocks the installation of all apps.
  ManagementPolicyMock policy;
  ExtensionSystem::Get(profile())->management_policy()->RegisterProvider(
      &policy);

  RunLaunchTest(kDefaultAppId, webstore_install::BLOCKED_BY_POLICY, false);
  EXPECT_FALSE(GetInstalledExtension(kDefaultAppId));
}

// Verifies that an app blacklisted for malware is not installed ephemerally.
IN_PROC_BROWSER_TEST_F(EphemeralAppLauncherTest, BlacklistedForMalware) {
  // Mock a BLACKLISTED_MALWARE return status.
  extensions::TestBlacklist blacklist_tester(
      ExtensionSystem::Get(profile())->blacklist());
  blacklist_tester.SetBlacklistState(
      kDefaultAppId, extensions::BLACKLISTED_MALWARE, false);

  RunLaunchTest(kDefaultAppId, webstore_install::BLACKLISTED, false);
  EXPECT_FALSE(GetInstalledExtension(kDefaultAppId));
}

// Verifies that an app with unknown blacklist status is installed ephemerally
// and launched.
IN_PROC_BROWSER_TEST_F(EphemeralAppLauncherTest, BlacklistStateUnknown) {
  // Mock a BLACKLISTED_MALWARE return status.
  extensions::TestBlacklist blacklist_tester(
      ExtensionSystem::Get(profile())->blacklist());
  blacklist_tester.SetBlacklistState(
      kDefaultAppId, extensions::BLACKLISTED_UNKNOWN, false);

  RunLaunchTest(kDefaultAppId, webstore_install::SUCCESS, true);
  ValidateAppInstalledEphemerally(kDefaultAppId);
}

// Verifies that an app with unsupported requirements is not installed
// ephemerally.
IN_PROC_BROWSER_TEST_F(EphemeralAppLauncherTest, UnsupportedRequirements) {
  scoped_refptr<EphemeralAppLauncherForTest> launcher(
      new EphemeralAppLauncherForTest(kDefaultAppId, profile()));
  launcher->set_requirements_error("App has unsupported requirements");

  StartLauncherAndCheckResult(
      launcher.get(), webstore_install::REQUIREMENT_VIOLATIONS, false);
  EXPECT_FALSE(GetInstalledExtension(kDefaultAppId));
}

// Verifies that an app disabled due to permissions increase can be enabled
// and launched.
IN_PROC_BROWSER_TEST_F(EphemeralAppLauncherTest, EnableAndLaunchApp) {
  const Extension* app = InstallAndDisableApp(
      kDefaultAppTestPath, Extension::DISABLE_PERMISSIONS_INCREASE);
  ASSERT_TRUE(app);

  AutoAcceptInstall();
  RunLaunchTest(app->id(), webstore_install::SUCCESS, false);
}

// Verifies that if the user cancels the enable flow, the app will not be
// enabled and launched.
IN_PROC_BROWSER_TEST_F(EphemeralAppLauncherTest, EnableCancelled) {
  const Extension* app = InstallAndDisableApp(
      kDefaultAppTestPath, Extension::DISABLE_PERMISSIONS_INCREASE);
  ASSERT_TRUE(app);

  AutoCancelInstall();
  RunLaunchTest(app->id(), webstore_install::USER_CANCELLED, false);
}

// Verifies that an installed app that had been blocked by policy cannot be
// launched.
IN_PROC_BROWSER_TEST_F(EphemeralAppLauncherTest, LaunchAppBlockedByPolicy) {
  const Extension* app = InstallExtension(GetTestPath(kDefaultAppTestPath), 1);
  ASSERT_TRUE(app);

  // Simulate blocking of the app after it has been installed.
  ManagementPolicyMock policy;
  ExtensionSystem::Get(profile())->management_policy()->RegisterProvider(
      &policy);
  ExtensionSystem::Get(profile())->extension_service()->CheckManagementPolicy();

  RunLaunchTest(app->id(), webstore_install::BLOCKED_BY_POLICY, false);
}

// Verifies that an installed blacklisted app cannot be launched.
IN_PROC_BROWSER_TEST_F(EphemeralAppLauncherTest, LaunchBlacklistedApp) {
  const Extension* app =
      InstallAndDisableApp(kDefaultAppTestPath, Extension::DISABLE_GREYLIST);
  ASSERT_TRUE(app);

  RunLaunchTest(app->id(), webstore_install::BLACKLISTED, false);
}

// Verifies that an installed app with unsupported requirements cannot be
// launched.
IN_PROC_BROWSER_TEST_F(EphemeralAppLauncherTest,
                       LaunchAppWithUnsupportedRequirements) {
  const Extension* app = InstallAndDisableApp(
      kDefaultAppTestPath, Extension::DISABLE_UNSUPPORTED_REQUIREMENT);
  ASSERT_TRUE(app);

  RunLaunchTest(app->id(), webstore_install::REQUIREMENT_VIOLATIONS, false);
}

// Verifies that a launch will fail if the app is currently being installed.
IN_PROC_BROWSER_TEST_F(EphemeralAppLauncherTest, InstallInProgress) {
  extensions::ActiveInstallData install_data(kDefaultAppId);
  InstallTracker::Get(profile())->AddActiveInstall(install_data);

  RunLaunchTest(kDefaultAppId, webstore_install::INSTALL_IN_PROGRESS, false);
}

// Verifies that a launch will fail if a duplicate launch is in progress.
IN_PROC_BROWSER_TEST_F(EphemeralAppLauncherTest, DuplicateLaunchInProgress) {
  extensions::ActiveInstallData install_data(kDefaultAppId);
  install_data.is_ephemeral = true;
  InstallTracker::Get(profile())->AddActiveInstall(install_data);

  RunLaunchTest(kDefaultAppId, webstore_install::LAUNCH_IN_PROGRESS, false);
}
