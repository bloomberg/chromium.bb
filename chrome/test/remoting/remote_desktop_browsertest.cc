// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/remoting/remote_desktop_browsertest.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/remoting/key_code_conv.h"
#include "chrome/test/remoting/page_load_notification_observer.h"
#include "chrome/test/remoting/waiter.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/switches.h"
#include "ui/base/window_open_disposition.h"

namespace remoting {

RemoteDesktopBrowserTest::RemoteDesktopBrowserTest()
    : extension_(NULL) {
}

RemoteDesktopBrowserTest::~RemoteDesktopBrowserTest() {}

void RemoteDesktopBrowserTest::SetUp() {
  ParseCommandLine();
  PlatformAppBrowserTest::SetUp();
}

void RemoteDesktopBrowserTest::SetUpOnMainThread() {
  PlatformAppBrowserTest::SetUpOnMainThread();

  // Pushing the initial WebContents instance onto the stack before
  // RunTestOnMainThread() and after |InProcessBrowserTest::browser_|
  // is initialized in InProcessBrowserTest::RunTestOnMainThreadLoop()
  web_contents_stack_.push_back(
      browser()->tab_strip_model()->GetActiveWebContents());
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
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL("http://www.google.com"), 1);

  EXPECT_EQ(GetCurrentURL().host(), "www.google.com");
}

void RemoteDesktopBrowserTest::OpenClientBrowserPage() {
  // Open the client browser page in a new tab
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(http_server() + "/clientpage.html"),
      NEW_FOREGROUND_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // Save this web content for later reference
  client_web_content_ = browser()->tab_strip_model()->GetActiveWebContents();

  // Go back to the previous tab that has chromoting opened
  browser()->tab_strip_model()->SelectPreviousTab();
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

void RemoteDesktopBrowserTest::InstallChromotingAppCrx() {
  ASSERT_TRUE(!is_unpacked());

  base::FilePath install_dir(WebAppCrxPath());
  scoped_refptr<const Extension> extension(InstallExtensionWithUIAutoConfirm(
      install_dir, 1, browser()));

  EXPECT_FALSE(extension.get() == NULL);

  extension_ = extension.get();
}

void RemoteDesktopBrowserTest::InstallChromotingAppUnpacked() {
  ASSERT_TRUE(is_unpacked());

  scoped_refptr<extensions::UnpackedInstaller> installer =
      extensions::UnpackedInstaller::Create(extension_service());
  installer->set_prompt_for_plugins(false);

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
      content::NotificationService::AllSources());

  installer->Load(webapp_unpacked_);

  observer.Wait();
}

void RemoteDesktopBrowserTest::UninstallChromotingApp() {
  UninstallExtension(ChromotingID());
  extension_ = NULL;
}

void RemoteDesktopBrowserTest::VerifyChromotingLoaded(bool expected) {
  const extensions::ExtensionSet* extensions =
      extension_service()->extensions();
  scoped_refptr<const extensions::Extension> extension;
  bool installed = false;

  for (extensions::ExtensionSet::const_iterator iter = extensions->begin();
       iter != extensions->end(); ++iter) {
    extension = *iter;
    // Is there a better way to recognize the chromoting extension
    // than name comparison?
    if (extension->name() == extension_name_) {
      installed = true;
      break;
    }
  }

  if (installed) {
    if (extension_)
      EXPECT_EQ(extension, extension_);
    else
      extension_ = extension.get();

    // Either a V1 (TYPE_LEGACY_PACKAGED_APP) or a V2 (TYPE_PLATFORM_APP ) app.
    extensions::Manifest::Type type = extension->GetType();
    EXPECT_TRUE(type == extensions::Manifest::TYPE_PLATFORM_APP ||
                type == extensions::Manifest::TYPE_LEGACY_PACKAGED_APP);

    EXPECT_TRUE(extension->ShouldDisplayInAppLauncher());
  }

  ASSERT_EQ(installed, expected);
}

