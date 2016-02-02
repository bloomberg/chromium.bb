// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/network_change_notifier.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using chrome_browser_net::NetworkPredictionOptions;
using net::NetworkChangeNotifier;

namespace {

const char kPrefetchPage[] = "/prerender/simple_prefetch.html";

class MockNetworkChangeNotifierWIFI : public NetworkChangeNotifier {
 public:
  ConnectionType GetCurrentConnectionType() const override {
    return NetworkChangeNotifier::CONNECTION_WIFI;
  }
};

class MockNetworkChangeNotifier4G : public NetworkChangeNotifier {
 public:
  ConnectionType GetCurrentConnectionType() const override {
    return NetworkChangeNotifier::CONNECTION_4G;
  }
};

class PrefetchBrowserTestBase : public InProcessBrowserTest {
 public:
  explicit PrefetchBrowserTestBase(bool disabled_via_field_trial)
      : disabled_via_field_trial_(disabled_via_field_trial) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (disabled_via_field_trial_) {
      command_line->AppendSwitchASCII(switches::kForceFieldTrials,
                                      "Prefetch/ExperimentDisabled/");
    }
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void SetPreference(NetworkPredictionOptions value) {
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kNetworkPredictionOptions, value);
  }

  bool RunPrefetchExperiment(bool expect_success, Browser* browser) {
    GURL url = embedded_test_server()->GetURL(kPrefetchPage);

    const base::string16 expected_title =
        expect_success ? base::ASCIIToUTF16("link onload")
                       : base::ASCIIToUTF16("link onerror");
    content::TitleWatcher title_watcher(
        browser->tab_strip_model()->GetActiveWebContents(), expected_title);
    ui_test_utils::NavigateToURL(browser, url);
    return expected_title == title_watcher.WaitAndGetTitle();
  }

 private:
  bool disabled_via_field_trial_;
};

class PrefetchBrowserTestPrediction : public PrefetchBrowserTestBase {
 public:
  PrefetchBrowserTestPrediction() : PrefetchBrowserTestBase(false) {}
};

class PrefetchBrowserTestPredictionDisabled : public PrefetchBrowserTestBase {
 public:
  PrefetchBrowserTestPredictionDisabled() : PrefetchBrowserTestBase(true) {}
};

// Prefetch is disabled via field experiment.  Prefetch should be dropped.
IN_PROC_BROWSER_TEST_F(PrefetchBrowserTestPredictionDisabled,
                       ExperimentDisabled) {
  EXPECT_TRUE(RunPrefetchExperiment(false, browser()));
  // Should not prefetch even if preference is ALWAYS.
  SetPreference(NetworkPredictionOptions::NETWORK_PREDICTION_ALWAYS);
  EXPECT_TRUE(RunPrefetchExperiment(false, browser()));
}

// Prefetch should be allowed depending on preference and network type.
IN_PROC_BROWSER_TEST_F(PrefetchBrowserTestPrediction, PreferenceWorks) {
  // Set real NetworkChangeNotifier singleton aside.
  scoped_ptr<NetworkChangeNotifier::DisableForTest> disable_for_test(
      new NetworkChangeNotifier::DisableForTest);

  // Preference defaults to WIFI_ONLY: prefetch when not on cellular.
  {
    scoped_ptr<NetworkChangeNotifier> mock(new MockNetworkChangeNotifierWIFI);
    EXPECT_TRUE(RunPrefetchExperiment(true, browser()));
  }
  {
    scoped_ptr<NetworkChangeNotifier> mock(new MockNetworkChangeNotifier4G);
    EXPECT_TRUE(RunPrefetchExperiment(false, browser()));
  }

  // Set preference to ALWAYS: always prefetch.
  SetPreference(NetworkPredictionOptions::NETWORK_PREDICTION_ALWAYS);
  {
    scoped_ptr<NetworkChangeNotifier> mock(new MockNetworkChangeNotifierWIFI);
    EXPECT_TRUE(RunPrefetchExperiment(true, browser()));
  }
  {
    scoped_ptr<NetworkChangeNotifier> mock(new MockNetworkChangeNotifier4G);
    EXPECT_TRUE(RunPrefetchExperiment(true, browser()));
  }

  // Set preference to NEVER: never prefetch.
  SetPreference(NetworkPredictionOptions::NETWORK_PREDICTION_NEVER);
  {
    scoped_ptr<NetworkChangeNotifier> mock(new MockNetworkChangeNotifierWIFI);
    EXPECT_TRUE(RunPrefetchExperiment(false, browser()));
  }
  {
    scoped_ptr<NetworkChangeNotifier> mock(new MockNetworkChangeNotifier4G);
    EXPECT_TRUE(RunPrefetchExperiment(false, browser()));
  }
}

// Bug 339909: When in incognito mode the browser crashed due to an
// uninitialized preference member. Verify that it no longer does.
IN_PROC_BROWSER_TEST_F(PrefetchBrowserTestPrediction, IncognitoTest) {
  Profile* incognito_profile = browser()->profile()->GetOffTheRecordProfile();
  Browser* incognito_browser = new Browser(
      Browser::CreateParams(incognito_profile, browser()->host_desktop_type()));

  // Navigate just to have a tab in this window, otherwise there is no
  // WebContents for the incognito browser.
  OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));

  EXPECT_TRUE(RunPrefetchExperiment(true, incognito_browser));
}

}  // namespace
