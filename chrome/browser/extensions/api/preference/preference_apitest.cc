// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/webrtc_ip_handling_policy.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "media/media_features.h"

class ExtensionPreferenceApiTest : public ExtensionApiTest {
 protected:
  ExtensionPreferenceApiTest() : profile_(NULL) {}

  void CheckPreferencesSet() {
    PrefService* prefs = profile_->GetPrefs();
    const PrefService::Preference* pref = prefs->FindPreference(
        prefs::kBlockThirdPartyCookies);
    ASSERT_TRUE(pref);
    EXPECT_TRUE(pref->IsExtensionControlled());
    EXPECT_TRUE(prefs->GetBoolean(prefs::kAlternateErrorPagesEnabled));
    EXPECT_TRUE(prefs->GetBoolean(autofill::prefs::kAutofillEnabled));
    EXPECT_FALSE(prefs->GetBoolean(prefs::kBlockThirdPartyCookies));
    EXPECT_TRUE(prefs->GetBoolean(prefs::kEnableHyperlinkAuditing));
    EXPECT_TRUE(prefs->GetBoolean(prefs::kEnableReferrers));
    EXPECT_TRUE(prefs->GetBoolean(prefs::kOfferTranslateEnabled));
    EXPECT_EQ(chrome_browser_net::NETWORK_PREDICTION_DEFAULT,
              prefs->GetInteger(prefs::kNetworkPredictionOptions));
    EXPECT_TRUE(
        prefs->GetBoolean(password_manager::prefs::kCredentialsEnableService));
    EXPECT_TRUE(prefs->GetBoolean(prefs::kSafeBrowsingEnabled));
    EXPECT_TRUE(prefs->GetBoolean(prefs::kSearchSuggestEnabled));
  }

  void CheckPreferencesCleared() {
    PrefService* prefs = profile_->GetPrefs();
    const PrefService::Preference* pref = prefs->FindPreference(
        prefs::kBlockThirdPartyCookies);
    ASSERT_TRUE(pref);
    EXPECT_FALSE(pref->IsExtensionControlled());
    EXPECT_FALSE(prefs->GetBoolean(prefs::kAlternateErrorPagesEnabled));
    EXPECT_FALSE(prefs->GetBoolean(autofill::prefs::kAutofillEnabled));
    EXPECT_TRUE(prefs->GetBoolean(prefs::kBlockThirdPartyCookies));
    EXPECT_FALSE(prefs->GetBoolean(prefs::kEnableHyperlinkAuditing));
    EXPECT_FALSE(prefs->GetBoolean(prefs::kEnableReferrers));
    EXPECT_FALSE(prefs->GetBoolean(prefs::kOfferTranslateEnabled));
    EXPECT_EQ(chrome_browser_net::NETWORK_PREDICTION_NEVER,
              prefs->GetInteger(prefs::kNetworkPredictionOptions));
    EXPECT_FALSE(
        prefs->GetBoolean(password_manager::prefs::kCredentialsEnableService));
    EXPECT_FALSE(prefs->GetBoolean(prefs::kSafeBrowsingEnabled));
    EXPECT_FALSE(prefs->GetBoolean(prefs::kSearchSuggestEnabled));
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

    // The browser might get closed later (and therefore be destroyed), so we
    // save the profile.
    profile_ = browser()->profile();

    // Closing the last browser window also releases a module reference. Make
    // sure it's not the last one, so the message loop doesn't quit
    // unexpectedly.
    keep_alive_.reset(new ScopedKeepAlive(KeepAliveOrigin::BROWSER,
                                          KeepAliveRestartOption::DISABLED));
  }

  void TearDownOnMainThread() override {
    // BrowserProcess::Shutdown() needs to be called in a message loop, so we
    // post a task to release the keep alive, then run the message loop.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&std::unique_ptr<ScopedKeepAlive>::reset,
                                  base::Unretained(&keep_alive_), nullptr));
    content::RunAllPendingInMessageLoop();

    ExtensionApiTest::TearDownOnMainThread();
  }

  Profile* profile_;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
};

// http://crbug.com/177163
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_Standard DISABLED_Standard
#else
#define MAYBE_Standard Standard
#endif
IN_PROC_BROWSER_TEST_F(ExtensionPreferenceApiTest, MAYBE_Standard) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(prefs::kAlternateErrorPagesEnabled, false);
  prefs->SetBoolean(autofill::prefs::kAutofillEnabled, false);
  prefs->SetBoolean(prefs::kBlockThirdPartyCookies, true);
  prefs->SetBoolean(prefs::kEnableHyperlinkAuditing, false);
  prefs->SetBoolean(prefs::kEnableReferrers, false);
  prefs->SetBoolean(prefs::kOfferTranslateEnabled, false);
  prefs->SetInteger(prefs::kNetworkPredictionOptions,
                    chrome_browser_net::NETWORK_PREDICTION_NEVER);
  prefs->SetBoolean(password_manager::prefs::kCredentialsEnableService, false);
  prefs->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  prefs->SetBoolean(prefs::kSearchSuggestEnabled, false);
