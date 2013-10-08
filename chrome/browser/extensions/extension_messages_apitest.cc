// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/test_extension_dir.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace extensions {
namespace {

class MessageSender : public content::NotificationObserver {
 public:
  MessageSender() {
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING,
                   content::NotificationService::AllSources());
  }

 private:
  static scoped_ptr<base::ListValue> BuildEventArguments(
      const bool last_message,
      const std::string& data) {
    DictionaryValue* event = new DictionaryValue();
    event->SetBoolean("lastMessage", last_message);
    event->SetString("data", data);
    scoped_ptr<base::ListValue> arguments(new base::ListValue());
    arguments->Append(event);
    return arguments.Pass();
  }

  static scoped_ptr<Event> BuildEvent(scoped_ptr<base::ListValue> event_args,
                                      Profile* profile,
                                      GURL event_url) {
    scoped_ptr<Event> event(new Event("test.onMessage", event_args.Pass()));
    event->restrict_to_profile = profile;
    event->event_url = event_url;
    return event.Pass();
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    EventRouter* event_router = ExtensionSystem::Get(
        content::Source<Profile>(source).ptr())->event_router();

    // Sends four messages to the extension. All but the third message sent
    // from the origin http://b.com/ are supposed to arrive.
    event_router->BroadcastEvent(BuildEvent(
        BuildEventArguments(false, "no restriction"),
        content::Source<Profile>(source).ptr(),
        GURL()));
    event_router->BroadcastEvent(BuildEvent(
        BuildEventArguments(false, "http://a.com/"),
        content::Source<Profile>(source).ptr(),
        GURL("http://a.com/")));
    event_router->BroadcastEvent(BuildEvent(
        BuildEventArguments(false, "http://b.com/"),
        content::Source<Profile>(source).ptr(),
        GURL("http://b.com/")));
    event_router->BroadcastEvent(BuildEvent(
        BuildEventArguments(true, "last message"),
        content::Source<Profile>(source).ptr(),
        GURL()));
  }

  content::NotificationRegistrar registrar_;
};

// Tests that message passing between extensions and content scripts works.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Messaging) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("messaging/connect")) << message_;
}

// Tests that message passing from one extension to another works.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MessagingExternal) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("..").AppendASCII("good")
                    .AppendASCII("Extensions")
                    .AppendASCII("bjafgdebaacbbbecmhlhpofkepfkgcpa")
                    .AppendASCII("1.0")));

  ASSERT_TRUE(RunExtensionTest("messaging/connect_external")) << message_;
}

// Tests that messages with event_urls are only passed to extensions with
// appropriate permissions.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MessagingEventURL) {
  MessageSender sender;
  ASSERT_TRUE(RunExtensionTest("messaging/event_url")) << message_;
}

// Tests connecting from a panel to its extension.
class PanelMessagingTest : public ExtensionApiTest {
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnablePanels);
  }
};

IN_PROC_BROWSER_TEST_F(PanelMessagingTest, MessagingPanel) {
  ASSERT_TRUE(RunExtensionTest("messaging/connect_panel")) << message_;
}

// Tests externally_connectable between a web page and an extension.
//
// TODO(kalman): Test between extensions. This is already tested in this file,
// but not with externally_connectable set in the manifest.
//
// TODO(kalman): Test with host permissions.
class ExternallyConnectableMessagingTest : public ExtensionApiTest {
 protected:
  // Result codes from the test. These must match up with |results| in
  // c/t/d/extensions/api_test/externally_connectable/assertions.json.
  enum Result {
    OK = 0,
    NAMESPACE_NOT_DEFINED = 1,
    FUNCTION_NOT_DEFINED = 2,
    COULD_NOT_ESTABLISH_CONNECTION_ERROR = 3,
    OTHER_ERROR = 4,
    INCORRECT_RESPONSE_SENDER = 5,
    INCORRECT_RESPONSE_MESSAGE = 6,
  };

  bool AppendIframe(const GURL& src) {
    bool result;
    CHECK(content::ExecuteScriptAndExtractBool(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "actions.appendIframe('" + src.spec() + "');", &result));
    return result;
  }

  Result CanConnectAndSendMessages(const std::string& extension_id) {
    return CanConnectAndSendMessages(browser(), extension_id, "");
  }