void RemoteDesktopBrowserTest::LaunchChromotingApp() {
  ASSERT_TRUE(extension_);

  GURL chromoting_main = Chromoting_Main_URL();
  // We cannot simply wait for any page load because the first page
  // loaded could be the generated background page. We need to wait
  // till the chromoting main page is loaded.
  PageLoadNotificationObserver observer(chromoting_main);

  OpenApplication(AppLaunchParams(
      browser()->profile(),
      extension_,
      is_platform_app() ? extensions::LAUNCH_CONTAINER_NONE :
          extensions::LAUNCH_CONTAINER_TAB,
      is_platform_app() ? NEW_WINDOW : CURRENT_TAB));

  observer.Wait();


  // The active WebContents instance should be the source of the LOAD_STOP
  // notification.
  content::NavigationController* controller =
      content::Source<content::NavigationController>(observer.source()).ptr();

  content::WebContents* web_contents = controller->GetWebContents();
  if (web_contents != active_web_contents())
    web_contents_stack_.push_back(web_contents);

  app_web_content_ = web_contents;

  if (is_platform_app()) {
    EXPECT_EQ(GetFirstAppWindowWebContents(), active_web_contents());
  } else {
    // For apps v1 only, the DOMOperationObserver is not ready at the LOAD_STOP
    // event. A half second wait is necessary for the subsequent javascript
    // injection to work.
    // TODO(weitaosu): Find out whether there is a more appropriate notification
    // to wait for so we can get rid of this wait.
    ASSERT_TRUE(TimeoutWaiter(base::TimeDelta::FromSeconds(5)).Wait());
  }

  EXPECT_EQ(Chromoting_Main_URL(), GetCurrentURL());
}

void RemoteDesktopBrowserTest::Authorize() {
  // The chromoting extension should be installed.
  ASSERT_TRUE(extension_);

  // The chromoting main page should be loaded in the current tab
  // and isAuthenticated() should be false (auth dialog visible).
  ASSERT_EQ(Chromoting_Main_URL(), GetCurrentURL());
  ASSERT_FALSE(IsAuthenticated());

  // The second observer monitors the loading of the Google login page.
  // Unfortunately we cannot specify a source in this observer because
  // we can't get a handle of the new window until the first observer
  // has finished waiting. But we will assert that the source of the
  // load stop event is indeed the newly created browser window.
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());

  ClickOnControl("auth-button");

  observer.Wait();

  content::NavigationController* controller =
      content::Source<content::NavigationController>(observer.source()).ptr();

  web_contents_stack_.push_back(controller->GetWebContents());

  // Verify the active tab is at the "Google Accounts" login page.
  EXPECT_EQ("accounts.google.com", GetCurrentURL().host());
  EXPECT_TRUE(HtmlElementExists("Email"));
  EXPECT_TRUE(HtmlElementExists("Passwd"));
}

void RemoteDesktopBrowserTest::Authenticate() {
  // The chromoting extension should be installed.
  ASSERT_TRUE(extension_);

  // The active tab should have the "Google Accounts" login page loaded.
  ASSERT_EQ("accounts.google.com", GetCurrentURL().host());
  ASSERT_TRUE(HtmlElementExists("Email"));
  ASSERT_TRUE(HtmlElementExists("Passwd"));

  // Now log in using the username and password passed in from the command line.
  ExecuteScriptAndWaitForAnyPageLoad(
      "document.getElementById(\"Email\").value = \"" + username_ + "\";" +
      "document.getElementById(\"Passwd\").value = \"" + password_ +"\";" +
      "document.forms[\"gaia_loginform\"].submit();");

  // TODO(weitaosu): Is there a better way to verify we are on the
  // "Request for Permission" page?
  // V2 app won't ask for approval here because the chromoting test account
  // has already been granted permissions.
  if (!is_platform_app()) {
    EXPECT_EQ(GetCurrentURL().host(), "accounts.google.com");
    EXPECT_TRUE(HtmlElementExists("submit_approve_access"));
  }
}

void RemoteDesktopBrowserTest::Approve() {
  // The chromoting extension should be installed.
  ASSERT_TRUE(extension_);

  if (is_platform_app()) {
    // Popping the login window off the stack to return to the chromoting
    // window.
    web_contents_stack_.pop_back();

    // There is nothing for the V2 app to approve because the chromoting test
    // account has already been granted permissions.
    // TODO(weitaosu): Revoke the permission at the beginning of the test so
    // that we can test first-time experience here.
    ConditionalTimeoutWaiter waiter(
        base::TimeDelta::FromSeconds(7),
        base::TimeDelta::FromSeconds(1),
        base::Bind(
            &RemoteDesktopBrowserTest::IsAuthenticatedInWindow,
            active_web_contents()));

    ASSERT_TRUE(waiter.Wait());
  } else {
    ASSERT_EQ("accounts.google.com", GetCurrentURL().host());

    // Is there a better way to verify we are on the "Request for Permission"
    // page?
    ASSERT_TRUE(HtmlElementExists("submit_approve_access"));

    const GURL chromoting_main = Chromoting_Main_URL();

    // active_web_contents() is still the login window but the observer
    // should be set up to observe the chromoting window because that is
    // where we'll receive the message from the login window and reload the
    // chromoting app.
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
          base::Bind(
              &RemoteDesktopBrowserTest::IsAuthenticatedInWindow,
              browser()->tab_strip_model()->GetActiveWebContents()));

    ExecuteScript(
        "lso.approveButtonAction();"
        "document.forms[\"connect-approve\"].submit();");

    observer.Wait();

    // Popping the login window off the stack to return to the chromoting
    // window.
    web_contents_stack_.pop_back();
  }

  ASSERT_TRUE(GetCurrentURL() == Chromoting_Main_URL());

  EXPECT_TRUE(IsAuthenticated());
}