#if BUILDFLAG(ENABLE_WEBRTC)
  prefs->SetBoolean(prefs::kWebRTCMultipleRoutesEnabled, false);
  prefs->SetBoolean(prefs::kWebRTCNonProxiedUdpEnabled, false);
  prefs->SetString(prefs::kWebRTCIPHandlingPolicy,
                   content::kWebRTCIPHandlingDefaultPublicInterfaceOnly);
#endif

  const char kExtensionPath[] = "preference/standard";

  EXPECT_TRUE(RunExtensionSubtest(kExtensionPath, "test.html")) << message_;
  CheckPreferencesSet();

  // The settings should not be reset when the extension is reloaded.
  ReloadExtension(last_loaded_extension_id());
  CheckPreferencesSet();

  // Uninstalling and installing the extension (without running the test that
  // calls the extension API) should clear the settings.
  extensions::TestExtensionRegistryObserver observer(
      extensions::ExtensionRegistry::Get(profile_), last_loaded_extension_id());
  UninstallExtension(last_loaded_extension_id());
  observer.WaitForExtensionUninstalled();
  CheckPreferencesCleared();

  LoadExtension(test_data_dir_.AppendASCII(kExtensionPath));
  CheckPreferencesCleared();
}

IN_PROC_BROWSER_TEST_F(ExtensionPreferenceApiTest, PersistentIncognito) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(prefs::kBlockThirdPartyCookies, false);

  EXPECT_TRUE(
      RunExtensionTestIncognito("preference/persistent_incognito")) <<
      message_;

  // Setting an incognito preference should not create an incognito profile.
  EXPECT_FALSE(profile_->HasOffTheRecordProfile());

  PrefService* otr_prefs = profile_->GetOffTheRecordProfile()->GetPrefs();
  const PrefService::Preference* pref =
      otr_prefs->FindPreference(prefs::kBlockThirdPartyCookies);
  ASSERT_TRUE(pref);
  EXPECT_TRUE(pref->IsExtensionControlled());
  EXPECT_TRUE(otr_prefs->GetBoolean(prefs::kBlockThirdPartyCookies));

  pref = prefs->FindPreference(prefs::kBlockThirdPartyCookies);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->IsExtensionControlled());
  EXPECT_FALSE(prefs->GetBoolean(prefs::kBlockThirdPartyCookies));
}

// Flakily times out: http://crbug.com/106144
IN_PROC_BROWSER_TEST_F(ExtensionPreferenceApiTest, DISABLED_IncognitoDisabled) {
  EXPECT_FALSE(RunExtensionTest("preference/persistent_incognito"));
}

IN_PROC_BROWSER_TEST_F(ExtensionPreferenceApiTest, SessionOnlyIncognito) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(prefs::kBlockThirdPartyCookies, false);

  EXPECT_TRUE(
      RunExtensionTestIncognito("preference/session_only_incognito")) <<
      message_;

  EXPECT_TRUE(profile_->HasOffTheRecordProfile());

  PrefService* otr_prefs = profile_->GetOffTheRecordProfile()->GetPrefs();
  const PrefService::Preference* pref =
      otr_prefs->FindPreference(prefs::kBlockThirdPartyCookies);
  ASSERT_TRUE(pref);
  EXPECT_TRUE(pref->IsExtensionControlled());
  EXPECT_FALSE(otr_prefs->GetBoolean(prefs::kBlockThirdPartyCookies));

  pref = prefs->FindPreference(prefs::kBlockThirdPartyCookies);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->IsExtensionControlled());
  EXPECT_FALSE(prefs->GetBoolean(prefs::kBlockThirdPartyCookies));
}

IN_PROC_BROWSER_TEST_F(ExtensionPreferenceApiTest, Clear) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(prefs::kBlockThirdPartyCookies, true);

  EXPECT_TRUE(RunExtensionTest("preference/clear")) << message_;

  const PrefService::Preference* pref = prefs->FindPreference(
      prefs::kBlockThirdPartyCookies);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->IsExtensionControlled());
  EXPECT_EQ(true, prefs->GetBoolean(prefs::kBlockThirdPartyCookies));
}

