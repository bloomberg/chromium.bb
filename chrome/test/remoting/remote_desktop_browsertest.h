// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_REMOTING_REMOTE_DESKTOP_BROWSERTEST_H_
#define CHROME_TEST_REMOTING_REMOTE_DESKTOP_BROWSERTEST_H_

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

namespace {
// Command line arguments specific to the chromoting browser tests.
const char kOverrideUserDataDir[] = "override-user-data-dir";
const char kNoCleanup[] = "no-cleanup";
const char kNoInstall[] = "no-install";
const char kWebAppCrx[] = "webapp-crx";
const char kUsername[] = "username";
const char kkPassword[] = "password";
const char kMe2MePin[] = "me2me-pin";
const char kRemoteHostName[] = "remote-host-name";

// ASSERT_TRUE can only be used in void returning functions. This version
// should be used in non-void-returning functions.
inline void _ASSERT_TRUE(bool condition) {
  ASSERT_TRUE(condition);
  return;
}

}  // namespace

namespace remoting {

class RemoteDesktopBrowserTest : public ExtensionBrowserTest {
 public:
  RemoteDesktopBrowserTest();
  virtual ~RemoteDesktopBrowserTest();

  // InProcessBrowserTest Overrides
  virtual void SetUp() OVERRIDE;
  virtual void SetUpOnMainThread() OVERRIDE;

 protected:
  // InProcessBrowserTest Overrides
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;

  // InProcessBrowserTest Overrides
  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE;

  // The following helpers each perform a simple task.

  // Verify the test has access to the internet (specifically google.com)
  void VerifyInternetAccess();

  // Install the chromoting extension from a crx file.
  void InstallChromotingApp();

  // Uninstall the chromoting extension.
  void UninstallChromotingApp();

  // Test whether the chromoting extension is installed.
  void VerifyChromotingLoaded(bool expected);

  // Launch the chromoting app.
  void LaunchChromotingApp();

  // Authorize: grant extended access permission to the user's computer.
  void Authorize();

  // Authenticate: sign in to google using the credentials provided.
  void Authenticate();

  // Approve: grant the chromoting app necessary permissions.
  void Approve();

  // Click on "Get Started" in the Me2Me section and show the host list.
  void StartMe2Me();

  // Disconnect the active Me2Me session.
  void DisconnectMe2Me();

  // Simulate a key event.
  void SimulateKeyPressWithCode(ui::KeyboardCode keyCode, const char* code);

  void SimulateKeyPressWithCode(ui::KeyboardCode keyCode,
                                const char* code,
                                bool control,
                                bool shift,
                                bool alt,
                                bool command);

  // Simulate typing a character
  void SimulateCharInput(char c);

  // Simulate typing a string
  void SimulateStringInput(const std::string& input);

  // Helper to simulate a left button mouse click.
  void SimulateMouseLeftClickAt(int x, int y);

  // Helper to simulate a mouse click.
  void SimulateMouseClickAt(
      int modifiers, WebKit::WebMouseEvent::Button button, int x, int y);

  // The following helpers each perform a composite task.

  // Install the chromoting extension
  void Install();

  // Clean up after the test.
  void Cleanup();

  // Perform all the auth steps: authorization, authenticattion, etc.
  // It starts from the chromoting main page unauthenticated and ends up back
  // on the chromoting main page authenticated and ready to go.
  void Auth();

  // Connect to the local host through Me2Me.
  void ConnectToLocalHost();

  // Connect to a remote host through Me2Me.
  void ConnectToRemoteHost(const std::string& host_name);

  // Enter the pin number and connect.
  void EnterPin(const std::string& name);

  // Helper to get the pin number used for me2me authentication.
  std::string me2me_pin() { return me2me_pin_; }

  // Helper to get the name of the remote host to connect to.
  std::string remote_host_name() { return remote_host_name_; }

 private:
  // Change behavior of the default host resolver to allow DNS lookup
  // to proceed instead of being blocked by the test infrastructure.
  void EnableDNSLookupForThisTest(
    net::RuleBasedHostResolverProc* host_resolver);

  // We need to reset the DNS lookup when we finish, or the test will fail.
  void DisableDNSLookupForThisTest();

  void ParseCommandLine();

  // Accessor methods.

  // Helper to get the path to the crx file of the webapp to be tested.
  base::FilePath WebAppCrxPath() { return webapp_crx_; }

  // Helper to get the extension ID of the installed chromoting webapp.
  std::string ChromotingID() { return chromoting_id_; }

  // Whether to perform the cleanup tasks (uninstalling chromoting, etc).
  // This is useful for diagnostic purposes.
  bool NoCleanup() { return no_cleanup_; }

  // Whether to install the chromoting extension before running the test cases.
  // This is useful for diagnostic purposes.
  bool NoInstall() { return no_install_; }

