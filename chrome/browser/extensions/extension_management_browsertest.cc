// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/extensions/autoupdate_interceptor.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/extension_updater.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"

class ExtensionManagementTest : public ExtensionBrowserTest {
 protected:
  // Helper method that returns whether the extension is at the given version.
  // This calls version(), which must be defined in the extension's bg page,
  // as well as asking the extension itself.
  //
  // Note that 'version' here means something different than the version field
  // in the extension's manifest. We use the version as reported by the
  // background page to test how overinstalling crx files with the same
  // manifest version works.
  bool IsExtensionAtVersion(const Extension* extension,
                            const std::string& expected_version) {
    // Test that the extension's version from the manifest and reported by the
    // background page is correct.  This is to ensure that the processes are in
    // sync with the Extension.
    ExtensionProcessManager* manager = browser()->profile()->
        GetExtensionProcessManager();
    ExtensionHost* ext_host = manager->GetBackgroundHostForExtension(extension);
    EXPECT_TRUE(ext_host);
    if (!ext_host)
      return false;

    std::string version_from_bg;
    bool exec = ui_test_utils::ExecuteJavaScriptAndExtractString(
        ext_host->render_view_host(), L"", L"version()", &version_from_bg);
    EXPECT_TRUE(exec);
    if (!exec)
      return false;

    if (version_from_bg != expected_version ||
        extension->VersionString() != expected_version)
      return false;
    return true;
  }

  // Helper method that installs a low permission extension then updates
  // to the second version requiring increased permissions. Returns whether
  // the operation was completed successfully.
  bool InstallAndUpdateIncreasingPermissionsExtension() {
    ExtensionService* service = browser()->profile()->GetExtensionService();
    size_t size_before = service->extensions()->size();

    // Install the initial version, which should happen just fine.
    if (!InstallExtension(
        test_data_dir_.AppendASCII("permissions-low-v1.crx"), 1))
      return false;

    // Upgrade to a version that wants more permissions. We should disable the
    // extension and prompt the user to reenable.
    if (service->extensions()->size() != size_before + 1)
      return false;
    if (!UpdateExtension(
        service->extensions()->at(size_before)->id(),
        test_data_dir_.AppendASCII("permissions-high-v2.crx"), -1))
      return false;
    EXPECT_EQ(size_before, service->extensions()->size());
    if (service->disabled_extensions()->size() != 1u)
      return false;
    return true;
  }
};

// Tests that installing the same version overwrites.
IN_PROC_BROWSER_TEST_F(ExtensionManagementTest, InstallSameVersion) {
  ExtensionService* service = browser()->profile()->GetExtensionService();
  const size_t size_before = service->extensions()->size();
  ASSERT_TRUE(InstallExtension(
      test_data_dir_.AppendASCII("install/install.crx"), 1));
  FilePath old_path = service->extensions()->back()->path();

  // Install an extension with the same version. The previous install should be
  // overwritten.
  ASSERT_TRUE(InstallExtension(
      test_data_dir_.AppendASCII("install/install_same_version.crx"), 0));
  FilePath new_path = service->extensions()->back()->path();

  EXPECT_FALSE(IsExtensionAtVersion(service->extensions()->at(size_before),
                                    "1.0"));
  EXPECT_NE(old_path.value(), new_path.value());
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementTest, InstallOlderVersion) {
  ExtensionService* service = browser()->profile()->GetExtensionService();
  const size_t size_before = service->extensions()->size();
  ASSERT_TRUE(InstallExtension(
      test_data_dir_.AppendASCII("install/install.crx"), 1));
  ASSERT_TRUE(InstallExtension(
      test_data_dir_.AppendASCII("install/install_older_version.crx"), 0));
  EXPECT_TRUE(IsExtensionAtVersion(service->extensions()->at(size_before),
                                   "1.0"));
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementTest, InstallThenCancel) {
  ExtensionService* service = browser()->profile()->GetExtensionService();
  const size_t size_before = service->extensions()->size();
  ASSERT_TRUE(InstallExtension(
      test_data_dir_.AppendASCII("install/install.crx"), 1));

  // Cancel this install.
  StartInstallButCancel(test_data_dir_.AppendASCII("install/install_v2.crx"));
  EXPECT_TRUE(IsExtensionAtVersion(service->extensions()->at(size_before),
                                   "1.0"));
}

