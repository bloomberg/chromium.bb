// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/remoting/remote_desktop_browsertest.h"

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/test/remoting/key_code_conv.h"
#include "chrome/test/remoting/waiter.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/test_utils.h"

using extensions::Extension;

namespace remoting {

RemoteDesktopBrowserTest::RemoteDesktopBrowserTest() {}

RemoteDesktopBrowserTest::~RemoteDesktopBrowserTest() {}

void RemoteDesktopBrowserTest::SetUp() {
  ParseCommandLine();
  ExtensionBrowserTest::SetUp();
}

void RemoteDesktopBrowserTest::SetUpOnMainThread() {
  ExtensionBrowserTest::SetUpOnMainThread();

  // Initializing to browser() before RunTestOnMainThread() and after
  // |InProcessBrowserTest::browser_| is initialized in
  // InProcessBrowserTest::RunTestOnMainThreadLoop()
  active_browser_ = browser();
}

// Change behavior of the default host resolver to avoid DNS lookup errors,
// so we can make network calls.
void RemoteDesktopBrowserTest::SetUpInProcessBrowserTestFixture() {
  // The resolver object lifetime is managed by sync_test_setup, not here.
  EnableDNSLookupForThisTest(
      new net::RuleBasedHostResolverProc(host_resolver()));
}

void RemoteDesktopBrowserTest::TearDownInProcessBrowserTestFixture() {
  DisableDNSLookupForThisTest();
}

void RemoteDesktopBrowserTest::VerifyInternetAccess() {
  GURL google_url("http://www.google.com");
  NavigateToURLAndWaitForPageLoad(google_url);

  EXPECT_EQ(GetCurrentURL().host(), "www.google.com");
}

bool RemoteDesktopBrowserTest::HtmlElementVisible(const std::string& name) {
  _ASSERT_TRUE(HtmlElementExists(name));

  ExecuteScript(
      "function isElementVisible(name) {"
      "  var element = document.getElementById(name);"
      "  /* The existence of the element has already been ASSERTed. */"
      "  do {"
      "    if (element.hidden) {"
      "      return false;"
      "    }"
      "    element = element.parentNode;"
      "  } while (element != null);"
      "  return true;"
      "};");

  return ExecuteScriptAndExtractBool(
      "isElementVisible(\"" + name + "\")");
}

void RemoteDesktopBrowserTest::InstallChromotingApp() {
  base::FilePath install_dir(WebAppCrxPath());
  scoped_refptr<const Extension> extension(InstallExtensionWithUIAutoConfirm(
      install_dir, 1, active_browser_));

  EXPECT_FALSE(extension.get() == NULL);
}

void RemoteDesktopBrowserTest::UninstallChromotingApp() {
  UninstallExtension(ChromotingID());
  chromoting_id_.clear();
}

void RemoteDesktopBrowserTest::VerifyChromotingLoaded(bool expected) {
  const ExtensionSet* extensions = extension_service()->extensions();
  scoped_refptr<const extensions::Extension> extension;
  ExtensionSet::const_iterator iter;
  bool installed = false;

  for (iter = extensions->begin(); iter != extensions->end(); ++iter) {
    extension = *iter;
    // Is there a better way to recognize the chromoting extension
    // than name comparison?
    if (extension->name() == "Chromoting" ||
        extension->name() == "Chrome Remote Desktop") {
      installed = true;
      break;
    }
  }

  if (installed) {
    chromoting_id_ = extension->id();

    EXPECT_EQ(extension->GetType(),
        extensions::Manifest::TYPE_LEGACY_PACKAGED_APP);

    EXPECT_TRUE(extension->ShouldDisplayInAppLauncher());
  }

  EXPECT_EQ(installed, expected);
}

void RemoteDesktopBrowserTest::LaunchChromotingApp() {
  ASSERT_FALSE(ChromotingID().empty());

  const GURL chromoting_main = Chromoting_Main_URL();
  NavigateToURLAndWaitForPageLoad(chromoting_main);

  EXPECT_EQ(GetCurrentURL(), chromoting_main);
}

void RemoteDesktopBrowserTest::Authorize() {
  // The chromoting extension should be installed.
  ASSERT_FALSE(ChromotingID().empty());

  // The chromoting main page should be loaded in the current tab
  // and isAuthenticated() should be false (auth dialog visible).
  ASSERT_EQ(Chromoting_Main_URL(), GetCurrentURL());
  ASSERT_FALSE(ExecuteScriptAndExtractBool(
      "remoting.oauth2.isAuthenticated()"));

  // The first observer monitors the creation of the new window for
  // the Google loging page.
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_BROWSER_OPENED,
      content::NotificationService::AllSources());