IN_PROC_BROWSER_TEST_F(ExtensionPreferenceApiTest, OnChange) {
  EXPECT_TRUE(RunExtensionTestIncognito("preference/onchange")) <<
      message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionPreferenceApiTest, OnChangeSplit) {
  extensions::ResultCatcher catcher;
  catcher.RestrictToBrowserContext(profile_);
  extensions::ResultCatcher catcher_incognito;
  catcher_incognito.RestrictToBrowserContext(
      profile_->GetOffTheRecordProfile());

  // Open an incognito window.
  OpenURLOffTheRecord(profile_, GURL("chrome://newtab/"));

  // changeDefault listeners.
  ExtensionTestMessageListener listener1("changeDefault regular ready", true);
  ExtensionTestMessageListener listener_incognito1(
      "changeDefault incognito ready", true);

  // changeIncognitoOnly listeners.
  ExtensionTestMessageListener listener2(
      "changeIncognitoOnly regular ready", true);
  ExtensionTestMessageListener listener_incognito2(
      "changeIncognitoOnly incognito ready", true);
  ExtensionTestMessageListener listener3(
      "changeIncognitoOnly regular listening", true);
  ExtensionTestMessageListener listener_incognito3(
      "changeIncognitoOnly incognito pref set", false);

  // changeDefaultOnly listeners.
  ExtensionTestMessageListener listener4(
      "changeDefaultOnly regular ready", true);
  ExtensionTestMessageListener listener_incognito4(
      "changeDefaultOnly incognito ready", true);
  ExtensionTestMessageListener listener5(
      "changeDefaultOnly regular pref set", false);
  ExtensionTestMessageListener listener_incognito5(
      "changeDefaultOnly incognito listening", true);

  // changeIncognitoOnlyBack listeners.
  ExtensionTestMessageListener listener6(
      "changeIncognitoOnlyBack regular ready", true);
  ExtensionTestMessageListener listener_incognito6(
      "changeIncognitoOnlyBack incognito ready", true);
  ExtensionTestMessageListener listener7(
      "changeIncognitoOnlyBack regular listening", true);
  ExtensionTestMessageListener listener_incognito7(
      "changeIncognitoOnlyBack incognito pref set", false);

  // clearIncognito listeners.
  ExtensionTestMessageListener listener8(
      "clearIncognito regular ready", true);
  ExtensionTestMessageListener listener_incognito8(
      "clearIncognito incognito ready", true);
  ExtensionTestMessageListener listener9(
      "clearIncognito regular listening", true);
  ExtensionTestMessageListener listener_incognito9(
      "clearIncognito incognito pref cleared", false);

  // clearDefault listeners.
  ExtensionTestMessageListener listener10(
      "clearDefault regular ready", true);
  ExtensionTestMessageListener listener_incognito10(
      "clearDefault incognito ready", true);

  base::FilePath extension_data_dir =
      test_data_dir_.AppendASCII("preference").AppendASCII("onchange_split");
  ASSERT_TRUE(LoadExtensionIncognito(extension_data_dir));

  // Test 1 - changeDefault
  EXPECT_TRUE(listener1.WaitUntilSatisfied()); // Regular ready
  EXPECT_TRUE(listener_incognito1.WaitUntilSatisfied()); // Incognito ready
  listener1.Reply("ok");
  listener_incognito1.Reply("ok");

  // Test 2 - changeIncognitoOnly
  EXPECT_TRUE(listener2.WaitUntilSatisfied()); // Regular ready
  EXPECT_TRUE(listener_incognito2.WaitUntilSatisfied()); // Incognito ready
  EXPECT_TRUE(listener3.WaitUntilSatisfied()); // Regular listening
  listener2.Reply("ok");
  listener_incognito2.Reply("ok");
  // Incognito preference set -- notify the regular listener
  EXPECT_TRUE(listener_incognito3.WaitUntilSatisfied());
  listener3.Reply("ok");

  // Test 3 - changeDefaultOnly
  EXPECT_TRUE(listener4.WaitUntilSatisfied()); // Regular ready
  EXPECT_TRUE(listener_incognito4.WaitUntilSatisfied()); // Incognito ready
  EXPECT_TRUE(listener_incognito5.WaitUntilSatisfied()); // Incognito listening
  listener4.Reply("ok");
  listener_incognito4.Reply("ok");
  // Regular preference set - notify the incognito listener
  EXPECT_TRUE(listener5.WaitUntilSatisfied());
  listener_incognito5.Reply("ok");

  // Test 4 - changeIncognitoOnlyBack
  EXPECT_TRUE(listener6.WaitUntilSatisfied()); // Regular ready
  EXPECT_TRUE(listener_incognito6.WaitUntilSatisfied()); // Incognito ready
  EXPECT_TRUE(listener7.WaitUntilSatisfied()); // Regular listening
  listener6.Reply("ok");
  listener_incognito6.Reply("ok");
  // Incognito preference set -- notify the regular listener
  EXPECT_TRUE(listener_incognito7.WaitUntilSatisfied());
  listener7.Reply("ok");

  // Test 5 - clearIncognito
  EXPECT_TRUE(listener8.WaitUntilSatisfied()); // Regular ready
  EXPECT_TRUE(listener_incognito8.WaitUntilSatisfied()); // Incognito ready
  EXPECT_TRUE(listener9.WaitUntilSatisfied()); // Regular listening
  listener8.Reply("ok");
  listener_incognito8.Reply("ok");
  // Incognito preference cleared -- notify the regular listener
  EXPECT_TRUE(listener_incognito9.WaitUntilSatisfied());
  listener9.Reply("ok");

  // Test 6 - clearDefault
  EXPECT_TRUE(listener10.WaitUntilSatisfied()); // Regular ready
  EXPECT_TRUE(listener_incognito10.WaitUntilSatisfied()); // Incognito ready
  listener10.Reply("ok");
  listener_incognito10.Reply("ok");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ExtensionPreferenceApiTest, DataReductionProxy) {
  EXPECT_TRUE(RunExtensionTest("preference/data_reduction_proxy")) <<
      message_;
}