// Tests that installing and uninstalling extensions don't crash with an
// incognito window open.
IN_PROC_BROWSER_TEST_F(ExtensionManagementTest, Incognito) {
  // Open an incognito window to the extensions management page.  We just
  // want to make sure that we don't crash while playing with extensions when
  // this guy is around.
  ui_test_utils::OpenURLOffTheRecord(browser()->profile(),
                                     GURL(chrome::kChromeUIExtensionsURL));

  ASSERT_TRUE(InstallExtension(test_data_dir_.AppendASCII("good.crx"), 1));
  UninstallExtension("ldnnhddmnhbkjipkidpdiheffobcpfmf");
}

// Tests the process of updating an extension to one that requires higher
// permissions.
IN_PROC_BROWSER_TEST_F(ExtensionManagementTest, UpdatePermissions) {
  ExtensionService* service = browser()->profile()->GetExtensionService();
  ASSERT_TRUE(InstallAndUpdateIncreasingPermissionsExtension());
  const size_t size_before = service->extensions()->size();

  // Now try reenabling it.
  service->EnableExtension(service->disabled_extensions()->at(0)->id());
  EXPECT_EQ(size_before + 1, service->extensions()->size());
  EXPECT_EQ(0u, service->disabled_extensions()->size());
}

// Tests that we can uninstall a disabled extension.
IN_PROC_BROWSER_TEST_F(ExtensionManagementTest, UninstallDisabled) {
  ExtensionService* service = browser()->profile()->GetExtensionService();
  ASSERT_TRUE(InstallAndUpdateIncreasingPermissionsExtension());
  const size_t size_before = service->extensions()->size();

  // Now try uninstalling it.
  UninstallExtension(service->disabled_extensions()->at(0)->id());
  EXPECT_EQ(size_before, service->extensions()->size());
  EXPECT_EQ(0u, service->disabled_extensions()->size());
}

// Tests that disabling and re-enabling an extension works.
IN_PROC_BROWSER_TEST_F(ExtensionManagementTest, DisableEnable) {
  ExtensionProcessManager* manager = browser()->profile()->
      GetExtensionProcessManager();
  ExtensionService* service = browser()->profile()->GetExtensionService();
  const size_t size_before = service->extensions()->size();

  // Load an extension, expect the background page to be available.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("bjafgdebaacbbbecmhlhpofkepfkgcpa")
                    .AppendASCII("1.0")));
  ASSERT_EQ(size_before + 1, service->extensions()->size());
  EXPECT_EQ(0u, service->disabled_extensions()->size());
  const Extension* extension = service->extensions()->at(size_before);
  EXPECT_TRUE(manager->GetBackgroundHostForExtension(extension));

  // After disabling, the background page should go away.
  service->DisableExtension("bjafgdebaacbbbecmhlhpofkepfkgcpa");
  EXPECT_EQ(size_before, service->extensions()->size());
  EXPECT_EQ(1u, service->disabled_extensions()->size());
  EXPECT_FALSE(manager->GetBackgroundHostForExtension(extension));

  // And bring it back.
  service->EnableExtension("bjafgdebaacbbbecmhlhpofkepfkgcpa");
  EXPECT_EQ(size_before + 1, service->extensions()->size());
  EXPECT_EQ(0u, service->disabled_extensions()->size());
  EXPECT_TRUE(manager->GetBackgroundHostForExtension(extension));
}