  // The second observer monitors the loading of the Google login page.
  // Unfortunately we cannot specify a source in this observer because
  // we can't get a handle of the new window until the first observer
  // has finished waiting. But we will assert that the source of the
  // load stop event is indeed the newly created browser window.
  content::WindowedNotificationObserver observer2(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());

  ClickOnControl("auth-button");

  observer.Wait();

  const content::Source<Browser>* source =
      static_cast<const content::Source<Browser>*>(&observer.source());
  active_browser_ = source->ptr();

  observer2.Wait();

  const content::Source<content::NavigationController>* source2 =
      static_cast<const content::Source<content::NavigationController>*>(
          &observer2.source());
  content::NavigationController* controller = source2->ptr();

  EXPECT_EQ(controller->GetWebContents(),
            active_browser_->tab_strip_model()->GetActiveWebContents());

  // Verify the active tab is at the "Google Accounts" login page.
  EXPECT_EQ("accounts.google.com", GetCurrentURL().host());
  EXPECT_TRUE(HtmlElementExists("Email"));
  EXPECT_TRUE(HtmlElementExists("Passwd"));
}

void RemoteDesktopBrowserTest::Authenticate() {
  // The chromoting extension should be installed.
  ASSERT_FALSE(ChromotingID().empty());

  // The active tab should have the "Google Accounts" login page loaded.
  ASSERT_EQ("accounts.google.com", GetCurrentURL().host());
  ASSERT_TRUE(HtmlElementExists("Email"));
  ASSERT_TRUE(HtmlElementExists("Passwd"));

  // Now log in using the username and password passed in from the command line.
  ExecuteScriptAndWaitForAnyPageLoad(
      "document.getElementById(\"Email\").value = \"" + username_ + "\";" +
      "document.getElementById(\"Passwd\").value = \"" + password_ +"\";" +
      "document.forms[\"gaia_loginform\"].submit();");

  EXPECT_EQ(GetCurrentURL().host(), "accounts.google.com");

  // TODO(weitaosu): Is there a better way to verify we are on the
  // "Request for Permission" page?
  EXPECT_TRUE(HtmlElementExists("submit_approve_access"));
}

void RemoteDesktopBrowserTest::Approve() {
  // The chromoting extension should be installed.
  ASSERT_FALSE(ChromotingID().empty());

  // The active tab should have the chromoting app loaded.
  ASSERT_EQ("accounts.google.com", GetCurrentURL().host());

  // Is there a better way to verify we are on the "Request for Permission"
  // page?
  ASSERT_TRUE(HtmlElementExists("submit_approve_access"));

  const GURL chromoting_main = Chromoting_Main_URL();

  // |active_browser_| is still set to the login window but the observer
  // should be set up to observe the chromoting window because that is
  // where we'll receive the message from the login window and reload the
  // chromoting app.
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
        base::Bind(
            &RemoteDesktopBrowserTest::IsAuthenticated, browser()));

  ExecuteScript(
      "lso.approveButtonAction();"
      "document.forms[\"connect-approve\"].submit();");

  observer.Wait();

  // Resetting |active_browser_| to browser() now that the Google login
  // window is closed and the focus has returned to the chromoting window.
  active_browser_ = browser();

  ASSERT_TRUE(GetCurrentURL() == chromoting_main);

  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      "remoting.oauth2.isAuthenticated()"));
}

void RemoteDesktopBrowserTest::StartMe2Me() {
  // The chromoting extension should be installed.
  ASSERT_FALSE(ChromotingID().empty());

  // The active tab should have the chromoting app loaded.
  ASSERT_EQ(Chromoting_Main_URL(), GetCurrentURL());
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      "remoting.oauth2.isAuthenticated()"));

  // The Me2Me host list should be hidden.
  ASSERT_FALSE(HtmlElementVisible("me2me-content"));
  // The Me2Me "Get Start" button should be visible.
  ASSERT_TRUE(HtmlElementVisible("get-started-me2me"));

  // Starting Me2Me.
  ExecuteScript("remoting.showMe2MeUiAndSave();");

  EXPECT_TRUE(HtmlElementVisible("me2me-content"));
  EXPECT_FALSE(HtmlElementVisible("me2me-first-run"));

  // Wait until localHost is initialized. This can take a while.
  ConditionalTimeoutWaiter waiter(
      base::TimeDelta::FromSeconds(3),
      base::TimeDelta::FromSeconds(1),
      base::Bind(&RemoteDesktopBrowserTest::IsLocalHostReady, this));
  EXPECT_TRUE(waiter.Wait());

  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      "remoting.hostList.localHost_.hostName && "
      "remoting.hostList.localHost_.hostId && "
      "remoting.hostList.localHost_.status && "
      "remoting.hostList.localHost_.status == 'ONLINE'"));
}

