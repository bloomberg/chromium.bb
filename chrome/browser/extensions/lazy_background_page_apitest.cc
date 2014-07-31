// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/lazy_background_page_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

using extensions::Extension;

namespace {

// This unfortunate bit of silliness is necessary when loading an extension in
// incognito. The goal is to load the extension, enable incognito, then wait
// for both background pages to load and close. The problem is that enabling
// incognito involves reloading the extension - and the background pages may
// have already loaded once before then. So we wait until the extension is
// unloaded before listening to the background page notifications.
class LoadedIncognitoObserver : public extensions::ExtensionRegistryObserver {
 public:
  explicit LoadedIncognitoObserver(Profile* profile)
      : profile_(profile), extension_registry_observer_(this) {
    extension_registry_observer_.Add(
        extensions::ExtensionRegistry::Get(profile_));
  }

  void Wait() {
    ASSERT_TRUE(original_complete_.get());
    original_complete_->Wait();
    incognito_complete_->Wait();
  }

 private:
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const Extension* extension,
      extensions::UnloadedExtensionInfo::Reason reason) OVERRIDE {
    original_complete_.reset(new LazyBackgroundObserver(profile_));
    incognito_complete_.reset(
        new LazyBackgroundObserver(profile_->GetOffTheRecordProfile()));
  }

  Profile* profile_;
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_;
  scoped_ptr<LazyBackgroundObserver> original_complete_;
  scoped_ptr<LazyBackgroundObserver> incognito_complete_;
};

}  // namespace

class LazyBackgroundPageApiTest : public ExtensionApiTest {
 public:
  LazyBackgroundPageApiTest() {}
  virtual ~LazyBackgroundPageApiTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // Set shorter delays to prevent test timeouts.
    command_line->AppendSwitchASCII(
        extensions::switches::kEventPageIdleTime, "1000");
    command_line->AppendSwitchASCII(
        extensions::switches::kEventPageSuspendingTime, "1000");
  }

  // Loads the extension, which temporarily starts the lazy background page
  // to dispatch the onInstalled event. We wait until it shuts down again.
  const Extension* LoadExtensionAndWait(const std::string& test_name) {
    LazyBackgroundObserver page_complete;
    base::FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
        AppendASCII(test_name);
    const Extension* extension = LoadExtension(extdir);
    if (extension)
      page_complete.Wait();
    return extension;
  }

  // Returns true if the lazy background page for the extension with
  // |extension_id| is still running.
  bool IsBackgroundPageAlive(const std::string& extension_id) {
    extensions::ProcessManager* pm = extensions::ExtensionSystem::Get(
        browser()->profile())->process_manager();
    return pm->GetBackgroundHostForExtension(extension_id);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LazyBackgroundPageApiTest);
};

IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, BrowserActionCreateTab) {
  ASSERT_TRUE(LoadExtensionAndWait("browser_action_create_tab"));

  // Lazy Background Page doesn't exist yet.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));
  int num_tabs_before = browser()->tab_strip_model()->count();

  // Observe background page being created and closed after
  // the browser action is clicked.
  LazyBackgroundObserver page_complete;
  BrowserActionTestUtil(browser()).Press(0);
  page_complete.Wait();

  // Background page created a new tab before it closed.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));
  EXPECT_EQ(num_tabs_before + 1, browser()->tab_strip_model()->count());
  EXPECT_EQ(std::string(chrome::kChromeUIExtensionsURL),
            browser()->tab_strip_model()->GetActiveWebContents()->
                GetURL().spec());
}

IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest,
                       BrowserActionCreateTabAfterCallback) {
  ASSERT_TRUE(LoadExtensionAndWait("browser_action_with_callback"));

  // Lazy Background Page doesn't exist yet.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));
  int num_tabs_before = browser()->tab_strip_model()->count();

  // Observe background page being created and closed after
  // the browser action is clicked.
  LazyBackgroundObserver page_complete;
  BrowserActionTestUtil(browser()).Press(0);
  page_complete.Wait();

  // Background page is closed after creating a new tab.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));
  EXPECT_EQ(num_tabs_before + 1, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, BroadcastEvent) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  const Extension* extension = LoadExtensionAndWait("broadcast_event");
  ASSERT_TRUE(extension);

  // Lazy Background Page doesn't exist yet.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));
  int num_page_actions = browser()->window()->GetLocationBar()->
      GetLocationBarForTesting()->PageActionVisibleCount();

  // Open a tab to a URL that will trigger the page action to show.
  LazyBackgroundObserver page_complete;
  content::WindowedNotificationObserver page_action_changed(
      extensions::NOTIFICATION_EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.html"));
  page_complete.Wait();

  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));

  // Page action is shown.
  page_action_changed.Wait();
  EXPECT_EQ(num_page_actions + 1,
            browser()->window()->GetLocationBar()->
                GetLocationBarForTesting()->PageActionVisibleCount());
}

IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, Filters) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  const Extension* extension = LoadExtensionAndWait("filters");
  ASSERT_TRUE(extension);

  // Lazy Background Page doesn't exist yet.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));

  // Open a tab to a URL that will fire a webNavigation event.
  LazyBackgroundObserver page_complete;
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.html"));
  page_complete.Wait();
}

// Tests that the lazy background page receives the onInstalled event and shuts
// down.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, OnInstalled) {
  ResultCatcher catcher;
  ASSERT_TRUE(LoadExtensionAndWait("on_installed"));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  // Lazy Background Page has been shut down.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));
}

// Tests that a JavaScript alert keeps the lazy background page alive.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, WaitForDialog) {
  LazyBackgroundObserver background_observer;
  base::FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
      AppendASCII("wait_for_dialog");
  const Extension* extension = LoadExtension(extdir);
  ASSERT_TRUE(extension);

  // The test extension opens a dialog on installation.
  AppModalDialog* dialog = ui_test_utils::WaitForAppModalDialog();
  ASSERT_TRUE(dialog);

  // With the dialog open the background page is still alive.
  EXPECT_TRUE(IsBackgroundPageAlive(extension->id()));

  // Close the dialog. The keep alive count is decremented.
  extensions::ProcessManager* pm =
      extensions::ExtensionSystem::Get(browser()->profile())->process_manager();
  int previous_keep_alive_count = pm->GetLazyKeepaliveCount(extension);
  dialog->CloseModalDialog();
  EXPECT_EQ(previous_keep_alive_count - 1,
            pm->GetLazyKeepaliveCount(extension));

  // The background page closes now that the dialog is gone.
  background_observer.WaitUntilClosed();
  EXPECT_FALSE(IsBackgroundPageAlive(extension->id()));
}

// Tests that the lazy background page stays alive until all visible views are
// closed.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, WaitForView) {
  LazyBackgroundObserver page_complete;
  ResultCatcher catcher;
  base::FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
      AppendASCII("wait_for_view");
  const Extension* extension = LoadExtension(extdir);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  // The extension should've opened a new tab to an extension page.
  EXPECT_EQ(extension->GetResourceURL("extension_page.html").spec(),
            browser()->tab_strip_model()->GetActiveWebContents()->
                GetURL().spec());

  // Lazy Background Page still exists, because the extension created a new tab
  // to an extension page.
  EXPECT_TRUE(IsBackgroundPageAlive(last_loaded_extension_id()));

  // Close the new tab.
  browser()->tab_strip_model()->CloseWebContentsAt(
      browser()->tab_strip_model()->active_index(), TabStripModel::CLOSE_NONE);
  page_complete.Wait();

  // Lazy Background Page has been shut down.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));
}

// Tests that the lazy background page stays alive until all network requests
// are complete.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, WaitForRequest) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(StartEmbeddedTestServer());

  LazyBackgroundObserver page_complete;
  ResultCatcher catcher;
  base::FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
      AppendASCII("wait_for_request");
  const Extension* extension = LoadExtension(extdir);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  // Lazy Background Page still exists, because the extension started a request.
  extensions::ProcessManager* pm =
      extensions::ExtensionSystem::Get(browser()->profile())->process_manager();
  extensions::ExtensionHost* host =
      pm->GetBackgroundHostForExtension(last_loaded_extension_id());
  ASSERT_TRUE(host);

  // Abort the request.
  bool result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      host->render_view_host(), "abortRequest()", &result));
  EXPECT_TRUE(result);
  page_complete.Wait();

  // Lazy Background Page has been shut down.
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id()));
}