// Used for testing notifications sent during extension updates.
class NotificationListener : public NotificationObserver {
 public:
  NotificationListener() : started_(false), finished_(false) {
    NotificationType::Type types[] = {
      NotificationType::EXTENSION_UPDATING_STARTED,
      NotificationType::EXTENSION_UPDATING_FINISHED,
      NotificationType::EXTENSION_UPDATE_FOUND
    };
    for (size_t i = 0; i < arraysize(types); i++) {
      registrar_.Add(this, types[i], NotificationService::AllSources());
    }
  }
  ~NotificationListener() {}

  bool started() { return started_; }

  bool finished() { return finished_; }

  const std::set<std::string>& updates() { return updates_; }

  void Reset() {
    started_ = false;
    finished_ = false;
    updates_.clear();
  }

  // Implements NotificationObserver interface.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    switch (type.value) {
      case NotificationType::EXTENSION_UPDATING_STARTED: {
        DCHECK(!started_);
        started_ = true;
        break;
      }
      case NotificationType::EXTENSION_UPDATING_FINISHED: {
        DCHECK(!finished_);
        finished_ = true;
        break;
      }
      case NotificationType::EXTENSION_UPDATE_FOUND: {
        const std::string* id = Details<const std::string>(details).ptr();
        updates_.insert(*id);
        break;
      }
      default:
        NOTREACHED();
    }
  }

 private:
  NotificationRegistrar registrar_;

  // Did we see EXTENSION_UPDATING_STARTED?
  bool started_;

  // Did we see EXTENSION_UPDATING_FINISHED?
  bool finished_;

  // The set of extension id's we've seen via EXTENSION_UPDATE_FOUND.
  std::set<std::string> updates_;
};