void RemoteDesktopBrowserTest::ExpandMe2Me() {
  // The chromoting extension should be installed.
  ASSERT_TRUE(extension_);

  // The active tab should have the chromoting app loaded.
  ASSERT_EQ(Chromoting_Main_URL(), GetCurrentURL());
  EXPECT_TRUE(IsAuthenticated());

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
  ASSERT_TRUE(extension_);

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
      active_web_contents(),
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
  SimulateMouseClickAt(0, blink::WebMouseEvent::ButtonLeft, x, y);
}

void RemoteDesktopBrowserTest::SimulateMouseClickAt(
    int modifiers, blink::WebMouseEvent::Button button, int x, int y) {
  // TODO(weitaosu): The coordinate translation doesn't work correctly for
  // apps v2.
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
  if (!NoInstall()) {
    VerifyChromotingLoaded(false);
    if (is_unpacked())
      InstallChromotingAppUnpacked();
    else
      InstallChromotingAppCrx();
  }

  VerifyChromotingLoaded(true);
}

void RemoteDesktopBrowserTest::Cleanup() {
  // TODO(weitaosu): Remove this hack by blocking on the appropriate
  // notification.
  // The browser may still be loading images embedded in the webapp. If we
  // uinstall it now those load will fail.
  ASSERT_TRUE(TimeoutWaiter(base::TimeDelta::FromSeconds(2)).Wait());

  if (!NoCleanup()) {
    UninstallChromotingApp();
    VerifyChromotingLoaded(false);
  }

  // TODO(chaitali): Remove this additional timeout after we figure out
  // why this is needed for the v1 app to work.
  // Without this timeout the test fail with a "CloseWebContents called for
  // tab not in our strip" error for the v1 app.
  ASSERT_TRUE(TimeoutWaiter(base::TimeDelta::FromSeconds(2)).Wait());
}

void RemoteDesktopBrowserTest::SetUpTestForMe2Me() {
  VerifyInternetAccess();
  Install();
  LaunchChromotingApp();
  Auth();
  ExpandMe2Me();
  LoadScript(app_web_content(), FILE_PATH_LITERAL("browser_test.js"));
}

void RemoteDesktopBrowserTest::Auth() {
  Authorize();
  Authenticate();
  Approve();
}

void RemoteDesktopBrowserTest::ConnectToLocalHost(bool remember_pin) {
  // Verify that the local host is online.
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      "remoting.hostList.localHost_.hostName && "
      "remoting.hostList.localHost_.hostId && "
      "remoting.hostList.localHost_.status && "
      "remoting.hostList.localHost_.status == 'ONLINE'"));

  // Connect.
  ClickOnControl("this-host-connect");

  // Enter the pin # passed in from the command line.
  EnterPin(me2me_pin(), remember_pin);

  WaitForConnection();
}

void RemoteDesktopBrowserTest::ConnectToRemoteHost(
    const std::string& host_name, bool remember_pin) {
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
  EnterPin(me2me_pin(), remember_pin);

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
  extension_name_ = command_line->GetSwitchValueASCII(kExtensionName);
  http_server_ = command_line->GetSwitchValueASCII(kHttpServer);

  no_cleanup_ = command_line->HasSwitch(kNoCleanup);
  no_install_ = command_line->HasSwitch(kNoInstall);

  if (!no_install_) {
    webapp_crx_ = command_line->GetSwitchValuePath(kWebAppCrx);
    webapp_unpacked_ = command_line->GetSwitchValuePath(kWebAppUnpacked);
    // One and only one of these two arguments should be provided.
    ASSERT_NE(webapp_crx_.empty(), webapp_unpacked_.empty());
  }

  // Run with "enable-web-based-signin" flag to enforce web-based sign-in,
  // rather than inline signin. This ensures we use the same authentication
  // page, regardless of whether we are testing the v1 or v2 web-app.
  command_line->AppendSwitch(switches::kEnableWebBasedSignin);

  // Enable experimental extensions; this is to allow adding the LG extensions
  command_line->AppendSwitch(
    extensions::switches::kEnableExperimentalExtensionApis);
}

void RemoteDesktopBrowserTest::ExecuteScript(const std::string& script) {
  ASSERT_TRUE(content::ExecuteScript(active_web_contents(), script));
}