void RemoteDesktopBrowserTest::DisconnectMe2Me() {
  // The chromoting extension should be installed.
  ASSERT_FALSE(ChromotingID().empty());

  // The active tab should have the chromoting app loaded.
  ASSERT_EQ(Chromoting_Main_URL(), GetCurrentURL());
  ASSERT_TRUE(RemoteDesktopBrowserTest::IsSessionConnected());

  ClickOnControl("toolbar-stub");

  EXPECT_TRUE(HtmlElementVisible("session-toolbar"));

  ClickOnControl("toolbar-disconnect");

  EXPECT_TRUE(HtmlElementVisible("client-dialog"));
  EXPECT_TRUE(HtmlElementVisible("client-reconnect-button"));
  EXPECT_TRUE(HtmlElementVisible("client-finished-me2me-button"));

  ClickOnControl("client-finished-me2me-button");

  EXPECT_FALSE(HtmlElementVisible("client-dialog"));
}

void RemoteDesktopBrowserTest::SimulateKeyPressWithCode(
    ui::KeyboardCode keyCode,
    const char* code) {
  SimulateKeyPressWithCode(keyCode, code, false, false, false, false);
}

void RemoteDesktopBrowserTest::SimulateKeyPressWithCode(
    ui::KeyboardCode keyCode,
    const char* code,
    bool control,
    bool shift,
    bool alt,
    bool command) {
  content::SimulateKeyPressWithCode(
      active_browser_->tab_strip_model()->GetActiveWebContents(),
      keyCode,
      code,
      control,
      shift,
      alt,
      command);
}

void RemoteDesktopBrowserTest::SimulateCharInput(char c) {
  const char* code;
  ui::KeyboardCode keyboard_code;
  bool shift;
  GetKeyValuesFromChar(c, &code, &keyboard_code, &shift);
  ASSERT_TRUE(code != NULL);
  SimulateKeyPressWithCode(keyboard_code, code, false, shift, false, false);
}

void RemoteDesktopBrowserTest::SimulateStringInput(const std::string& input) {
  for (size_t i = 0; i < input.length(); ++i)
    SimulateCharInput(input[i]);
}

void RemoteDesktopBrowserTest::SimulateMouseLeftClickAt(int x, int y) {
  SimulateMouseClickAt(0, WebKit::WebMouseEvent::ButtonLeft, x, y);
}

void RemoteDesktopBrowserTest::SimulateMouseClickAt(
    int modifiers, WebKit::WebMouseEvent::Button button, int x, int y) {
  ExecuteScript(
      "var clientPluginElement = "
           "document.getElementById('session-client-plugin');"
      "var clientPluginRect = clientPluginElement.getBoundingClientRect();");

  int top = ExecuteScriptAndExtractInt("clientPluginRect.top");
  int left = ExecuteScriptAndExtractInt("clientPluginRect.left");
  int width = ExecuteScriptAndExtractInt("clientPluginRect.width");
  int height = ExecuteScriptAndExtractInt("clientPluginRect.height");

  ASSERT_GT(x, 0);
  ASSERT_LT(x, width);
  ASSERT_GT(y, 0);
  ASSERT_LT(y, height);

  content::SimulateMouseClickAt(
      browser()->tab_strip_model()->GetActiveWebContents(),
      modifiers,
      button,
      gfx::Point(left + x, top + y));
}

void RemoteDesktopBrowserTest::Install() {
  // TODO(weitaosu): add support for unpacked extension (the v2 app needs it).
  if (!NoInstall()) {
    VerifyChromotingLoaded(false);
    InstallChromotingApp();
  }

  VerifyChromotingLoaded(true);
}

void RemoteDesktopBrowserTest::Cleanup() {
  // TODO(weitaosu): Remove this hack by blocking on the appropriate
  // notification.
  // The browser may still be loading images embedded in the webapp. If we
  // uinstall it now those load will fail. Navigating away to avoid the load
  // failures.
  ui_test_utils::NavigateToURL(active_browser_, GURL("about:blank"));

  if (!NoCleanup()) {
    UninstallChromotingApp();
    VerifyChromotingLoaded(false);
  }
}