// Tests extension autoupdate.
IN_PROC_BROWSER_TEST_F(ExtensionManagementTest, AutoUpdate) {
  NotificationListener notification_listener;
  FilePath basedir = test_data_dir_.AppendASCII("autoupdate");
  // Note: This interceptor gets requests on the IO thread.
  scoped_refptr<AutoUpdateInterceptor> interceptor(new AutoUpdateInterceptor());
  URLFetcher::enable_interception_for_tests(true);

  interceptor->SetResponseOnIOThread("http://localhost/autoupdate/manifest",
                                     basedir.AppendASCII("manifest_v2.xml"));
  interceptor->SetResponseOnIOThread("http://localhost/autoupdate/v2.crx",
                                     basedir.AppendASCII("v2.crx"));

  // Install version 1 of the extension.
  ExtensionTestMessageListener listener1("v1 installed", false);
  ExtensionService* service = browser()->profile()->GetExtensionService();
  const size_t size_before = service->extensions()->size();
  ASSERT_TRUE(service->disabled_extensions()->empty());
  ASSERT_TRUE(InstallExtension(basedir.AppendASCII("v1.crx"), 1));
  listener1.WaitUntilSatisfied();
  const ExtensionList* extensions = service->extensions();
  ASSERT_EQ(size_before + 1, extensions->size());
  ASSERT_EQ("ogjcoiohnmldgjemafoockdghcjciccf",
            extensions->at(size_before)->id());
  ASSERT_EQ("1.0", extensions->at(size_before)->VersionString());

  // We don't want autoupdate blacklist checks.
  service->updater()->set_blacklist_checks_enabled(false);

  // Run autoupdate and make sure version 2 of the extension was installed.
  ExtensionTestMessageListener listener2("v2 installed", false);
  service->updater()->CheckNow();
  ASSERT_TRUE(WaitForExtensionInstall());
  listener2.WaitUntilSatisfied();
  extensions = service->extensions();
  ASSERT_EQ(size_before + 1, extensions->size());
  ASSERT_EQ("ogjcoiohnmldgjemafoockdghcjciccf",
            extensions->at(size_before)->id());
  ASSERT_EQ("2.0", extensions->at(size_before)->VersionString());
  ASSERT_TRUE(notification_listener.started());
  ASSERT_TRUE(notification_listener.finished());
  ASSERT_TRUE(ContainsKey(notification_listener.updates(),
                          "ogjcoiohnmldgjemafoockdghcjciccf"));
  notification_listener.Reset();

  // Now try doing an update to version 3, which has been incorrectly
  // signed. This should fail.
  interceptor->SetResponseOnIOThread("http://localhost/autoupdate/manifest",
                                     basedir.AppendASCII("manifest_v3.xml"));
  interceptor->SetResponseOnIOThread("http://localhost/autoupdate/v3.crx",
                                     basedir.AppendASCII("v3.crx"));

  service->updater()->CheckNow();
  ASSERT_TRUE(WaitForExtensionInstallError());
  ASSERT_TRUE(notification_listener.started());
  ASSERT_TRUE(notification_listener.finished());
  ASSERT_TRUE(ContainsKey(notification_listener.updates(),
                          "ogjcoiohnmldgjemafoockdghcjciccf"));

  // Make sure the extension state is the same as before.
  extensions = service->extensions();
  ASSERT_EQ(size_before + 1, extensions->size());
  ASSERT_EQ("ogjcoiohnmldgjemafoockdghcjciccf",
            extensions->at(size_before)->id());
  ASSERT_EQ("2.0", extensions->at(size_before)->VersionString());
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementTest, ExternalUrlUpdate) {
  ExtensionService* service = browser()->profile()->GetExtensionService();
  const char* kExtensionId = "ogjcoiohnmldgjemafoockdghcjciccf";
  // We don't want autoupdate blacklist checks.
  service->updater()->set_blacklist_checks_enabled(false);

  FilePath basedir = test_data_dir_.AppendASCII("autoupdate");

  // Note: This interceptor gets requests on the IO thread.
  scoped_refptr<AutoUpdateInterceptor> interceptor(new AutoUpdateInterceptor());
  URLFetcher::enable_interception_for_tests(true);

  interceptor->SetResponseOnIOThread("http://localhost/autoupdate/manifest",
                                     basedir.AppendASCII("manifest_v2.xml"));
  interceptor->SetResponseOnIOThread("http://localhost/autoupdate/v2.crx",
                                     basedir.AppendASCII("v2.crx"));

  const size_t size_before = service->extensions()->size();
  ASSERT_TRUE(service->disabled_extensions()->empty());

  PendingExtensionManager* pending_extension_manager =
      service->pending_extension_manager();

  // The code that reads external_extensions.json uses this method to inform
  // the ExtensionService of an extension to download.  Using the real code
  // is race-prone, because instantating the ExtensionService starts a read
  // of external_extensions.json before this test function starts.

  pending_extension_manager->AddFromExternalUpdateUrl(
      kExtensionId, GURL("http://localhost/autoupdate/manifest"),
      Extension::EXTERNAL_PREF_DOWNLOAD);

  // Run autoupdate and make sure version 2 of the extension was installed.
  service->updater()->CheckNow();
  ASSERT_TRUE(WaitForExtensionInstall());
  const ExtensionList* extensions = service->extensions();
  ASSERT_EQ(size_before + 1, extensions->size());
  ASSERT_EQ(kExtensionId, extensions->at(size_before)->id());
  ASSERT_EQ("2.0", extensions->at(size_before)->VersionString());

  // Uninstalling the extension should set a pref that keeps the extension from
  // being installed again the next time external_extensions.json is read.

  UninstallExtension(kExtensionId);

  ExtensionPrefs* extension_prefs = service->extension_prefs();
  EXPECT_TRUE(extension_prefs->IsExternalExtensionUninstalled(kExtensionId))
      << "Uninstalling should set kill bit on externaly installed extension.";

  // Try to install the extension again from an external source. It should fail
  // because of the killbit.
  pending_extension_manager->AddFromExternalUpdateUrl(
      kExtensionId, GURL("http://localhost/autoupdate/manifest"),
      Extension::EXTERNAL_PREF_DOWNLOAD);
  EXPECT_FALSE(pending_extension_manager->IsIdPending(kExtensionId))
      << "External reinstall of a killed extension shouldn't work.";
  EXPECT_TRUE(extension_prefs->IsExternalExtensionUninstalled(kExtensionId))
      << "External reinstall of a killed extension should leave it killed.";

  // Installing from non-external source.
  ASSERT_TRUE(InstallExtension(basedir.AppendASCII("v2.crx"), 1));

  EXPECT_FALSE(extension_prefs->IsExternalExtensionUninstalled(kExtensionId))
      << "Reinstalling should clear the kill bit.";

  // Uninstalling from a non-external source should not set the kill bit.
  UninstallExtension(kExtensionId);

  EXPECT_FALSE(extension_prefs->IsExternalExtensionUninstalled(kExtensionId))
      << "Uninstalling non-external extension should not set kill bit.";
}