// Tests that the lazy background page stays alive until all visible views are
// closed.
// http://crbug.com/175778; test fails frequently on OS X
#if defined(OS_MACOSX)
#define MAYBE_WaitForNTP DISABLED_WaitForNTP
#else
#define MAYBE_WaitForNTP WaitForNTP
#endif
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, MAYBE_WaitForNTP) {
  LazyBackgroundObserver lazybg;
  ResultCatcher catcher;
  base::FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
      AppendASCII("wait_for_ntp");
  const Extension* extension = LoadExtension(extdir);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  // The extension should've opened a new tab to an extension page.
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  // Lazy Background Page still exists, because the extension created a new tab
  // to an extension page.
  EXPECT_TRUE(IsBackgroundPageAlive(last_loaded_extension_id()));

  // Navigate away from the NTP, which should close the event page.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  lazybg.Wait();

  // Lazy Background Page has been shut down.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));
}

// Tests that an incognito split mode extension gets 2 lazy background pages,
// and they each load and unload at the proper times.
// See crbug.com/248437
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, DISABLED_IncognitoSplitMode) {
  // Open incognito window.
  Browser* incognito_browser = ui_test_utils::OpenURLOffTheRecord(
      browser()->profile(), GURL("about:blank"));

  // Load the extension with incognito enabled.
  {
    LoadedIncognitoObserver loaded(browser()->profile());
    base::FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
        AppendASCII("incognito_split");
    ASSERT_TRUE(LoadExtensionIncognito(extdir));
    loaded.Wait();
  }

  // Lazy Background Page doesn't exist yet.
  extensions::ProcessManager* pm =
      extensions::ExtensionSystem::Get(browser()->profile())->process_manager();
  extensions::ProcessManager* pmi =
      extensions::ExtensionSystem::Get(incognito_browser->profile())->
          process_manager();
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id()));
  EXPECT_FALSE(pmi->GetBackgroundHostForExtension(last_loaded_extension_id()));

  // Trigger a browserAction event in the original profile and ensure only
  // the original event page received it (since the event is scoped to the
  // profile).
  {
    ExtensionTestMessageListener listener("waiting", false);
    ExtensionTestMessageListener listener_incognito("waiting_incognito", false);

    LazyBackgroundObserver page_complete(browser()->profile());
    BrowserActionTestUtil(browser()).Press(0);
    page_complete.Wait();

    // Only the original event page received the message.
    EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id()));
    EXPECT_FALSE(
        pmi->GetBackgroundHostForExtension(last_loaded_extension_id()));
    EXPECT_TRUE(listener.was_satisfied());
    EXPECT_FALSE(listener_incognito.was_satisfied());
  }

  // Trigger a bookmark created event and ensure both pages receive it.
  {
    ExtensionTestMessageListener listener("waiting", false);
    ExtensionTestMessageListener listener_incognito("waiting_incognito", false);

    LazyBackgroundObserver page_complete(browser()->profile()),
                           page2_complete(incognito_browser->profile());
    BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForProfile(browser()->profile());
    test::WaitForBookmarkModelToLoad(bookmark_model);
    const BookmarkNode* parent = bookmark_model->bookmark_bar_node();
    bookmark_model->AddURL(
        parent, 0, base::ASCIIToUTF16("Title"), GURL("about:blank"));
    page_complete.Wait();
    page2_complete.Wait();

    // Both pages received the message.
    EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id()));
    EXPECT_FALSE(
        pmi->GetBackgroundHostForExtension(last_loaded_extension_id()));
    EXPECT_TRUE(listener.was_satisfied());
    EXPECT_TRUE(listener_incognito.was_satisfied());
  }
}

// Tests that messages from the content script activate the lazy background
// page, and keep it alive until all channels are closed.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, Messaging) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(LoadExtensionAndWait("messaging"));

  // Lazy Background Page doesn't exist yet.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Navigate to a page that opens a message channel to the background page.
  ResultCatcher catcher;
  LazyBackgroundObserver lazybg;
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.html"));
  lazybg.WaitUntilLoaded();

  // Background page got the content script's message and is still loaded
  // until we close the channel.
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(IsBackgroundPageAlive(last_loaded_extension_id()));

  // Navigate away, closing the message channel and therefore the background
  // page.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  lazybg.WaitUntilClosed();

  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));
}