  Result CanConnectAndSendMessages(const std::string& extension_id,
                                   const char* frame_xpath,
                                   const char* message) {
    return CanConnectAndSendMessages(browser(), extension_id, frame_xpath,
                                     message);
  }

  Result CanConnectAndSendMessages(Browser* browser,
                                   const std::string& extension_id) {
    return CanConnectAndSendMessages(browser, extension_id, "");
  }

  Result CanConnectAndSendMessages(const std::string& extension_id,
                                   const char* frame_xpath) {
    return CanConnectAndSendMessages(browser(), extension_id, frame_xpath);
  }

  Result CanConnectAndSendMessages(Browser* browser,
                                   const std::string& extension_id,
                                   const char* frame_xpath,
                                   const char* message = NULL) {
    int result;
    std::string args = "'" + extension_id + "'";
    if (message)
      args += std::string(", '") + message + "'";
    CHECK(content::ExecuteScriptInFrameAndExtractInt(
        browser->tab_strip_model()->GetActiveWebContents(),
        frame_xpath,
        base::StringPrintf("assertions.canConnectAndSendMessages(%s)",
                           args.c_str()).c_str(),
        &result));
    return static_cast<Result>(result);
  }

  testing::AssertionResult AreAnyNonWebApisDefined() {
    return AreAnyNonWebApisDefined("");
  }

  testing::AssertionResult AreAnyNonWebApisDefined(const char* frame_xpath) {
    // All runtime API methods are non-web except for sendRequest and connect.
    const char* non_messaging_apis[] = {
        "getBackgroundPage",
        "getManifest",
        "getURL",
        "reload",
        "requestUpdateCheck",
        "restart",
        "connectNative",
        "sendNativeMessage",
        "onStartup",
        "onInstalled",
        "onSuspend",
        "onSuspendCanceled",
        "onUpdateAvailable",
        "onBrowserUpdateAvailable",
        "onConnect",
        "onConnectExternal",
        "onMessage",
        "onMessageExternal",
        "onRestartRequired",
        "id",
    };

    // Turn the array into a JS array, which effectively gets eval()ed.
    std::string as_js_array;
    for (size_t i = 0; i < arraysize(non_messaging_apis); ++i) {
      as_js_array += as_js_array.empty() ? "[" : ",";
      as_js_array += base::StringPrintf("'%s'", non_messaging_apis[i]);
    }
    as_js_array += "]";

    bool any_defined;
    CHECK(content::ExecuteScriptInFrameAndExtractBool(
        browser()->tab_strip_model()->GetActiveWebContents(),
        frame_xpath,
        "assertions.areAnyRuntimePropertiesDefined(" + as_js_array + ")",
        &any_defined));
    return any_defined ?
        testing::AssertionSuccess() : testing::AssertionFailure();
  }

  GURL GetURLForPath(const std::string& host, const std::string& path) {
    std::string port = base::IntToString(embedded_test_server()->port());
    GURL::Replacements replacements;
    replacements.SetHostStr(host);
    replacements.SetPortStr(port);
    return embedded_test_server()->GetURL(path).ReplaceComponents(replacements);
  }

  GURL chromium_org_url() {
    return GetURLForPath("www.chromium.org", "/chromium.org.html");
  }

  GURL google_com_url() {
    return GetURLForPath("www.google.com", "/google.com.html");
  }

  const Extension* LoadChromiumConnectableExtension() {
    const Extension* extension =
        LoadExtensionIntoDir(&web_connectable_dir_, base::StringPrintf(
            "{"
            "  \"name\": \"chromium_connectable\","
            "  %s,"
            "  \"externally_connectable\": {"
            "    \"matches\": [\"*://*.chromium.org:*/*\"]"
            "  }"
            "}",
            common_manifest()));
    CHECK(extension);
    return extension;
  }

  const Extension* LoadNotConnectableExtension() {
    const Extension* extension =
        LoadExtensionIntoDir(&not_connectable_dir_, base::StringPrintf(
            "{"
            "  \"name\": \"not_connectable\","
            "  %s"
            "}",
            common_manifest()));
    CHECK(extension);
    return extension;
  }