// See http://crbug.com/57378 for flakiness details.
IN_PROC_BROWSER_TEST_F(ExtensionManagementTest, ExternalPolicyRefresh) {
  ExtensionService* service = browser()->profile()->GetExtensionService();
  const char* kExtensionId = "ogjcoiohnmldgjemafoockdghcjciccf";
  // We don't want autoupdate blacklist checks.
  service->updater()->set_blacklist_checks_enabled(false);

  FilePath basedir = test_data_dir_.AppendASCII("autoupdate");

  // Note: This interceptor gets requests on the IO thread.
  scoped_refptr<AutoUpdateInterceptor> interceptor(new AutoUpdateInterceptor());
  URLFetcher::enable_interception_for_tests(true);

  interceptor->SetResponseOnIOThread("http://localhost/autoupdate/manifest",
                                     basedir.AppendASCII("manifest_v2.xml"));
  interceptor->SetResponseOnIOThread("http://localhost/autoupdate/v2.crx",
                                     basedir.AppendASCII("v2.crx"));

  const size_t size_before = service->extensions()->size();
  ASSERT_TRUE(service->disabled_extensions()->empty());

  PrefService* prefs = browser()->profile()->GetPrefs();
  {
    // Set the policy as a user preference and fire notification observers.
    ListPrefUpdate pref_update(prefs, prefs::kExtensionInstallForceList);
    ListValue* forcelist = pref_update.Get();
    ASSERT_TRUE(forcelist->empty());
    forcelist->Append(Value::CreateStringValue(
        std::string(kExtensionId) +
        ";http://localhost/autoupdate/manifest"));
  }

  // Check if the extension got installed.
  ASSERT_TRUE(WaitForExtensionInstall());
  const ExtensionList* extensions = service->extensions();
  ASSERT_EQ(size_before + 1, extensions->size());
  ASSERT_EQ(kExtensionId, extensions->at(size_before)->id());
  EXPECT_EQ("2.0", extensions->at(size_before)->VersionString());
  EXPECT_EQ(Extension::EXTERNAL_POLICY_DOWNLOAD,
            extensions->at(size_before)->location());

  // Try to disable and unstall the extension which should fail.
  service->DisableExtension(kExtensionId);
  EXPECT_EQ(size_before + 1, service->extensions()->size());
  EXPECT_EQ(0u, service->disabled_extensions()->size());
  UninstallExtension(kExtensionId);
  EXPECT_EQ(size_before + 1, service->extensions()->size());
  EXPECT_EQ(0u, service->disabled_extensions()->size());

  // Now try to disable it through the management api.
  ExtensionTestMessageListener listener1("ready", false);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/uninstall_extension")));
  ASSERT_TRUE(listener1.WaitUntilSatisfied());
  EXPECT_EQ(size_before + 2, service->extensions()->size());
  EXPECT_EQ(0u, service->disabled_extensions()->size());

  // Check that emptying the list triggers uninstall.
  {
    prefs->ClearPref(prefs::kExtensionInstallForceList);
  }
  EXPECT_EQ(size_before + 1, extensions->size());
  ExtensionList::const_iterator i;
  for (i = extensions->begin(); i != extensions->end(); ++i)
    EXPECT_NE(kExtensionId, (*i)->id());
}
