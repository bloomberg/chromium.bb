// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/lazy_background_page_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "googleurl/src/gurl.h"
#include "net/base/mock_host_resolver.h"

using extensions::Extension;

namespace {

// This unfortunate bit of silliness is necessary when loading an extension in
// incognito. The goal is to load the extension, enable incognito, then wait
// for both background pages to load and close. The problem is that enabling
// incognito involves reloading the extension - and the background pages may
// have already loaded once before then. So we wait until the extension is
// unloaded before listening to the background page notifications.
class LoadedIncognitoObserver : public content::NotificationObserver {
 public:
  explicit LoadedIncognitoObserver(Profile* profile) : profile_(profile) {
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                   content::Source<Profile>(profile));
  }

  void Wait() {
    ASSERT_TRUE(original_complete_.get());
    original_complete_->Wait();
    incognito_complete_->Wait();
  }

 private:

  virtual void Observe(
      int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) {
    original_complete_.reset(new LazyBackgroundObserver(profile_));
    incognito_complete_.reset(
        new LazyBackgroundObserver(profile_->GetOffTheRecordProfile()));
  }

  Profile* profile_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<LazyBackgroundObserver> original_complete_;
  scoped_ptr<LazyBackgroundObserver> incognito_complete_;
};

}  // namespace

class LazyBackgroundPageApiTest : public ExtensionApiTest {
 public:
  void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
    // Set shorter delays to prevent test timeouts.
    command_line->AppendSwitchASCII(switches::kEventPageIdleTime, "1");
    command_line->AppendSwitchASCII(switches::kEventPageUnloadingTime, "1");
  }

  // Loads the extension, which temporarily starts the lazy background page
  // to dispatch the onInstalled event. We wait until it shuts down again.
  const Extension* LoadExtensionAndWait(const std::string& test_name) {
    LazyBackgroundObserver page_complete;
    FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
        AppendASCII(test_name);
    const Extension* extension = LoadExtension(extdir);
    if (extension)
      page_complete.Wait();
    return extension;
  }
};

IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, BrowserActionCreateTab) {
  ASSERT_TRUE(LoadExtensionAndWait("browser_action_create_tab"));

  // Lazy Background Page doesn't exist yet.
  ExtensionProcessManager* pm =
      extensions::ExtensionSystem::Get(browser()->profile())->process_manager();
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
  int num_tabs_before = browser()->tab_strip_model()->count();

  // Observe background page being created and closed after
  // the browser action is clicked.
  LazyBackgroundObserver page_complete;
  BrowserActionTestUtil(browser()).Press(0);
  page_complete.Wait();

  // Background page created a new tab before it closed.
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
  EXPECT_EQ(num_tabs_before + 1, browser()->tab_strip_model()->count());
  EXPECT_EQ(std::string(chrome::kChromeUIExtensionsURL),
            browser()->tab_strip_model()->GetActiveWebContents()->
                GetURL().spec());
}

IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest,
                       BrowserActionCreateTabAfterCallback) {
  ASSERT_TRUE(LoadExtensionAndWait("browser_action_with_callback"));

  // Lazy Background Page doesn't exist yet.
  ExtensionProcessManager* pm =
      extensions::ExtensionSystem::Get(browser()->profile())->process_manager();
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
  int num_tabs_before = browser()->tab_strip_model()->count();

  // Observe background page being created and closed after
  // the browser action is clicked.
  LazyBackgroundObserver page_complete;
  BrowserActionTestUtil(browser()).Press(0);
  page_complete.Wait();

  // Background page is closed after creating a new tab.
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
  EXPECT_EQ(num_tabs_before + 1, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, BroadcastEvent) {
  ASSERT_TRUE(StartTestServer());

  const Extension* extension = LoadExtensionAndWait("broadcast_event");
  ASSERT_TRUE(extension);

  // Lazy Background Page doesn't exist yet.
  ExtensionProcessManager* pm =
      extensions::ExtensionSystem::Get(browser()->profile())->process_manager();
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
  int num_page_actions = browser()->window()->GetLocationBar()->
      GetLocationBarForTesting()->PageActionVisibleCount();

  // Open a tab to a URL that will trigger the page action to show.
  LazyBackgroundObserver page_complete;
  content::WindowedNotificationObserver page_action_changed(
        chrome::NOTIFICATION_EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED,
        content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(
      browser(), test_server()->GetURL("files/extensions/test_file.html"));
  page_complete.Wait();

  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));

  // Page action is shown.
  page_action_changed.Wait();
  EXPECT_EQ(num_page_actions + 1,
            browser()->window()->GetLocationBar()->
                GetLocationBarForTesting()->PageActionVisibleCount());
}

IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, Filters) {
  const Extension* extension = LoadExtensionAndWait("filters");
  ASSERT_TRUE(extension);

  // Lazy Background Page doesn't exist yet.
  ExtensionProcessManager* pm =
      extensions::ExtensionSystem::Get(browser()->profile())->process_manager();
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));

  // Open a tab to a URL that will fire a webNavigation event.
  LazyBackgroundObserver page_complete;
  ui_test_utils::NavigateToURL(
      browser(), test_server()->GetURL("files/extensions/test_file.html"));
  page_complete.Wait();
}

// Tests that the lazy background page receives the onInstalled event and shuts
// down.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, OnInstalled) {
  ResultCatcher catcher;
  ASSERT_TRUE(LoadExtensionAndWait("on_installed"));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  // Lazy Background Page has been shut down.
  ExtensionProcessManager* pm =
      extensions::ExtensionSystem::Get(browser()->profile())->process_manager();
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
}