  void InitializeTestServer() {
    base::FilePath test_data;
    EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data));
    embedded_test_server()->ServeFilesFromDirectory(test_data.AppendASCII(
        "extensions/api_test/messaging/externally_connectable/sites"));
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
    host_resolver()->AddRule("*", embedded_test_server()->base_url().host());
  }

  const char* close_background_message() {
    return "closeBackgroundPage";
  }

 private:
  const Extension* LoadExtensionIntoDir(TestExtensionDir* dir,
                                        const std::string& manifest) {
    dir->WriteManifest(manifest);
    dir->WriteFile(FILE_PATH_LITERAL("background.js"),
                   base::StringPrintf(
        "function maybeClose(message) {\n"
        "  if (message.indexOf('%s') >= 0)\n"
        "    window.setTimeout(function() { window.close() }, 0);\n"
        "}\n"
        "chrome.runtime.onMessageExternal.addListener(\n"
        "    function(message, sender, reply) {\n"
        "  reply({ message: message, sender: sender });\n"
        "  maybeClose(message);\n"
        "});\n"
        "chrome.runtime.onConnectExternal.addListener(function(port) {\n"
        "  port.onMessage.addListener(function(message) {\n"
        "    port.postMessage({ message: message, sender: port.sender });\n"
        "    maybeClose(message);\n"
        "  });\n"
        "});\n",
                   close_background_message()));
    return LoadExtension(dir->unpacked_path());
  }

  const char* common_manifest() {
    return "\"version\": \"1.0\","
           "\"background\": {"
           "    \"scripts\": [\"background.js\"],"
           "    \"persistent\": false"
           "},"
           "\"manifest_version\": 2";
  }

  TestExtensionDir web_connectable_dir_;
  TestExtensionDir not_connectable_dir_;
};

IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest, NotInstalled) {
  InitializeTestServer();

  const char kFakeId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

  ui_test_utils::NavigateToURL(browser(), chromium_org_url());
  EXPECT_EQ(NAMESPACE_NOT_DEFINED, CanConnectAndSendMessages(kFakeId));
  EXPECT_FALSE(AreAnyNonWebApisDefined());

  ui_test_utils::NavigateToURL(browser(), google_com_url());
  EXPECT_EQ(NAMESPACE_NOT_DEFINED, CanConnectAndSendMessages(kFakeId));
  EXPECT_FALSE(AreAnyNonWebApisDefined());
}

// Tests two extensions on the same sites: one web connectable, one not.
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       WebConnectableAndNotConnectable) {
  InitializeTestServer();

  // Install the web connectable extension. chromium.org can connect to it,
  // google.com can't.
  const Extension* chromium_connectable = LoadChromiumConnectableExtension();

  ui_test_utils::NavigateToURL(browser(), chromium_org_url());
  EXPECT_EQ(OK, CanConnectAndSendMessages(chromium_connectable->id()));
  EXPECT_FALSE(AreAnyNonWebApisDefined());

  ui_test_utils::NavigateToURL(browser(), google_com_url());
  EXPECT_EQ(NAMESPACE_NOT_DEFINED,
            CanConnectAndSendMessages(chromium_connectable->id()));
  EXPECT_FALSE(AreAnyNonWebApisDefined());

  // Install the non-connectable extension. Nothing can connect to it.
  const Extension* not_connectable = LoadNotConnectableExtension();

  ui_test_utils::NavigateToURL(browser(), chromium_org_url());
  // Namespace will be defined here because |chromium_connectable| can connect
  // to it - so this will be the "cannot establish connection" error.
  EXPECT_EQ(COULD_NOT_ESTABLISH_CONNECTION_ERROR,
            CanConnectAndSendMessages(not_connectable->id()));
  EXPECT_FALSE(AreAnyNonWebApisDefined());

  ui_test_utils::NavigateToURL(browser(), google_com_url());
  EXPECT_EQ(NAMESPACE_NOT_DEFINED,
            CanConnectAndSendMessages(not_connectable->id()));
  EXPECT_FALSE(AreAnyNonWebApisDefined());
}

