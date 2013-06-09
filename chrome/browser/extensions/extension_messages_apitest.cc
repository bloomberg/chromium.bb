// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "googleurl/src/gurl.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

class MessageSender : public content::NotificationObserver {
 public:
  MessageSender() {
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING,
                   content::NotificationService::AllSources());
  }

 private:
  static scoped_ptr<ListValue> BuildEventArguments(const bool last_message,
                                                   const std::string& data) {
    DictionaryValue* event = new DictionaryValue();
    event->SetBoolean("lastMessage", last_message);
    event->SetString("data", data);
    scoped_ptr<ListValue> arguments(new ListValue());
    arguments->Append(event);
    return arguments.Pass();
  }

  static scoped_ptr<extensions::Event> BuildEvent(
      scoped_ptr<ListValue> event_args,
      Profile* profile,
      GURL event_url) {
    scoped_ptr<extensions::Event> event(new extensions::Event(
        "test.onMessage", event_args.Pass()));
    event->restrict_to_profile = profile;
    event->event_url = event_url;
    return event.Pass();
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    extensions::EventRouter* event_router =
        extensions::ExtensionSystem::Get(
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

}  // namespace

// Tests that message passing between extensions and content scripts works.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Messaging) {
  ASSERT_TRUE(StartTestServer());
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
  // c/t/d/extensions/api_test/externally_connectable/sites/assertions.json.
  enum Result {
    OK = 0,
    NAMESPACE_NOT_DEFINED = 1,
    FUNCTION_NOT_DEFINED = 2,
    COULD_NOT_ESTABLISH_CONNECTION_ERROR = 3,
    OTHER_ERROR = 4,
    INCORRECT_RESPONSE_SENDER = 5,
    INCORRECT_RESPONSE_MESSAGE = 6,
  };

  Result CanConnectAndSendMessages(const std::string& extension_id) {
    int result;
    CHECK(content::ExecuteScriptAndExtractInt(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "assertions.canConnectAndSendMessages('" + extension_id + "')",
        &result));
    return static_cast<Result>(result);
  }

  testing::AssertionResult AreAnyNonWebApisDefined() {
    // All runtime API methods are non-web except for sendRequest and connect.
    const std::string non_messaging_apis[] = {
        "getBackgroundPage",
        "getManifest",
        "getURL",
        "reload",
        "requestUpdateCheck",
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
        "id",
    };
    return AreAnyRuntimePropertiesDefined(std::vector<std::string>(
        non_messaging_apis,
        non_messaging_apis + arraysize(non_messaging_apis)));
  }

  GURL GetURLForPath(const std::string& host, const std::string& path) {
    std::string port = base::IntToString(embedded_test_server()->port());
    GURL::Replacements replacements;
    replacements.SetHostStr(host);
    replacements.SetPortStr(port);
    return embedded_test_server()->GetURL(path).ReplaceComponents(replacements);
  }

 private:
  testing::AssertionResult AreAnyRuntimePropertiesDefined(
      const std::vector<std::string>& names) {
    for (size_t i = 0; i < names.size(); ++i) {
      if (IsRuntimePropertyDefined(names[i]) == OK)
        return testing::AssertionSuccess() << names[i] << " is defined";
    }
    return testing::AssertionFailure()
        << "none of " << names.size() << " properties are defined";
  }

  Result IsRuntimePropertyDefined(const std::string& name) {
    int result_int;
    CHECK(content::ExecuteScriptAndExtractInt(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "assertions.isDefined('" + name + "')",
        &result_int));
    return static_cast<Result>(result_int);
  }
};

IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       ExternallyConnectableMessaging) {
  const char kExtensionDir[] = "messaging/externally_connectable";

  // The extension allows connections from chromium.org but not google.com.
  const char kChromiumOrg[] = "www.chromium.org";
  const char kGoogleCom[] = "www.google.com";

  base::FilePath test_data;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data));
  embedded_test_server()->ServeFilesFromDirectory(
      test_data.AppendASCII("extensions/api_test").AppendASCII(kExtensionDir));
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  host_resolver()->AddRule("*", embedded_test_server()->base_url().host());

  const GURL kChromiumOrgUrl =
      GetURLForPath(kChromiumOrg, "/sites/chromium.org.html");
  const GURL kGoogleComUrl =
      GetURLForPath(kGoogleCom, "/sites/google.com.html");

  // When an extension isn't installed all attempts to connect to it should
  // fail.
  const std::string kFakeId = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  ui_test_utils::NavigateToURL(browser(), kChromiumOrgUrl);
  EXPECT_EQ(COULD_NOT_ESTABLISH_CONNECTION_ERROR,
            CanConnectAndSendMessages(kFakeId));
  EXPECT_FALSE(AreAnyNonWebApisDefined());

  ui_test_utils::NavigateToURL(browser(), kGoogleComUrl);
  EXPECT_EQ(COULD_NOT_ESTABLISH_CONNECTION_ERROR,
            CanConnectAndSendMessages(kFakeId));
  EXPECT_FALSE(AreAnyNonWebApisDefined());

  // Install the web connectable extension. chromium.org can connect to it,
  // google.com can't.
  const extensions::Extension* web_connectable = LoadExtension(
      test_data_dir_.AppendASCII(kExtensionDir).AppendASCII("web_connectable"));

  ui_test_utils::NavigateToURL(browser(), kChromiumOrgUrl);
  EXPECT_EQ(OK, CanConnectAndSendMessages(web_connectable->id()));
  EXPECT_FALSE(AreAnyNonWebApisDefined());

  ui_test_utils::NavigateToURL(browser(), kGoogleComUrl);
  EXPECT_EQ(COULD_NOT_ESTABLISH_CONNECTION_ERROR,
            CanConnectAndSendMessages(web_connectable->id()));
  EXPECT_FALSE(AreAnyNonWebApisDefined());

  // Install the non-connectable extension. Nothing can connect to it.
  const extensions::Extension* not_connectable = LoadExtension(
      test_data_dir_.AppendASCII(kExtensionDir).AppendASCII("not_connectable"));

  ui_test_utils::NavigateToURL(browser(), kChromiumOrgUrl);
  EXPECT_EQ(COULD_NOT_ESTABLISH_CONNECTION_ERROR,
            CanConnectAndSendMessages(not_connectable->id()));
  EXPECT_FALSE(AreAnyNonWebApisDefined());

  ui_test_utils::NavigateToURL(browser(), kGoogleComUrl);
  EXPECT_EQ(COULD_NOT_ESTABLISH_CONNECTION_ERROR,
            CanConnectAndSendMessages(not_connectable->id()));
  EXPECT_FALSE(AreAnyNonWebApisDefined());
}