void RemoteDesktopBrowserTest::Auth() {
  Authorize();
  Authenticate();
  Approve();
}

void RemoteDesktopBrowserTest::ConnectToLocalHost() {
  // Verify that the local host is online.
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      "remoting.hostList.localHost_.hostName && "
      "remoting.hostList.localHost_.hostId && "
      "remoting.hostList.localHost_.status && "
      "remoting.hostList.localHost_.status == 'ONLINE'"));

  // Connect.
  ClickOnControl("this-host-connect");

  // Enter the pin # passed in from the command line.
  EnterPin(me2me_pin());

  WaitForConnection();
}

void RemoteDesktopBrowserTest::ConnectToRemoteHost(
    const std::string& host_name) {
  std::string host_id = ExecuteScriptAndExtractString(
      "remoting.hostList.getHostIdForName('" + host_name + "')");

  EXPECT_FALSE(host_id.empty());
  std::string element_id = "host_" + host_id;

  // Verify the host is online.
  std::string host_div_class = ExecuteScriptAndExtractString(
      "document.getElementById('" + element_id + "').parentNode.className");
  EXPECT_NE(std::string::npos, host_div_class.find("host-online"));

  ClickOnControl(element_id);

  // Enter the pin # passed in from the command line.
  EnterPin(me2me_pin());

  WaitForConnection();
}

void RemoteDesktopBrowserTest::EnableDNSLookupForThisTest(
    net::RuleBasedHostResolverProc* host_resolver) {
  // mock_host_resolver_override_ takes ownership of the resolver.
  scoped_refptr<net::RuleBasedHostResolverProc> resolver =
      new net::RuleBasedHostResolverProc(host_resolver);
  resolver->AllowDirectLookup("*.google.com");
  // On Linux, we use Chromium's NSS implementation which uses the following
  // hosts for certificate verification. Without these overrides, running the
  // integration tests on Linux causes errors as we make external DNS lookups.
  resolver->AllowDirectLookup("*.thawte.com");
  resolver->AllowDirectLookup("*.geotrust.com");
  resolver->AllowDirectLookup("*.gstatic.com");
  resolver->AllowDirectLookup("*.googleapis.com");
  mock_host_resolver_override_.reset(
      new net::ScopedDefaultHostResolverProc(resolver.get()));
}

void RemoteDesktopBrowserTest::DisableDNSLookupForThisTest() {
  mock_host_resolver_override_.reset();
}

void RemoteDesktopBrowserTest::ParseCommandLine() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();

  // The test framework overrides any command line user-data-dir
  // argument with a /tmp/.org.chromium.Chromium.XXXXXX directory.
  // That happens in the ChromeTestLauncherDelegate, and affects
  // all unit tests (no opt out available). It intentionally erases
  // any --user-data-dir switch if present and appends a new one.
  // Re-override the default data dir if override-user-data-dir
  // is specified.
  if (command_line->HasSwitch(kOverrideUserDataDir)) {
    const base::FilePath& override_user_data_dir =
        command_line->GetSwitchValuePath(kOverrideUserDataDir);

    ASSERT_FALSE(override_user_data_dir.empty());

    command_line->AppendSwitchPath(switches::kUserDataDir,
                                   override_user_data_dir);
  }

  username_ = command_line->GetSwitchValueASCII(kUsername);
  password_ = command_line->GetSwitchValueASCII(kkPassword);
  me2me_pin_ = command_line->GetSwitchValueASCII(kMe2MePin);
  remote_host_name_ = command_line->GetSwitchValueASCII(kRemoteHostName);

  no_cleanup_ = command_line->HasSwitch(kNoCleanup);
  no_install_ = command_line->HasSwitch(kNoInstall);

  if (!no_install_) {
    webapp_crx_ = command_line->GetSwitchValuePath(kWebAppCrx);
    ASSERT_FALSE(webapp_crx_.empty());
  }
}

void RemoteDesktopBrowserTest::ExecuteScript(const std::string& script) {
  ASSERT_TRUE(content::ExecuteScript(
      active_browser_->tab_strip_model()->GetActiveWebContents(), script));
}

void RemoteDesktopBrowserTest::ExecuteScriptAndWaitForAnyPageLoad(
    const std::string& script) {
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &active_browser_->tab_strip_model()->GetActiveWebContents()->
              GetController()));

  ExecuteScript(script);

  observer.Wait();
}

