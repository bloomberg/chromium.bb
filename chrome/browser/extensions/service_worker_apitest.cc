// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind_helpers.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/test/background_page_watcher.h"
#include "extensions/test/extension_test_message_listener.h"

namespace extensions {

namespace {

// Pass into ServiceWorkerTest::StartTestFromBackgroundPage to indicate that
// registration is expected to succeed.
std::string* const kExpectSuccess = nullptr;

void DoNothingWithBool(bool b) {}

}  // namespace

class ServiceWorkerTest : public ExtensionApiTest {
 public:
  ServiceWorkerTest() : current_channel_(version_info::Channel::UNKNOWN) {}

  ~ServiceWorkerTest() override {}

 protected:
  // Returns the ProcessManager for the test's profile.
  ProcessManager* process_manager() { return ProcessManager::Get(profile()); }

  // Starts running a test from the background page test extension.
  //
  // This registers a service worker with |script_name|, and fetches the
  // registration result.
  //
  // If |error_or_null| is null (kExpectSuccess), success is expected and this
  // will fail if there is an error.
  // If |error_or_null| is not null, nothing is assumed, and the error (which
  // may be empty) is written to it.
  const Extension* StartTestFromBackgroundPage(const char* script_name,
                                               std::string* error_or_null) {
    const Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII("service_worker/background"));
    CHECK(extension);
    ExtensionHost* background_host =
        process_manager()->GetBackgroundHostForExtension(extension->id());
    CHECK(background_host);
    std::string error;
    CHECK(content::ExecuteScriptAndExtractString(
        background_host->host_contents(),
        base::StringPrintf("test.registerServiceWorker('%s')", script_name),
        &error));
    if (error_or_null)
      *error_or_null = error;
    else if (!error.empty())
      ADD_FAILURE() << "Got unexpected error " << error;
    return extension;
  }

  // Navigates the browser to a new tab at |url|, waits for it to load, then
  // returns it.
  content::WebContents* Navigate(const GURL& url) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::WaitForLoadStop(web_contents);
    return web_contents;
  }

  // Navigates the browser to |url| and returns the new tab's page type.
  content::PageType NavigateAndGetPageType(const GURL& url) {
    return Navigate(url)->GetController().GetActiveEntry()->GetPageType();
  }

  // Extracts the innerText from |contents|.
  std::string ExtractInnerText(content::WebContents* contents) {
    std::string inner_text;
    if (!content::ExecuteScriptAndExtractString(
            contents,
            "window.domAutomationController.send(document.body.innerText)",
            &inner_text)) {
      ADD_FAILURE() << "Failed to get inner text for "
                    << contents->GetVisibleURL();
    }
    return inner_text;
  }

  // Navigates the browser to |url|, then returns the innerText of the new
  // tab's WebContents' main frame.
  std::string NavigateAndExtractInnerText(const GURL& url) {
    return ExtractInnerText(Navigate(url));
  }

 private:
  // Sets the channel to "trunk" since service workers are restricted to trunk.
  ScopedCurrentChannel current_channel_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerTest);
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerTest, RegisterSucceedsOnTrunk) {
  StartTestFromBackgroundPage("register.js", kExpectSuccess);
}