// Tests that a KeepaliveImpulse increments the keep alive count, but eventually
// times out and background page will still close.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, ImpulseAddsCount) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const Extension* extension = LoadExtensionAndWait("messaging");
  ASSERT_TRUE(extension);

  // Lazy Background Page doesn't exist yet.
  extensions::ProcessManager* pm =
      extensions::ExtensionSystem::Get(browser()->profile())->process_manager();
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id()));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Navigate to a page that opens a message channel to the background page.
  ResultCatcher catcher;
  LazyBackgroundObserver lazybg;
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.html"));
  lazybg.WaitUntilLoaded();

  // Add an impulse and the keep alive count increases.
  int previous_keep_alive_count = pm->GetLazyKeepaliveCount(extension);
  pm->KeepaliveImpulse(extension);
  EXPECT_EQ(previous_keep_alive_count + 1,
            pm->GetLazyKeepaliveCount(extension));

  // Navigate away, closing the message channel and therefore the background
  // page after the impulse times out.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  lazybg.WaitUntilClosed();

  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id()));
}

// Tests that the lazy background page receives the unload event when we
// close it, and that it can execute simple API calls that don't require an
// asynchronous response.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, OnUnload) {
  ASSERT_TRUE(LoadExtensionAndWait("on_unload"));

  // Lazy Background Page has been shut down.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));

  // The browser action has a new title.
  BrowserActionTestUtil browser_action(browser());
  ASSERT_EQ(1, browser_action.NumberOfBrowserActions());
  EXPECT_EQ("Success", browser_action.GetTooltip(0));
}

// Tests that both a regular page and an event page will receive events when
// the event page is not loaded.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, EventDispatchToTab) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  const extensions::Extension* extension =
      LoadExtensionAndWait("event_dispatch_to_tab");

  ExtensionTestMessageListener page_ready("ready", true);
  GURL page_url = extension->GetResourceURL("page.html");
  ui_test_utils::NavigateToURL(browser(), page_url);
  EXPECT_TRUE(page_ready.WaitUntilSatisfied());

  // After the event is sent below, wait for the event page to have received
  // the event before proceeding with the test.  This allows the regular page
  // to test that the event page received the event, which makes the pass/fail
  // logic simpler.
  ExtensionTestMessageListener event_page_ready("ready", true);

  // Send an event by making a bookmark.
  BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForProfile(browser()->profile());
  test::WaitForBookmarkModelToLoad(bookmark_model);
  bookmarks::AddIfNotBookmarked(bookmark_model,
                                GURL("http://www.google.com"),
                                base::UTF8ToUTF16("Google"));

  EXPECT_TRUE(event_page_ready.WaitUntilSatisfied());

  page_ready.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Tests that the lazy background page updates the chrome://extensions page
// when it is destroyed.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, UpdateExtensionsPage) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIExtensionsURL));

  ResultCatcher catcher;
  base::FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
      AppendASCII("wait_for_view");
  const Extension* extension = LoadExtension(extdir);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  // The extension should've opened a new tab to an extension page.
  EXPECT_EQ(extension->GetResourceURL("extension_page.html").spec(),
            browser()->tab_strip_model()->GetActiveWebContents()->
                GetURL().spec());

  // Lazy Background Page still exists, because the extension created a new tab
  // to an extension page.
  EXPECT_TRUE(IsBackgroundPageAlive(last_loaded_extension_id()));

  // Close the new tab.
  LazyBackgroundObserver page_complete;
  browser()->tab_strip_model()->CloseWebContentsAt(
      browser()->tab_strip_model()->active_index(), TabStripModel::CLOSE_NONE);
  page_complete.WaitUntilClosed();

  // Lazy Background Page has been shut down.
  EXPECT_FALSE(IsBackgroundPageAlive(last_loaded_extension_id()));

  // Verify that extensions page shows that the lazy background page is
  // inactive.
  content::RenderFrameHost* frame = content::FrameMatchingPredicate(
      browser()->tab_strip_model()->GetActiveWebContents(),
      base::Bind(&content::FrameHasSourceUrl,
                 GURL(chrome::kChromeUIExtensionsFrameURL)));
  bool is_inactive;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      frame,
      "var ele = document.querySelectorAll('div.active-views');"
      "window.domAutomationController.send("
      "    ele[0].innerHTML.search('(Inactive)') > 0);",
      &is_inactive));
  EXPECT_TRUE(is_inactive);
}

// Tests that the lazy background page will be unloaded if the onSuspend event
// handler calls an API function such as chrome.storage.local.set().
// See: http://crbug.com/296834
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, OnSuspendUseStorageApi) {
  EXPECT_TRUE(LoadExtensionAndWait("on_suspend"));
}

// TODO: background page with timer.
// TODO: background page that interacts with popup.