// See http://crbug.com/297866
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       DISABLED_BackgroundPageClosesOnMessageReceipt) {
  InitializeTestServer();

  // Install the web connectable extension.
  const Extension* chromium_connectable = LoadChromiumConnectableExtension();

  ui_test_utils::NavigateToURL(browser(), chromium_org_url());
  // If the background page closes after receipt of the message, it will still
  // reply to this message...
  EXPECT_EQ(OK, CanConnectAndSendMessages(chromium_connectable->id(),
                                          "",
                                          close_background_message()));
  // and be re-opened by receipt of a subsequent message.
  EXPECT_EQ(OK, CanConnectAndSendMessages(chromium_connectable->id()));
}

// Tests that enabling and disabling an extension makes the runtime bindings
// appear and disappear.
//
// TODO(kalman): Test with multiple extensions that can be accessed by the same
// host.
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       EnablingAndDisabling) {
  InitializeTestServer();

  const Extension* chromium_connectable = LoadChromiumConnectableExtension();
  const Extension* not_connectable = LoadNotConnectableExtension();

  ui_test_utils::NavigateToURL(browser(), chromium_org_url());
  EXPECT_EQ(OK, CanConnectAndSendMessages(chromium_connectable->id()));
  EXPECT_EQ(COULD_NOT_ESTABLISH_CONNECTION_ERROR,
            CanConnectAndSendMessages(not_connectable->id()));

  DisableExtension(chromium_connectable->id());
  EXPECT_EQ(COULD_NOT_ESTABLISH_CONNECTION_ERROR,
            CanConnectAndSendMessages(chromium_connectable->id()));

  EnableExtension(chromium_connectable->id());
  EXPECT_EQ(OK, CanConnectAndSendMessages(chromium_connectable->id()));
  EXPECT_EQ(COULD_NOT_ESTABLISH_CONNECTION_ERROR,
            CanConnectAndSendMessages(not_connectable->id()));
}

// Tests connection from incognito tabs. Spanning mode only.
//
// TODO(kalman): ensure that we exercise split vs spanning incognito logic
// somewhere. This is a test that should be shared with the content script logic
// so it's not really our specific concern for web connectable.
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest, FromIncognito) {
  InitializeTestServer();

  const Extension* chromium_connectable = LoadChromiumConnectableExtension();

  Browser* incognito_browser = ui_test_utils::OpenURLOffTheRecord(
      profile()->GetOffTheRecordProfile(),
      chromium_org_url());

  // No connection because incognito enabled hasn't been set.
  const std::string& id = chromium_connectable->id();
  EXPECT_EQ(COULD_NOT_ESTABLISH_CONNECTION_ERROR,
            CanConnectAndSendMessages(incognito_browser, id));

  // Then yes.
  ExtensionPrefs::Get(profile())->SetIsIncognitoEnabled(id, true);
  EXPECT_EQ(OK, CanConnectAndSendMessages(incognito_browser, id));
}

// Tests a connection from an iframe within a tab which doesn't have
// permission. Iframe should work.
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       FromIframeWithPermission) {
  InitializeTestServer();

  const Extension* extension = LoadChromiumConnectableExtension();

  ui_test_utils::NavigateToURL(browser(), google_com_url());
  EXPECT_EQ(NAMESPACE_NOT_DEFINED, CanConnectAndSendMessages(extension->id()));
  EXPECT_FALSE(AreAnyNonWebApisDefined());

  ASSERT_TRUE(AppendIframe(chromium_org_url()));

  const char* frame_xpath = "//iframe[1]";
  EXPECT_EQ(OK, CanConnectAndSendMessages(extension->id(), frame_xpath));
  EXPECT_FALSE(AreAnyNonWebApisDefined(frame_xpath));
}

// Tests connection from an iframe without permission within a tab that does.
// Iframe shouldn't work.
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       FromIframeWithoutPermission) {
  InitializeTestServer();

  const Extension* extension = LoadChromiumConnectableExtension();

  ui_test_utils::NavigateToURL(browser(), chromium_org_url());
  EXPECT_EQ(OK, CanConnectAndSendMessages(extension->id()));
  EXPECT_FALSE(AreAnyNonWebApisDefined());

  ASSERT_TRUE(AppendIframe(google_com_url()));

  const char* frame_xpath = "//iframe[1]";
  EXPECT_EQ(NAMESPACE_NOT_DEFINED,
            CanConnectAndSendMessages(extension->id(), frame_xpath));
  EXPECT_FALSE(AreAnyNonWebApisDefined(frame_xpath));
}

}  // namespace
};  // namespace extensions
