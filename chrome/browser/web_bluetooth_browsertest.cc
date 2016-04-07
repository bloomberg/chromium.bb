// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains browsertests for Web Bluetooth that depend on behavior
// defined in chrome/, not just in content/.

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/permissions/permission_context_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"

namespace {

class WebBluetoothTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // This is needed while Web Bluetooth is an Origin Trial, but can go away
    // once it ships globally.
    command_line->AppendSwitch(switches::kEnableWebBluetooth);
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    // Navigate to a secure context.
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
    ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL("localhost", "/simple_page.html"));
    web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_THAT(
        web_contents_->GetMainFrame()->GetLastCommittedOrigin().Serialize(),
        testing::StartsWith("http://localhost:"));
  }

  content::WebContents* web_contents_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(WebBluetoothTest, KillSwitchShouldBlock) {
  // Fake the BluetoothAdapter to say it's present.
  scoped_refptr<device::MockBluetoothAdapter> adapter =
      new testing::NiceMock<device::MockBluetoothAdapter>;
  EXPECT_CALL(*adapter, IsPresent()).WillRepeatedly(testing::Return(true));
  device::BluetoothAdapterFactory::SetAdapterForTesting(adapter);

  // Turn on the global kill switch.
  std::map<std::string, std::string> params;
  params["Bluetooth"] =
      PermissionContextBase::kPermissionsKillSwitchBlockedValue;
  variations::AssociateVariationParams(
      PermissionContextBase::kPermissionsKillSwitchFieldStudy, "TestGroup",
      params);
  base::FieldTrialList::CreateFieldTrial(
      PermissionContextBase::kPermissionsKillSwitchFieldStudy, "TestGroup");

  std::string rejection;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents_,
      "navigator.bluetooth.requestDevice({filters: [{name: 'Hello'}]})"
      "  .then(() => { domAutomationController.send('Success'); },"
      "        reason => {"
      "      domAutomationController.send(reason.name + ': ' + reason.message);"
      "  });",
      &rejection));
  EXPECT_THAT(rejection,
              testing::MatchesRegex("NotFoundError: .*globally disabled.*"));
}

// Tests that using Finch field trial parameters for blacklist additions has
// the effect of rejecting requestDevice calls.
IN_PROC_BROWSER_TEST_F(WebBluetoothTest, BlacklistShouldBlock) {
  // Fake the BluetoothAdapter to say it's present.
  scoped_refptr<device::MockBluetoothAdapter> adapter =
      new testing::NiceMock<device::MockBluetoothAdapter>;
  EXPECT_CALL(*adapter, IsPresent()).WillRepeatedly(testing::Return(true));
  device::BluetoothAdapterFactory::SetAdapterForTesting(adapter);

  std::map<std::string, std::string> params;
  params["blacklist_additions"] = "ee01:e";
  variations::AssociateVariationParams("WebBluetoothBlacklist", "TestGroup",
                                       params);
  base::FieldTrialList::CreateFieldTrial("WebBluetoothBlacklist", "TestGroup");

  std::string rejection;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents_,
      "navigator.bluetooth.requestDevice({filters: [{services: [0xee01]}]})"
      "  .then(() => { domAutomationController.send('Success'); },"
      "        reason => {"
      "      domAutomationController.send(reason.name + ': ' + reason.message);"
      "  });",
      &rejection));
  EXPECT_THAT(rejection,
              testing::MatchesRegex("SecurityError: .*blacklisted UUID.*"));
}

}  // namespace