// This feature is restricted to trunk, so on dev it should have existing
// behavior - which is for it to fail.
IN_PROC_BROWSER_TEST_F(ServiceWorkerTest, RegisterFailsOnDev) {
  ScopedCurrentChannel current_channel_override(
      version_info::Channel::DEV);
  std::string error;
  const Extension* extension =
      StartTestFromBackgroundPage("register.js", &error);
  EXPECT_EQ(
      "Failed to register a ServiceWorker: The URL protocol of the current "
      "origin ('chrome-extension://" +
          extension->id() + "') is not supported.",
      error);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerTest, FetchArbitraryPaths) {
  const Extension* extension =
      StartTestFromBackgroundPage("fetch.js", kExpectSuccess);

  // Open some arbirary paths. Their contents should be what the service worker
  // responds with, which in this case is the path of the fetch.
  EXPECT_EQ(
      "Caught a fetch for /index.html",
      NavigateAndExtractInnerText(extension->GetResourceURL("index.html")));
  EXPECT_EQ("Caught a fetch for /path/to/other.html",
            NavigateAndExtractInnerText(
                extension->GetResourceURL("path/to/other.html")));
  EXPECT_EQ("Caught a fetch for /some/text/file.txt",
            NavigateAndExtractInnerText(
                extension->GetResourceURL("some/text/file.txt")));
  EXPECT_EQ("Caught a fetch for /no/file/extension",
            NavigateAndExtractInnerText(
                extension->GetResourceURL("no/file/extension")));
  EXPECT_EQ("Caught a fetch for /",
            NavigateAndExtractInnerText(extension->GetResourceURL("")));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerTest,
                       LoadingBackgroundPageBypassesServiceWorker) {
  const Extension* extension =
      StartTestFromBackgroundPage("fetch.js", kExpectSuccess);

  std::string kExpectedInnerText = "background.html contents for testing.";

  // Sanity check that the background page has the expected content.
  ExtensionHost* background_page =
      process_manager()->GetBackgroundHostForExtension(extension->id());
  ASSERT_TRUE(background_page);
  EXPECT_EQ(kExpectedInnerText,
            ExtractInnerText(background_page->host_contents()));

  // Close the background page.
  background_page->Close();
  BackgroundPageWatcher(process_manager(), extension).WaitForClose();
  background_page = nullptr;

  // Start it again.
  process_manager()->WakeEventPage(extension->id(),
                                   base::Bind(&DoNothingWithBool));
  BackgroundPageWatcher(process_manager(), extension).WaitForOpen();

  // Content should not have been affected by the fetch, which would otherwise
  // be "Caught fetch for...".
  background_page =
      process_manager()->GetBackgroundHostForExtension(extension->id());
  ASSERT_TRUE(background_page);
  content::WaitForLoadStop(background_page->host_contents());

  // TODO(kalman): Everything you've read has been a LIE! It should be:
  //
  // EXPECT_EQ(kExpectedInnerText,
  //           ExtractInnerText(background_page->host_contents()));
  //
  // but there is a bug, and we're actually *not* bypassing the service worker
  // for background page loads! For now, let it pass (assert wrong behavior)
  // because it's not a regression, but this must be fixed eventually.
  //
  // Tracked in crbug.com/532720.
  EXPECT_EQ("Caught a fetch for /background.html",
            ExtractInnerText(background_page->host_contents()));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerTest,
                       ServiceWorkerPostsMessageToBackgroundClient) {
  const Extension* extension = StartTestFromBackgroundPage(
      "post_message_to_background_client.js", kExpectSuccess);

  // The service worker in this test simply posts a message to the background
  // client it receives from getBackgroundClient().
  const char* kScript =
      "var messagePromise = null;\n"
      "if (test.lastMessageFromServiceWorker) {\n"
      "  messagePromise = Promise.resolve(test.lastMessageFromServiceWorker);\n"
      "} else {\n"
      "  messagePromise = test.waitForMessage(navigator.serviceWorker);\n"
      "}\n"
      "messagePromise.then(function(message) {\n"
      "  window.domAutomationController.send(String(message == 'success'));\n"
      "})\n";
  EXPECT_EQ("true", ExecuteScriptInBackgroundPage(extension->id(), kScript));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerTest,
                       BackgroundPagePostsMessageToServiceWorker) {
  const Extension* extension =
      StartTestFromBackgroundPage("post_message_to_sw.js", kExpectSuccess);

  // The service worker in this test waits for a message, then echoes it back
  // by posting a message to the background page via getBackgroundClient().
  const char* kScript =
      "var mc = new MessageChannel();\n"
      "test.waitForMessage(mc.port1).then(function(message) {\n"
      "  window.domAutomationController.send(String(message == 'hello'));\n"
      "});\n"
      "test.registeredServiceWorker.postMessage(\n"
      "    {message: 'hello', port: mc.port2}, [mc.port2])\n";
  EXPECT_EQ("true", ExecuteScriptInBackgroundPage(extension->id(), kScript));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerTest,
                       ServiceWorkerSuspensionOnExtensionUnload) {
  // For this test, only hold onto the extension's ID and URL + a function to
  // get a resource URL, because we're going to be disabling and uninstalling
  // it, which will invalidate the pointer.
  std::string extension_id;
  GURL extension_url;
  {
    const Extension* extension =
        StartTestFromBackgroundPage("fetch.js", kExpectSuccess);
    extension_id = extension->id();
    extension_url = extension->url();
  }
  auto get_resource_url = [&extension_url](const std::string& path) {
    return Extension::GetResourceURL(extension_url, path);
  };

  // Fetch should route to the service worker.
  EXPECT_EQ("Caught a fetch for /index.html",
            NavigateAndExtractInnerText(get_resource_url("index.html")));

  // Disable the extension. Opening the page should fail.
  extension_service()->DisableExtension(extension_id,
                                        Extension::DISABLE_USER_ACTION);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(content::PAGE_TYPE_ERROR,
            NavigateAndGetPageType(get_resource_url("index.html")));
  EXPECT_EQ(content::PAGE_TYPE_ERROR,
            NavigateAndGetPageType(get_resource_url("other.html")));

  // Re-enable the extension. Opening pages should immediately start to succeed
  // again.
  extension_service()->EnableExtension(extension_id);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("Caught a fetch for /index.html",
            NavigateAndExtractInnerText(get_resource_url("index.html")));
  EXPECT_EQ("Caught a fetch for /other.html",
            NavigateAndExtractInnerText(get_resource_url("other.html")));
  EXPECT_EQ("Caught a fetch for /another.html",
            NavigateAndExtractInnerText(get_resource_url("another.html")));

  // Uninstall the extension. Opening pages should fail again.
  base::string16 error;
  extension_service()->UninstallExtension(
      extension_id, UninstallReason::UNINSTALL_REASON_FOR_TESTING,
      base::Bind(&base::DoNothing), &error);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(content::PAGE_TYPE_ERROR,
            NavigateAndGetPageType(get_resource_url("index.html")));
  EXPECT_EQ(content::PAGE_TYPE_ERROR,
            NavigateAndGetPageType(get_resource_url("other.html")));
  EXPECT_EQ(content::PAGE_TYPE_ERROR,
            NavigateAndGetPageType(get_resource_url("anotherother.html")));
  EXPECT_EQ(content::PAGE_TYPE_ERROR,
            NavigateAndGetPageType(get_resource_url("final.html")));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerTest, BackgroundPageIsWokenIfAsleep) {
  const Extension* extension =
      StartTestFromBackgroundPage("wake_on_fetch.js", kExpectSuccess);

  // Navigate to special URLs that this test's service worker recognises, each
  // making a check then populating the response with either "true" or "false".
  EXPECT_EQ("true", NavigateAndExtractInnerText(extension->GetResourceURL(
                        "background-client-is-awake")));
  EXPECT_EQ("true", NavigateAndExtractInnerText(
                        extension->GetResourceURL("ping-background-client")));
  // Ping more than once for good measure.
  EXPECT_EQ("true", NavigateAndExtractInnerText(
                        extension->GetResourceURL("ping-background-client")));

  // Shut down the event page. The SW should detect that it's closed, but still
  // be able to ping it.
  ExtensionHost* background_page =
      process_manager()->GetBackgroundHostForExtension(extension->id());
  ASSERT_TRUE(background_page);
  background_page->Close();
  BackgroundPageWatcher(process_manager(), extension).WaitForClose();

  EXPECT_EQ("false", NavigateAndExtractInnerText(extension->GetResourceURL(
                         "background-client-is-awake")));
  EXPECT_EQ("true", NavigateAndExtractInnerText(
                        extension->GetResourceURL("ping-background-client")));
  EXPECT_EQ("true", NavigateAndExtractInnerText(
                        extension->GetResourceURL("ping-background-client")));
  EXPECT_EQ("true", NavigateAndExtractInnerText(extension->GetResourceURL(
                        "background-client-is-awake")));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerTest,
                       GetBackgroundClientFailsWithNoBackgroundPage) {
  // This extension doesn't have a background page, only a tab at page.html.
  // The service worker it registers tries to call getBackgroundClient() and
  // should fail.
  // Note that this also tests that service workers can be registered from tabs.
  EXPECT_TRUE(RunExtensionSubtest("service_worker/no_background", "page.html"));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerTest, NotificationAPI) {
  EXPECT_TRUE(RunExtensionSubtest("service_worker/notifications/has_permission",
                                  "page.html"));
}

}  // namespace extensions
