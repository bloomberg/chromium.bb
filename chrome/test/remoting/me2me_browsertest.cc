// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "chrome/test/remoting/remote_desktop_browsertest.h"
#include "chrome/test/remoting/waiter.h"

namespace remoting {

class Me2MeBrowserTest : public RemoteDesktopBrowserTest {
 protected:
  void TestKeyboardInput();
  void TestMouseInput();

  void ConnectPinlessAndCleanupPairings(bool cleanup_all);
  bool IsPairingSpinnerHidden();
};

IN_PROC_BROWSER_TEST_F(Me2MeBrowserTest,
                       MANUAL_Me2Me_Connect_Local_Host) {
  SetUpTestForMe2Me();

  ConnectToLocalHost(false);

  // TODO(chaitali): Change the mouse input test to also work in the
  // HTTP server framework
  // TestMouseInput();

  DisconnectMe2Me();
  Cleanup();
}

IN_PROC_BROWSER_TEST_F(Me2MeBrowserTest,
                       MANUAL_Me2Me_Connect_Remote_Host) {
  VerifyInternetAccess();
  Install();
  LaunchChromotingApp();

  // Authorize, Authenticate, and Approve.
  Auth();
  ExpandMe2Me();

  ConnectToRemoteHost(remote_host_name(), false);

  // TODO(weitaosu): Find a way to verify keyboard input injection.
  // We cannot use TestKeyboardInput because it assumes
  // that the client and the host are on the same machine.

  DisconnectMe2Me();
  Cleanup();
}

IN_PROC_BROWSER_TEST_F(Me2MeBrowserTest,
                       MANUAL_Me2Me_Connect_Pinless) {
  SetUpTestForMe2Me();

  ASSERT_FALSE(HtmlElementVisible("paired-client-manager-message"))
      << "The host must have no pairings before running the pinless test.";

  // Test that cleanup works with either the Delete or Delete all buttons.
  ConnectPinlessAndCleanupPairings(false);
  ConnectPinlessAndCleanupPairings(true);

  Cleanup();
}

void Me2MeBrowserTest::TestKeyboardInput() {
  // We will assume here that the browser window is already open on the host
  // and in focus.
  // Press tab to put focus on the textbox.
  SimulateKeyPressWithCode(ui::VKEY_TAB, "Tab", false, false, false, false);

  // Write some text in the box and press enter
  std::string text = "Abigail";
  SimulateStringInput(text);
  SimulateKeyPressWithCode(
      ui::VKEY_RETURN, "Enter", false, false, false, false);

  // Wait until the client tab sets the right variables
  ConditionalTimeoutWaiter waiter(
          base::TimeDelta::FromSeconds(10),
          base::TimeDelta::FromMilliseconds(500),
          base::Bind(&RemoteDesktopBrowserTest::IsHostActionComplete,
                     client_web_content(),
                     "testResult.keypressSucceeded"));
  EXPECT_TRUE(waiter.Wait());

  // Check that the text we got is correct
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      client_web_content(),
      "testResult.keypressText == '" + text + "'"));
}

void Me2MeBrowserTest::TestMouseInput() {
  SimulateMouseLeftClickAt(10, 50);
  // TODO: Verify programatically the mouse events are received by the host.
  // This will be tricky as it depends on the host OS, window manager, desktop
  // layout, and screen resolution. Until then we need to visually verify that
  // "Dash Home" is clicked on a Unity window manager.
  ASSERT_TRUE(TimeoutWaiter(base::TimeDelta::FromSeconds(5)).Wait());
}

void Me2MeBrowserTest::ConnectPinlessAndCleanupPairings(bool cleanup_all) {
  // First connection: verify that a PIN is requested, and request pairing.
  ConnectToLocalHost(true);
  DisconnectMe2Me();

  // TODO(jamiewalch): This reload is only needed because there's a bug in the
  // web-app whereby it doesn't refresh its pairing state correctly.
  // http://crbug.com/311290
  LaunchChromotingApp();
  ASSERT_TRUE(HtmlElementVisible("paired-client-manager-message"));

  // Second connection: verify that no PIN is requested.
  ClickOnControl("this-host-connect");
  WaitForConnection();
  DisconnectMe2Me();

  // Clean up pairings.
  ClickOnControl("open-paired-client-manager-dialog");
  ASSERT_TRUE(HtmlElementVisible("paired-client-manager-dialog"));

  if (cleanup_all) {
    ClickOnControl("delete-all-paired-clients");
  } else {
    std::string host_id = ExecuteScriptAndExtractString(
        "remoting.pairedClientManager.getFirstClientIdForTesting_()");
    std::string node_id = "delete-client-" + host_id;
    ClickOnControl(node_id);
  }

  // Wait for the "working" spinner to disappear. The spinner is shown by both
  // methods of deleting a host and is removed when the operation completes.
  ConditionalTimeoutWaiter waiter(
      base::TimeDelta::FromSeconds(5),
      base::TimeDelta::FromMilliseconds(200),
      base::Bind(&Me2MeBrowserTest::IsPairingSpinnerHidden, this));
  EXPECT_TRUE(waiter.Wait());
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      "document.getElementById('delete-all-paired-clients').disabled"));

  ClickOnControl("close-paired-client-manager-dialog");
  ASSERT_FALSE(HtmlElementVisible("paired-client-manager-dialog"));
  ASSERT_FALSE(HtmlElementVisible("paired-client-manager-message"));
}

bool Me2MeBrowserTest::IsPairingSpinnerHidden() {
  return !HtmlElementVisible("paired-client-manager-dialog-working");
}

}  // namespace remoting