  // Helper to construct the starting URL of the installed chromoting webapp.
  GURL Chromoting_Main_URL() {
    return GURL("chrome-extension://" + ChromotingID() + "/main.html");
  }

  // Helper to retrieve the current URL of the active tab in the active browser
  // window.
  GURL GetCurrentURL() {
    return GetCurrentURLInBrowser(active_browser_);
  }

  // Helper to retrieve the current URL of the active tab in the given browser
  // window.
  static GURL GetCurrentURLInBrowser(Browser* browser) {
    return browser->tab_strip_model()->GetActiveWebContents()->GetURL();
  }

  // Helpers to execute javascript code on a web page.

  // Helper to execute a javascript code snippet on the current page of
  // the active browser window.
  void ExecuteScript(const std::string& script);

  // Helper to execute a javascript code snippet one the current page of
  // the active browser window and wait for page load to complete.
  void ExecuteScriptAndWaitForAnyPageLoad(const std::string& script);

  // Helper to execute a javascript code snippet one the current page of
  // the active browser window and wait until the target url is loaded.
  // This is used when the target page is loaded after one or more redirections.
  void ExecuteScriptAndWaitForPageLoad(const std::string& script,
                                       const GURL& target);

  // Helper to execute a javascript code snippet on the current page of
  // the active browser window and extract the boolean result.
  bool ExecuteScriptAndExtractBool(const std::string& script) {
    return ExecuteScriptAndExtractBool(active_browser_, script);
  }

  // Helper to execute a javascript code snippet one the current page of
  // the active browser window and extract the boolean result.
  static bool ExecuteScriptAndExtractBool(Browser* browser,
                                          const std::string& script);

  // Helper to execute a javascript code snippet one the current page of
  // the active browser window and extract the int result.
  int ExecuteScriptAndExtractInt(const std::string& script);

  // Helper to execute a javascript code snippet one the current page of
  // the active browser window and extract the string result.
  std::string ExecuteScriptAndExtractString(const std::string& script);

  // Helper to navigate to a given url.
  void NavigateToURLAndWaitForPageLoad(const GURL& url);

  // Helper to check whether an html element with the given name exists on
  // the current page of the active browser window.
  bool HtmlElementExists(const std::string& name) {
    return ExecuteScriptAndExtractBool(
        "document.getElementById(\"" + name + "\") != null");
  }

  // Helper to check whether a html element with the given name is visible.
  bool HtmlElementVisible(const std::string& name);

  // Click on the named HTML control.
  void ClickOnControl(const std::string& name);

  // Wait for the me2me connection to be established.
  void WaitForConnection();

  // Checking whether the localHost has been initialized.
  bool IsLocalHostReady();

  // Callback used by EnterPin to check whether the pin form is visible
  // and to dismiss the host-needs-update dialog.
  bool IsPinFormVisible();

  // Callback used by WaitForConnection to check whether the connection
  // has been established.
  bool IsSessionConnected();

  // Callback used by ExecuteScriptAndWaitForPageLoad to check whether
  // the given page is currently loaded in the given browser window.
  static bool IsURLLoadedInWindow(Browser* browser, const GURL& url);

  // Callback used by Approve to check whether the chromoting app has
  // successfully authenticated with the google services.
  static bool IsAuthenticated(Browser* browser);

  // Fields

  // This test needs to make live DNS requests for access to
  // GAIA and sync server URLs under google.com. We use a scoped version
  // to override the default resolver while the test is active.
  scoped_ptr<net::ScopedDefaultHostResolverProc> mock_host_resolver_override_;

  // The "active" browser window the test needs to interact with.
  // We initialize |active_browser_| to the browser instance created by
  // InProcessBrowserTest as the initial browser window to run test in.
  // Whenever a new browser window is spawned and needs attention
  // |active_browser_| is set to that browser window and all subsequent
  // test actions happen there.
  // And when the focus is returned to the original browser window
  // |active_browser_| is reset to browser().
  // This pattern is sufficient for simple streamlined workflows where all
  // the browser windows form a LIFO stack. The test always interacts
  // with the "active" window which is always on the top of the "browser
  // stack". See also http://crrev.com/chrome/browser/ui/browser_list.h
  // If we ever need to deal with more complicated workflows the test
  // code will need to explicitly pass browser instances to the helper
  // routines.
  Browser* active_browser_;

  bool no_cleanup_;
  bool no_install_;
  std::string chromoting_id_;
  base::FilePath webapp_crx_;
  std::string username_;
  std::string password_;
  std::string me2me_pin_;
  std::string remote_host_name_;
};

}  // namespace remoting

#endif  // CHROME_TEST_REMOTING_REMOTE_DESKTOP_BROWSERTEST_H_