// Tests that the lazy background page stays alive until all visible views are
// closed.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, WaitForView) {
  LazyBackgroundObserver page_complete;
  ResultCatcher catcher;
  FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
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
  ExtensionProcessManager* pm =
      extensions::ExtensionSystem::Get(browser()->profile())->process_manager();
  EXPECT_TRUE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));

  // Close the new tab.
  browser()->tab_strip_model()->CloseWebContentsAt(
      browser()->tab_strip_model()->active_index(), TabStripModel::CLOSE_NONE);
  page_complete.Wait();

  // Lazy Background Page has been shut down.
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
}

// Tests that the lazy background page stays alive until all network requests
// are complete.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, WaitForRequest) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(StartTestServer());

  LazyBackgroundObserver page_complete;
  ResultCatcher catcher;
  FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
      AppendASCII("wait_for_request");
  const Extension* extension = LoadExtension(extdir);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  // Lazy Background Page still exists, because the extension started a request.
  ExtensionProcessManager* pm =
      extensions::ExtensionSystem::Get(browser()->profile())->process_manager();
  extensions::ExtensionHost* host =
      pm->GetBackgroundHostForExtension(last_loaded_extension_id_);
  ASSERT_TRUE(host);

  // Abort the request.
  bool result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      host->render_view_host(), "abortRequest()", &result));
  EXPECT_TRUE(result);
  page_complete.Wait();

  // Lazy Background Page has been shut down.
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
}

// Tests that the lazy background page stays alive until all visible views are
// closed.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, WaitForNTP) {
  LazyBackgroundObserver lazybg;
  ResultCatcher catcher;
  FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
      AppendASCII("wait_for_ntp");
  const Extension* extension = LoadExtension(extdir);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  // The extension should've opened a new tab to an extension page.
  EXPECT_EQ(std::string(chrome::kChromeUINewTabURL),
            browser()->tab_strip_model()->GetActiveWebContents()->
                GetURL().spec());

  // Lazy Background Page still exists, because the extension created a new tab
  // to an extension page.
  ExtensionProcessManager* pm =
      extensions::ExtensionSystem::Get(browser()->profile())->process_manager();
  EXPECT_TRUE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));

  // Navigate away from the NTP, which should close the event page.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  lazybg.Wait();

  // Lazy Background Page has been shut down.
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
}

// Tests that an incognito split mode extension gets 2 lazy background pages,
// and they each load and unload at the proper times.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, IncognitoSplitMode) {
  // Open incognito window.
  Browser* incognito_browser = ui_test_utils::OpenURLOffTheRecord(
      browser()->profile(), GURL("about:blank"));

  // Load the extension with incognito enabled.
  {
    LoadedIncognitoObserver loaded(browser()->profile());
    FilePath extdir = test_data_dir_.AppendASCII("lazy_background_page").
        AppendASCII("incognito_split");
    ASSERT_TRUE(LoadExtensionIncognito(extdir));
    loaded.Wait();
  }

  // Lazy Background Page doesn't exist yet.
  ExtensionProcessManager* pm =
      extensions::ExtensionSystem::Get(browser()->profile())->process_manager();
  ExtensionProcessManager* pmi =
      extensions::ExtensionSystem::Get(incognito_browser->profile())->
          process_manager();
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
  EXPECT_FALSE(pmi->GetBackgroundHostForExtension(last_loaded_extension_id_));

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
    EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
    EXPECT_FALSE(pmi->GetBackgroundHostForExtension(last_loaded_extension_id_));
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
    ui_test_utils::WaitForBookmarkModelToLoad(bookmark_model);
    const BookmarkNode* parent = bookmark_model->bookmark_bar_node();
    bookmark_model->AddURL(
        parent, 0, ASCIIToUTF16("Title"), GURL("about:blank"));
    page_complete.Wait();
    page2_complete.Wait();

    // Both pages received the message.
    EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
    EXPECT_FALSE(pmi->GetBackgroundHostForExtension(last_loaded_extension_id_));
    EXPECT_TRUE(listener.was_satisfied());
    EXPECT_TRUE(listener_incognito.was_satisfied());
  }
}

// Tests that messages from the content script activate the lazy background
// page, and keep it alive until all channels are closed.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, Messaging) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(LoadExtensionAndWait("messaging"));

  // Lazy Background Page doesn't exist yet.
  ExtensionProcessManager* pm =
      extensions::ExtensionSystem::Get(browser()->profile())->process_manager();
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Navigate to a page that opens a message channel to the background page.
  ResultCatcher catcher;
  LazyBackgroundObserver lazybg;
  ui_test_utils::NavigateToURL(
      browser(), test_server()->GetURL("files/extensions/test_file.html"));
  lazybg.WaitUntilLoaded();

  // Background page got the content script's message and is still loaded
  // until we close the channel.
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));

  // Navigate away, closing the message channel and therefore the background
  // page.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  lazybg.WaitUntilClosed();

  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));
}

// Tests that the lazy background page receives the unload event when we
// close it, and that it can execute simple API calls that don't require an
// asynchronous response.
IN_PROC_BROWSER_TEST_F(LazyBackgroundPageApiTest, OnUnload) {
  ASSERT_TRUE(LoadExtensionAndWait("on_unload"));

  // Lazy Background Page has been shut down.
  ExtensionProcessManager* pm =
      extensions::ExtensionSystem::Get(browser()->profile())->process_manager();
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id_));

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
  ui_test_utils::WaitForBookmarkModelToLoad(bookmark_model);
  bookmark_utils::AddIfNotBookmarked(
      bookmark_model,
      GURL("http://www.google.com"),
      UTF8ToUTF16("Google"));

  EXPECT_TRUE(event_page_ready.WaitUntilSatisfied());

  page_ready.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// TODO: background page with timer.
// TODO: background page that interacts with popup.
