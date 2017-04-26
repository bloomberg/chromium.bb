// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/test.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace OnMessage = api::test::OnMessage;

namespace {

// Tests running extension APIs on WebUI.
class ExtensionWebUITest : public ExtensionApiTest {
 protected:
  testing::AssertionResult RunTest(const char* name,
                                   const GURL& page_url,
                                   const GURL& frame_url,
                                   bool expected_result) {
    std::string script;
    {
      base::ThreadRestrictions::ScopedAllowIO allow_io;
      // Tests are located in chrome/test/data/extensions/webui/$(name).
      base::FilePath path;
      PathService::Get(chrome::DIR_TEST_DATA, &path);
      path =
          path.AppendASCII("extensions").AppendASCII("webui").AppendASCII(name);

      // Read the test.
      if (!base::PathExists(path))
        return testing::AssertionFailure() << "Couldn't find " << path.value();
      base::ReadFileToString(path, &script);
      script = "(function(){'use strict';" + script + "}());";
    }

    // Run the test.
    bool actual_result = false;
    content::RenderFrameHost* webui = NavigateToWebUI(page_url, frame_url);
    if (!webui)
      return testing::AssertionFailure() << "Failed to navigate to WebUI";
    CHECK(content::ExecuteScriptAndExtractBool(webui, script, &actual_result));
    return (expected_result == actual_result)
               ? testing::AssertionSuccess()
               : (testing::AssertionFailure() << "Check console output");
  }

  testing::AssertionResult RunTestOnExtensionsFrame(const char* name) {
    // In the current extension WebUI design, the content is actually hosted in
    // an iframe at chrome://extensions-frame.
    return RunTest(name,
                   GURL("chrome://extensions"),
                   GURL("chrome://extensions-frame"),
                   true);  // tests on chrome://extensions-frame should succeed
  }

  testing::AssertionResult RunTestOnChromeExtensionsFrame(const char* name) {
    // Like RunTestOnExtensionsFrame, but chrome://extensions is an alias for
    // chrome://chrome/extensions so test it explicitly.
    return RunTest(name,
                   GURL("chrome://chrome/extensions"),
                   GURL("chrome://extensions-frame"),
                   true);  // tests on chrome://extensions-frame should succeed
  }

  testing::AssertionResult RunTestOnChromeExtensions(const char* name) {
    // Despite the extensions page being hosted in an iframe, also test the
    // top-level chrome://extensions page (which actually loads
    // chrome://chrome/extensions). In the past there was a bug where top-level
    // extension WebUI bindings weren't correctly set up.
    return RunTest(name,
                   GURL("chrome://chrome/extensions"),
                   GURL("chrome://chrome/extensions"),
                   true);  // tests on chrome://chrome/extensions should succeed
  }

  testing::AssertionResult RunTestOnAbout(const char* name) {
    // chrome://about is an innocuous page that doesn't have any bindings.
    // Tests should fail.
    return RunTest(name,
                   GURL("chrome://about"),
                   GURL("chrome://about"),
                   false);  // tests on chrome://about should fail
  }