void RemoteDesktopBrowserTest::ExecuteScriptAndWaitForAnyPageLoad(
    const std::string& script) {
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &active_web_contents()->
              GetController()));

  ExecuteScript(script);

  observer.Wait();
}

// static
bool RemoteDesktopBrowserTest::ExecuteScriptAndExtractBool(
    content::WebContents* web_contents, const std::string& script) {
  bool result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send(" + script + ");",
      &result));

  return result;
}

// static
int RemoteDesktopBrowserTest::ExecuteScriptAndExtractInt(
    content::WebContents* web_contents, const std::string& script) {
  int result;
  _ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      web_contents,
      "window.domAutomationController.send(" + script + ");",
      &result));

  return result;
}

// static
std::string RemoteDesktopBrowserTest::ExecuteScriptAndExtractString(
    content::WebContents* web_contents, const std::string& script) {
  std::string result;
  _ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "window.domAutomationController.send(" + script + ");",
      &result));

  return result;
}

// static
bool RemoteDesktopBrowserTest::LoadScript(
    content::WebContents* web_contents,
    const base::FilePath::StringType& path) {
  std::string script;
  base::FilePath src_dir;
  _ASSERT_TRUE(PathService::Get(base::DIR_EXE, &src_dir));
  base::FilePath script_path = src_dir.Append(path);

  if (!base::ReadFileToString(script_path, &script)) {
    LOG(ERROR) << "Failed to load script " << script_path.value();
    return false;
  }

  return content::ExecuteScript(web_contents, script);
}

// static
void RemoteDesktopBrowserTest::RunJavaScriptTest(
    content::WebContents* web_contents,
    const std::string& testName,
    const std::string& testData) {
  std::string result;
  std::string script = "browserTest.runTest(browserTest." + testName + ", " +
                       testData + ");";

  LOG(INFO) << "Executing " << script;

  ASSERT_TRUE(
      content::ExecuteScriptAndExtractString(web_contents, script, &result));

  // Read in the JSON
  base::JSONReader reader;
  scoped_ptr<base::Value> value;
  value.reset(reader.Read(result, base::JSON_ALLOW_TRAILING_COMMAS));

  // Convert to dictionary
  base::DictionaryValue* dict_value = NULL;
  ASSERT_TRUE(value->GetAsDictionary(&dict_value));

  bool succeeded;
  std::string error_message;
  std::string stack_trace;

  // Extract the fields
  ASSERT_TRUE(dict_value->GetBoolean("succeeded", &succeeded));
  ASSERT_TRUE(dict_value->GetString("error_message", &error_message));
  ASSERT_TRUE(dict_value->GetString("stack_trace", &stack_trace));

  EXPECT_TRUE(succeeded) << error_message << "\n" << stack_trace;
}

void RemoteDesktopBrowserTest::ClickOnControl(const std::string& name) {
  ASSERT_TRUE(HtmlElementVisible(name));

  ExecuteScript("document.getElementById(\"" + name + "\").click();");
}

void RemoteDesktopBrowserTest::EnterPin(const std::string& pin,
                                        bool remember_pin) {
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

  if (remember_pin) {
    EXPECT_TRUE(HtmlElementVisible("remember-pin"));
    EXPECT_FALSE(ExecuteScriptAndExtractBool(
        "document.getElementById('remember-pin-checkbox').checked"));
    ClickOnControl("remember-pin");
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        "document.getElementById('remember-pin-checkbox').checked"));
  }

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
  // If some form of PINless authentication is enabled, the host version
  // warning may appear while waiting for the session to connect.
  DismissHostVersionWarningIfVisible();

  return ExecuteScriptAndExtractBool(
      "remoting.clientSession != null && "
      "remoting.clientSession.getState() == "
      "remoting.ClientSession.State.CONNECTED");
}

bool RemoteDesktopBrowserTest::IsPinFormVisible() {
  DismissHostVersionWarningIfVisible();
  return HtmlElementVisible("pin-form");
}

void RemoteDesktopBrowserTest::DismissHostVersionWarningIfVisible() {
  if (HtmlElementVisible("host-needs-update-connect-button"))
    ClickOnControl("host-needs-update-connect-button");
}

// static
bool RemoteDesktopBrowserTest::IsAuthenticatedInWindow(
    content::WebContents* web_contents) {
  return ExecuteScriptAndExtractBool(
      web_contents, "remoting.identity.isAuthenticated()");
}

// static
bool RemoteDesktopBrowserTest::IsHostActionComplete(
    content::WebContents* client_web_content,
    std::string host_action_var) {
  return ExecuteScriptAndExtractBool(
      client_web_content,
      host_action_var);
}

}  // namespace remoting
