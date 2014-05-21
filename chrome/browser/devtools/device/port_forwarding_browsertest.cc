// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/devtools/browser_list_tabcontents_provider.h"
#include "chrome/browser/devtools/device/port_forwarding_controller.h"
#include "chrome/browser/devtools/device/self_device_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"

namespace {
const char kPortForwardingTestPage[] =
    "files/devtools/port_forwarding/main.html";

const int kDefaultDebuggingPort = 9223;
}

class PortForwardingTest: public InProcessBrowserTest {
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kRemoteDebuggingPort,
        base::IntToString(kDefaultDebuggingPort));
  }

 protected:
  class Listener : public PortForwardingController::Listener {
   public:
    explicit Listener(Profile* profile)
        : profile_(profile) {
      PortForwardingController::Factory::GetForProfile(profile_)->
          AddListener(this);
    }

    virtual ~Listener() {
      PortForwardingController::Factory::GetForProfile(profile_)->
          RemoveListener(this);
    }

    virtual void PortStatusChanged(const DevicesStatus& status) OVERRIDE {
      if (status.empty())
        return;
      base::MessageLoop::current()->PostTask(
          FROM_HERE, base::MessageLoop::QuitClosure());
    }

   private:
    Profile* profile_;
  };
};

IN_PROC_BROWSER_TEST_F(PortForwardingTest,
                       LoadPageWithStyleAnsScript) {
  Profile* profile = browser()->profile();

  AndroidDeviceManager::DeviceProviders device_providers;
  device_providers.push_back(new SelfAsDeviceProvider(kDefaultDebuggingPort));
  DevToolsAndroidBridge::Factory::GetForProfile(profile)->
      set_device_providers_for_test(device_providers);

  ASSERT_TRUE(test_server()->Start());
  GURL original_url = test_server()->GetURL(kPortForwardingTestPage);

  std::string forwarding_port("8000");
  GURL forwarding_url(original_url.scheme() + "://" +
      original_url.host() + ":" + forwarding_port + original_url.path());

  PrefService* prefs = profile->GetPrefs();
  prefs->SetBoolean(prefs::kDevToolsPortForwardingEnabled, true);

  base::DictionaryValue config;
  config.SetString(
      forwarding_port, original_url.host() + ":" + original_url.port());
  prefs->Set(prefs::kDevToolsPortForwardingConfig, config);

  Listener wait_for_port_forwarding(profile);
  content::RunMessageLoop();

  BrowserListTabContentsProvider::EnableTethering();

  ui_test_utils::NavigateToURL(browser(), forwarding_url);

  content::RenderViewHost* rvh = browser()->tab_strip_model()->
      GetWebContentsAt(0)->GetRenderViewHost();

  std::string result;
  ASSERT_TRUE(
      content::ExecuteScriptAndExtractString(
          rvh,
          "window.domAutomationController.send(document.title)",
          &result));
  ASSERT_EQ("Port forwarding test", result) << "Document has not loaded.";

  ASSERT_TRUE(
      content::ExecuteScriptAndExtractString(
          rvh,
          "window.domAutomationController.send(getBodyTextContent())",
          &result));
  ASSERT_EQ("content", result) << "Javascript has not loaded.";

  ASSERT_TRUE(
      content::ExecuteScriptAndExtractString(
          rvh,
          "window.domAutomationController.send(getBodyMarginLeft())",
          &result));
  ASSERT_EQ("100px", result) << "CSS has not loaded.";
}