 private:
  // Navigates the browser to a WebUI page and returns the RenderFrameHost for
  // that page.
  content::RenderFrameHost* NavigateToWebUI(const GURL& page_url,
                                            const GURL& frame_url) {
    ui_test_utils::NavigateToURL(browser(), page_url);

    content::WebContents* active_web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    if (active_web_contents->GetLastCommittedURL() == frame_url)
      return active_web_contents->GetMainFrame();

    return FrameMatchingPredicate(
        active_web_contents,
        base::Bind(&content::FrameHasSourceUrl, frame_url));
  }
};

#if !defined(OS_WIN)  // flaky http://crbug.com/530722

IN_PROC_BROWSER_TEST_F(ExtensionWebUITest, SanityCheckAvailableAPIsInFrame) {
  ASSERT_TRUE(RunTestOnExtensionsFrame("sanity_check_available_apis.js"));
}

IN_PROC_BROWSER_TEST_F(ExtensionWebUITest,
                       SanityCheckAvailableAPIsInChromeFrame) {
  ASSERT_TRUE(RunTestOnChromeExtensionsFrame("sanity_check_available_apis.js"));
}

IN_PROC_BROWSER_TEST_F(ExtensionWebUITest, SanityCheckAvailableAPIsInToplevel) {
  ASSERT_TRUE(RunTestOnChromeExtensions("sanity_check_available_apis.js"));
}

IN_PROC_BROWSER_TEST_F(ExtensionWebUITest, SanityCheckUnavailableAPIs) {
  ASSERT_TRUE(RunTestOnAbout("sanity_check_available_apis.js"));
}

// Tests chrome.test.sendMessage, which exercises WebUI making a
// function call and receiving a response.
IN_PROC_BROWSER_TEST_F(ExtensionWebUITest, SendMessage) {
  std::unique_ptr<ExtensionTestMessageListener> listener(
      new ExtensionTestMessageListener("ping", true));

  ASSERT_TRUE(RunTestOnExtensionsFrame("send_message.js"));

  ASSERT_TRUE(listener->WaitUntilSatisfied());
  listener->Reply("pong");

  listener.reset(new ExtensionTestMessageListener(false));
  ASSERT_TRUE(listener->WaitUntilSatisfied());
  EXPECT_EQ("true", listener->message());
}

// Tests chrome.runtime.onMessage, which exercises WebUI registering and
// receiving an event.
IN_PROC_BROWSER_TEST_F(ExtensionWebUITest, OnMessage) {
  ASSERT_TRUE(RunTestOnExtensionsFrame("on_message.js"));

  OnMessage::Info info;
  info.data = "hi";
  info.last_message = true;
  EventRouter::Get(profile())->BroadcastEvent(base::WrapUnique(
      new Event(events::RUNTIME_ON_MESSAGE, OnMessage::kEventName,
                OnMessage::Create(info))));

  std::unique_ptr<ExtensionTestMessageListener> listener(
      new ExtensionTestMessageListener(false));
  ASSERT_TRUE(listener->WaitUntilSatisfied());
  EXPECT_EQ("true", listener->message());
}

// Tests chrome.runtime.lastError, which exercises WebUI accessing a property
// on an API which it doesn't actually have access to. A bindings test really.
IN_PROC_BROWSER_TEST_F(ExtensionWebUITest, RuntimeLastError) {
  std::unique_ptr<ExtensionTestMessageListener> listener(
      new ExtensionTestMessageListener("ping", true));

  ASSERT_TRUE(RunTestOnExtensionsFrame("runtime_last_error.js"));

  ASSERT_TRUE(listener->WaitUntilSatisfied());
  listener->ReplyWithError("unknown host");

  listener.reset(new ExtensionTestMessageListener(false));
  ASSERT_TRUE(listener->WaitUntilSatisfied());
  EXPECT_EQ("true", listener->message());
}

IN_PROC_BROWSER_TEST_F(ExtensionWebUITest, CanEmbedExtensionOptions) {
  std::unique_ptr<ExtensionTestMessageListener> listener(
      new ExtensionTestMessageListener("ready", true));

  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("extension_options")
                        .AppendASCII("embed_self"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(RunTestOnExtensionsFrame("can_embed_extension_options.js"));

  ASSERT_TRUE(listener->WaitUntilSatisfied());
  listener->Reply(extension->id());
  listener.reset(new ExtensionTestMessageListener("load", false));
  ASSERT_TRUE(listener->WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ExtensionWebUITest, ReceivesExtensionOptionsOnClose) {
  std::unique_ptr<ExtensionTestMessageListener> listener(
      new ExtensionTestMessageListener("ready", true));

  const Extension* extension =
      InstallExtension(test_data_dir_.AppendASCII("extension_options")
          .AppendASCII("close_self"), 1);
  ASSERT_TRUE(extension);

  ASSERT_TRUE(
      RunTestOnExtensionsFrame("receives_extension_options_on_close.js"));

  ASSERT_TRUE(listener->WaitUntilSatisfied());
  listener->Reply(extension->id());
  listener.reset(new ExtensionTestMessageListener("onclose received", false));
  ASSERT_TRUE(listener->WaitUntilSatisfied());
}

// Regression test for crbug.com/414526.
//
// Same setup as CanEmbedExtensionOptions but disable the extension before
// embedding.
IN_PROC_BROWSER_TEST_F(ExtensionWebUITest, EmbedDisabledExtension) {
  std::unique_ptr<ExtensionTestMessageListener> listener(
      new ExtensionTestMessageListener("ready", true));

  std::string extension_id;
  {
    const Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII("extension_options")
                          .AppendASCII("embed_self"));
    ASSERT_TRUE(extension);
    extension_id = extension->id();
    DisableExtension(extension_id);
  }

  ASSERT_TRUE(RunTestOnExtensionsFrame("can_embed_extension_options.js"));

  ASSERT_TRUE(listener->WaitUntilSatisfied());
  listener->Reply(extension_id);
  listener.reset(new ExtensionTestMessageListener("createfailed", false));
  ASSERT_TRUE(listener->WaitUntilSatisfied());
}

#endif

}  // namespace

}  // namespace extensions