void RemoteDesktopBrowserTest::ExecuteScriptAndWaitForPageLoad(
    const std::string& script,
    const GURL& target) {
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      base::Bind(&RemoteDesktopBrowserTest::IsURLLoadedInWindow,
                 active_browser_,
                 target));

  ExecuteScript(script);

  observer.Wait();
}

// static
bool RemoteDesktopBrowserTest::ExecuteScriptAndExtractBool(
    Browser* browser, const std::string& script) {
  bool result;
  _ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      browser->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(" + script + ");",
      &result));

  return result;
}

int RemoteDesktopBrowserTest::ExecuteScriptAndExtractInt(
    const std::string& script) {
  int result;
  _ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      active_browser_->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(" + script + ");",
      &result));

  return result;
}

std::string RemoteDesktopBrowserTest::ExecuteScriptAndExtractString(
    const std::string& script) {
  std::string result;
  _ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      active_browser_->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(" + script + ");",
      &result));

  return result;
}

// Helper to navigate to a given url.
void RemoteDesktopBrowserTest::NavigateToURLAndWaitForPageLoad(
    const GURL& url) {
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &active_browser_->tab_strip_model()->GetActiveWebContents()->
              GetController()));

  ui_test_utils::NavigateToURL(active_browser_, url);
  observer.Wait();
}

void RemoteDesktopBrowserTest::ClickOnControl(const std::string& name) {
  ASSERT_TRUE(HtmlElementVisible(name));

  ExecuteScript("document.getElementById(\"" + name + "\").click();");
}

void RemoteDesktopBrowserTest::EnterPin(const std::string& pin) {
  // Wait for the pin-form to be displayed. This can take a while.
  // We also need to dismiss the host-needs-update dialog if it comes up.
  // TODO(weitaosu) 1: Instead of polling, can we register a callback to be
  // called when the pin-form is ready?
  // TODO(weitaosu) 2: Instead of blindly dismiss the host-needs-update dialog,
  // we should verify that it only pops up at the right circumstance. That
  // probably belongs in a separate test case though.
  ConditionalTimeoutWaiter waiter(
      base::TimeDelta::FromSeconds(5),
      base::TimeDelta::FromSeconds(1),
      base::Bind(&RemoteDesktopBrowserTest::IsPinFormVisible, this));
  EXPECT_TRUE(waiter.Wait());

  ExecuteScript(
      "document.getElementById(\"pin-entry\").value = \"" + pin + "\";");

  ClickOnControl("pin-connect-button");
}

void RemoteDesktopBrowserTest::WaitForConnection() {
  // Wait until the client has connected to the server.
  // This can take a while.
  // TODO(weitaosu): Instead of polling, can we register a callback to
  // remoting.clientSession.onStageChange_?
  ConditionalTimeoutWaiter waiter(
      base::TimeDelta::FromSeconds(4),
      base::TimeDelta::FromSeconds(1),
      base::Bind(&RemoteDesktopBrowserTest::IsSessionConnected, this));
  EXPECT_TRUE(waiter.Wait());

  // The client is not yet ready to take input when the session state becomes
  // CONNECTED. Wait for 2 seconds for the client to become ready.
  // TODO(weitaosu): Find a way to detect when the client is truly ready.
  TimeoutWaiter(base::TimeDelta::FromSeconds(2)).Wait();
}

bool RemoteDesktopBrowserTest::IsLocalHostReady() {
  // TODO(weitaosu): Instead of polling, can we register a callback to
  // remoting.hostList.setLocalHost_?
  return ExecuteScriptAndExtractBool("remoting.hostList.localHost_ != null");
}

bool RemoteDesktopBrowserTest::IsSessionConnected() {
  return ExecuteScriptAndExtractBool(
      "remoting.clientSession != null && "
      "remoting.clientSession.getState() == "
      "remoting.ClientSession.State.CONNECTED");
}

bool RemoteDesktopBrowserTest::IsPinFormVisible() {
  if (HtmlElementVisible("host-needs-update-connect-button"))
    ClickOnControl("host-needs-update-connect-button");

  return HtmlElementVisible("pin-form");
}

// static
bool RemoteDesktopBrowserTest::IsURLLoadedInWindow(Browser* browser,
                                                   const GURL& url) {
  return GetCurrentURLInBrowser(browser) == url;
}

// static
bool RemoteDesktopBrowserTest::IsAuthenticated(Browser* browser) {
  return ExecuteScriptAndExtractBool(
      browser, "remoting.oauth2.isAuthenticated()");
}

}  // namespace remoting
