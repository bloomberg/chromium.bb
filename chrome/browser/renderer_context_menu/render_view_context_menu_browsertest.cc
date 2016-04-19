// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/base/load_flags.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "third_party/WebKit/public/web/WebContextMenuData.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/base/models/menu_model.h"

using content::WebContents;

namespace {

class ContextMenuBrowserTest : public InProcessBrowserTest {
 public:
  ContextMenuBrowserTest() {}

 protected:
  std::unique_ptr<TestRenderViewContextMenu> CreateContextMenuMediaTypeNone(
      const GURL& unfiltered_url,
      const GURL& url) {
    return CreateContextMenu(unfiltered_url, url, base::string16(),
                             blink::WebContextMenuData::MediaTypeNone,
                             ui::MENU_SOURCE_NONE);
  }

  std::unique_ptr<TestRenderViewContextMenu> CreateContextMenuMediaTypeImage(
      const GURL& url) {
    return CreateContextMenu(GURL(), url, base::string16(),
                             blink::WebContextMenuData::MediaTypeImage,
                             ui::MENU_SOURCE_NONE);
  }

  std::unique_ptr<TestRenderViewContextMenu> CreateContextMenu(
      const GURL& unfiltered_url,
      const GURL& url,
      const base::string16& link_text,
      blink::WebContextMenuData::MediaType media_type,
      ui::MenuSourceType source_type) {
    content::ContextMenuParams params;
    params.media_type = media_type;
    params.unfiltered_link_url = unfiltered_url;
    params.link_url = url;
    params.src_url = url;
    params.link_text = link_text;
    WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    params.page_url = web_contents->GetController().GetActiveEntry()->GetURL();
    params.source_type = source_type;
#if defined(OS_MACOSX)
    params.writing_direction_default = 0;
    params.writing_direction_left_to_right = 0;
    params.writing_direction_right_to_left = 0;
#endif  // OS_MACOSX
    std::unique_ptr<TestRenderViewContextMenu> menu(
        new TestRenderViewContextMenu(web_contents->GetMainFrame(), params));
    menu->Init();
    return menu;
  }

  // Does not work on ChromeOS.
  Profile* CreateSecondaryProfile(int profile_num) {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath profile_path = profile_manager->user_data_dir();
    profile_path = profile_path.AppendASCII(
        base::StringPrintf("New Profile %d", profile_num));
    return profile_manager->GetProfile(profile_path);
  }
};

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       OpenEntryPresentForNormalURLs) {
  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeNone(GURL("http://www.google.com/"),
                                     GURL("http://www.google.com/"));

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE));
  ASSERT_FALSE(menu->IsItemInRangePresent(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                          IDC_OPEN_LINK_IN_PROFILE_LAST));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       OpenEntryAbsentForFilteredURLs) {
  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeNone(GURL("chrome://history"), GURL());

  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE));
  ASSERT_FALSE(menu->IsItemInRangePresent(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                          IDC_OPEN_LINK_IN_PROFILE_LAST));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, ContextMenuForCanvas) {
  content::ContextMenuParams params;
  params.media_type = blink::WebContextMenuData::MediaTypeCanvas;

  TestRenderViewContextMenu menu(
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      params);
  menu.Init();

  ASSERT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_SAVEIMAGEAS));
  ASSERT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_COPYIMAGE));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, CopyLinkTextMouse) {
  std::unique_ptr<TestRenderViewContextMenu> menu = CreateContextMenu(
      GURL("http://www.google.com/"), GURL("http://www.google.com/"),
      base::ASCIIToUTF16("Google"), blink::WebContextMenuData::MediaTypeNone,
      ui::MENU_SOURCE_MOUSE);

  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKTEXT));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, CopyLinkTextTouchNoText) {
  std::unique_ptr<TestRenderViewContextMenu> menu = CreateContextMenu(
      GURL("http://www.google.com/"), GURL("http://www.google.com/"),
      base::ASCIIToUTF16(""), blink::WebContextMenuData::MediaTypeNone,
      ui::MENU_SOURCE_TOUCH);

  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKTEXT));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, CopyLinkTextTouchTextOnly) {
  std::unique_ptr<TestRenderViewContextMenu> menu = CreateContextMenu(
      GURL("http://www.google.com/"), GURL("http://www.google.com/"),
      base::ASCIIToUTF16("Google"), blink::WebContextMenuData::MediaTypeNone,
      ui::MENU_SOURCE_TOUCH);

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKTEXT));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, CopyLinkTextTouchTextImage) {
  std::unique_ptr<TestRenderViewContextMenu> menu = CreateContextMenu(
      GURL("http://www.google.com/"), GURL("http://www.google.com/"),
      base::ASCIIToUTF16("Google"), blink::WebContextMenuData::MediaTypeImage,
      ui::MENU_SOURCE_TOUCH);

  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKTEXT));
}

// Opens a link in a new tab via a "real" context menu.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, RealMenu) {
  ContextMenuNotificationObserver menu_observer(
      IDC_CONTENT_CONTEXT_OPENLINKNEWTAB);
  ui_test_utils::WindowedTabAddedNotificationObserver tab_observer(
      content::NotificationService::AllSources());

  // Go to a page with a link
  ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,<a href='about:blank'>link</a>"));

  // Open a context menu.
  blink::WebMouseEvent mouse_event;
  mouse_event.type = blink::WebInputEvent::MouseDown;
  mouse_event.button = blink::WebMouseEvent::ButtonRight;
  mouse_event.x = 15;
  mouse_event.y = 15;
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  gfx::Rect offset = tab->GetContainerBounds();
  mouse_event.globalX = 15 + offset.x();
  mouse_event.globalY = 15 + offset.y();
  mouse_event.clickCount = 1;
  tab->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(mouse_event);
  mouse_event.type = blink::WebInputEvent::MouseUp;
  tab->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(mouse_event);

  // The menu_observer will select "Open in new tab", wait for the new tab to
  // be added.
  tab_observer.Wait();
  tab = tab_observer.GetTab();
  content::WaitForLoadStop(tab);

  // Verify that it's the correct tab.
  EXPECT_EQ(GURL("about:blank"), tab->GetURL());
}

// Verify that "Open Link in New Tab" doesn't send URL fragment as referrer.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, OpenInNewTabReferrer) {
  ui_test_utils::WindowedTabAddedNotificationObserver tab_observer(
      content::NotificationService::AllSources());

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL echoheader(embedded_test_server()->GetURL("/echoheader?Referer"));

  // Go to a |page| with a link to echoheader URL.
  GURL page("data:text/html,<a href='" + echoheader.spec() + "'>link</a>");
  ui_test_utils::NavigateToURL(browser(), page);

  // Set up referrer URL with fragment.
  const GURL kReferrerWithFragment("http://foo.com/test#fragment");
  const std::string kCorrectReferrer("http://foo.com/test");

  // Set up menu with link URL.
  content::ContextMenuParams context_menu_params;
  context_menu_params.page_url = kReferrerWithFragment;
  context_menu_params.link_url = echoheader;

  // Select "Open Link in New Tab" and wait for the new tab to be added.
  TestRenderViewContextMenu menu(
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);

  tab_observer.Wait();
  content::WebContents* tab = tab_observer.GetTab();
  content::WaitForLoadStop(tab);

  // Verify that it's the correct tab.
  ASSERT_EQ(echoheader, tab->GetURL());
  // Verify that the text on the page matches |kCorrectReferrer|.
  std::string actual_referrer;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      tab,
      "window.domAutomationController.send(window.document.body.textContent);",
      &actual_referrer));
  ASSERT_EQ(kCorrectReferrer, actual_referrer);

  // Verify that the referrer on the page matches |kCorrectReferrer|.
  std::string page_referrer;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      tab,
      "window.domAutomationController.send(window.document.referrer);",
      &page_referrer));
  ASSERT_EQ(kCorrectReferrer, page_referrer);
}

// Verify that "Open Link in Incognito Window " doesn't send referrer URL.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, OpenIncognitoNoneReferrer) {
  ui_test_utils::WindowedTabAddedNotificationObserver tab_observer(
      content::NotificationService::AllSources());

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL echoheader(embedded_test_server()->GetURL("/echoheader?Referer"));

  // Go to a |page| with a link to echoheader URL.
  GURL page("data:text/html,<a href='" + echoheader.spec() + "'>link</a>");
  ui_test_utils::NavigateToURL(browser(), page);

  // Set up referrer URL with fragment.
  const GURL kReferrerWithFragment("http://foo.com/test#fragment");
  const std::string kNoneReferrer("None");
  const std::string kEmptyReferrer("");

  // Set up menu with link URL.
  content::ContextMenuParams context_menu_params;
  context_menu_params.page_url = kReferrerWithFragment;
  context_menu_params.link_url = echoheader;

  // Select "Open Link in Incognito Window" and wait for window to be added.
  TestRenderViewContextMenu menu(
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD, 0);

  tab_observer.Wait();
  content::WebContents* tab = tab_observer.GetTab();
  content::WaitForLoadStop(tab);

  // Verify that it's the correct tab.
  ASSERT_EQ(echoheader, tab->GetURL());
  // Verify that the text on the page matches |kNoneReferrer|.
  std::string actual_referrer;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      tab,
      "window.domAutomationController.send(window.document.body.textContent);",
      &actual_referrer));
  ASSERT_EQ(kNoneReferrer, actual_referrer);

  // Verify that the referrer on the page matches |kEmptyReferrer|.
  std::string page_referrer;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      tab,
      "window.domAutomationController.send(window.document.referrer);",
      &page_referrer));
  ASSERT_EQ(kEmptyReferrer, page_referrer);
}

// Check filename on clicking "Save Link As" via a "real" context menu.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, SuggestedFileName) {
  // Register observer.
  ContextMenuWaiter menu_observer(content::NotificationService::AllSources());

  // Go to a page with a link having download attribute.
  const std::string kSuggestedFilename("test_filename.png");
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("data:text/html,<a href='about:blank' download='" +
           kSuggestedFilename + "'>link</a>"));

  // Open a context menu.
  blink::WebMouseEvent mouse_event;
  mouse_event.type = blink::WebInputEvent::MouseDown;
  mouse_event.button = blink::WebMouseEvent::ButtonRight;
  mouse_event.x = 15;
  mouse_event.y = 15;
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  tab->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(mouse_event);
  mouse_event.type = blink::WebInputEvent::MouseUp;
  tab->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(mouse_event);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // Compare filename.
  base::string16 suggested_filename = menu_observer.params().suggested_filename;
  ASSERT_EQ(kSuggestedFilename, base::UTF16ToUTF8(suggested_filename).c_str());
}

// Ensure that View Page Info won't crash if there is no visible entry.
// See http://crbug.com/370863.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, ViewPageInfoWithNoEntry) {
  // Create a new tab with no committed entry.
  ui_test_utils::WindowedTabAddedNotificationObserver tab_observer(
      content::NotificationService::AllSources());
  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(), "window.open();"));
  tab_observer.Wait();
  content::WebContents* tab = tab_observer.GetTab();
  EXPECT_FALSE(tab->GetController().GetLastCommittedEntry());
  EXPECT_FALSE(tab->GetController().GetVisibleEntry());

  // Create a context menu.
  content::ContextMenuParams context_menu_params;
  TestRenderViewContextMenu menu(tab->GetMainFrame(), context_menu_params);
  menu.Init();

  // The item shouldn't be enabled in the menu.
  EXPECT_FALSE(menu.IsCommandIdEnabled(IDC_CONTENT_CONTEXT_VIEWPAGEINFO));

  // Ensure that viewing page info doesn't crash even if you can get to it.
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_VIEWPAGEINFO, 0);
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, DataSaverOpenOrigImageInNewTab) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(
      data_reduction_proxy::switches::kEnableDataReductionProxy);

  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeImage(GURL("http://url.com/image.png"));

  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB));
  ASSERT_TRUE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_OPEN_ORIGINAL_IMAGE_NEW_TAB));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       DataSaverHttpsOpenImageInNewTab) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(
      data_reduction_proxy::switches::kEnableDataReductionProxy);

  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeImage(GURL("https://url.com/image.png"));

  ASSERT_FALSE(
      menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPEN_ORIGINAL_IMAGE_NEW_TAB));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, OpenImageInNewTab) {
  std::unique_ptr<TestRenderViewContextMenu> menu =
      CreateContextMenuMediaTypeImage(GURL("http://url.com/image.png"));
  ASSERT_FALSE(
      menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPEN_ORIGINAL_IMAGE_NEW_TAB));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB));
}

// Functionality is not present on ChromeOS.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, OpenLinkInProfileEntryPresent) {
  {
    std::unique_ptr<TestRenderViewContextMenu> menu(
        CreateContextMenuMediaTypeNone(GURL("http://www.google.com/"),
                                       GURL("http://www.google.com/")));

    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
    // With only one profile exists, we don't add any items to the context menu
    // for opening links in other profiles.
    ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE));
    ASSERT_FALSE(menu->IsItemInRangePresent(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                            IDC_OPEN_LINK_IN_PROFILE_LAST));
  }

  // Create one additional profile, but do not yet open windows in it.
  CreateSecondaryProfile(1);

  {
    std::unique_ptr<TestRenderViewContextMenu> menu(
        CreateContextMenuMediaTypeNone(GURL("http://www.google.com/"),
                                       GURL("http://www.google.com/")));

    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
    // With two profiles (the current and another profile), no submenu is
    // created. Instead, a single item is added to the main context menu.
    ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE));
    ASSERT_TRUE(menu->IsItemPresent(IDC_OPEN_LINK_IN_PROFILE_FIRST));
  }

  CreateSecondaryProfile(2);

  {
    std::unique_ptr<TestRenderViewContextMenu> menu(
        CreateContextMenuMediaTypeNone(GURL("http://www.google.com/"),
                                       GURL("http://www.google.com/")));

    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
    ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
    // As soon as at least three profiles exist, we show all profiles in a
    // submenu.
    ui::MenuModel* model = NULL;
    int index = -1;
    ASSERT_TRUE(menu->GetMenuModelAndItemIndex(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                               &model, &index));
    ASSERT_EQ(2, model->GetItemCount());
    ASSERT_FALSE(menu->IsItemInRangePresent(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                            IDC_OPEN_LINK_IN_PROFILE_LAST));
  }
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, OpenLinkInProfile) {
  // Create |num_profiles| extra profiles for testing.
  const int num_profiles = 8;
  // The following are the profile numbers that are omitted and need signin.
  // These profiles are not added to the menu. Omitted profiles refers to
  // supervised profiles in the process of creation.
  std::vector<int> profiles_omit;
  profiles_omit.push_back(4);

  std::vector<int> profiles_signin_required;
  profiles_signin_required.push_back(1);
  profiles_signin_required.push_back(3);
  profiles_signin_required.push_back(6);

  // Create the profiles.
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  std::vector<Profile*> profiles_in_menu;
  for (int i = 0; i < num_profiles; ++i) {
    Profile* profile = CreateSecondaryProfile(i);
    ProfileAttributesEntry* entry;
    ASSERT_TRUE(storage.GetProfileAttributesWithPath(profile->GetPath(),
                                                     &entry));
    // Open a browser window for the profile if and only if the profile is not
    // omitted nor needing signin.
    if (std::binary_search(profiles_omit.begin(), profiles_omit.end(), i)) {
      entry->SetIsOmitted(true);
    } else if (std::binary_search(profiles_signin_required.begin(),
                                  profiles_signin_required.end(), i)) {
      entry->SetIsSigninRequired(true);
    } else {
      profiles::FindOrCreateNewWindowForProfile(
          profile, chrome::startup::IS_NOT_PROCESS_STARTUP,
          chrome::startup::IS_NOT_FIRST_RUN, false);
      profiles_in_menu.push_back(profile);
    }
  }

  ui_test_utils::WindowedTabAddedNotificationObserver tab_observer(
      content::NotificationService::AllSources());

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/"));

  std::unique_ptr<TestRenderViewContextMenu> menu(
      CreateContextMenuMediaTypeNone(url, url));

  // Verify that the size of the menu is correct.
  ui::MenuModel* model = NULL;
  int index = -1;
  ASSERT_TRUE(menu->GetMenuModelAndItemIndex(IDC_OPEN_LINK_IN_PROFILE_FIRST,
                                             &model, &index));
  ASSERT_EQ(static_cast<int>(profiles_in_menu.size()), model->GetItemCount());

  // Open the menu items. They should match their corresponding profiles in
  // |profiles_in_menu|.
  for (Profile* profile : profiles_in_menu) {
    int command_id = menu->GetCommandIDByProfilePath(profile->GetPath());
    ASSERT_NE(-1, command_id);
    menu->ExecuteCommand(command_id, 0);

    tab_observer.Wait();
    content::WebContents* tab = tab_observer.GetTab();
    content::WaitForLoadStop(tab);

    // Verify that it's the correct tab and profile.
    EXPECT_EQ(url, tab->GetURL());
    EXPECT_EQ(profile, Profile::FromBrowserContext(tab->GetBrowserContext()));
  }
}
#endif  // !defined(OS_CHROMEOS)

class ThumbnailResponseWatcher : public content::NotificationObserver {
 public:
  enum QuitReason {
    STILL_RUNNING = 0,
    THUMBNAIL_RECEIVED,
    RENDER_PROCESS_GONE,
  };

  class MessageFilter : public content::BrowserMessageFilter {
   public:
    explicit MessageFilter(ThumbnailResponseWatcher* owner)
        : content::BrowserMessageFilter(ChromeMsgStart), owner_(owner) {}

    bool OnMessageReceived(const IPC::Message& message) override {
      if (message.type() ==
          ChromeViewHostMsg_RequestThumbnailForContextNode_ACK::ID) {
        content::BrowserThread::PostTask(
            content::BrowserThread::UI, FROM_HERE,
            base::Bind(&MessageFilter::OnRequestThumbnailForContextNodeACK,
                       this));
      }
      return false;
    }

    void OnRequestThumbnailForContextNodeACK() {
      if (owner_)
        owner_->OnRequestThumbnailForContextNodeACK();
    }

    void Disown() { owner_ = nullptr; }

   private:
    ~MessageFilter() override {}

    ThumbnailResponseWatcher* owner_;

    DISALLOW_COPY_AND_ASSIGN(MessageFilter);
  };

  explicit ThumbnailResponseWatcher(
      content::RenderProcessHost* render_process_host)
      : message_loop_runner_(new content::MessageLoopRunner),
        filter_(new MessageFilter(this)),
        quit_reason_(STILL_RUNNING) {
    notification_registrar_.Add(
        this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
        content::Source<content::RenderProcessHost>(render_process_host));
    notification_registrar_.Add(
        this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
        content::Source<content::RenderProcessHost>(render_process_host));
    render_process_host->AddFilter(filter_.get());
  }

  ~ThumbnailResponseWatcher() override { filter_->Disown(); }

  QuitReason Wait() WARN_UNUSED_RESULT {
    message_loop_runner_->Run();
    DCHECK_NE(STILL_RUNNING, quit_reason_);
    return quit_reason_;
  }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    DCHECK(type == content::NOTIFICATION_RENDERER_PROCESS_CLOSED ||
           type == content::NOTIFICATION_RENDERER_PROCESS_TERMINATED);
    quit_reason_ = RENDER_PROCESS_GONE;
    message_loop_runner_->Quit();
  }

  void OnRequestThumbnailForContextNodeACK() {
    quit_reason_ = THUMBNAIL_RECEIVED;
    message_loop_runner_->Quit();
  }

 private:
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  scoped_refptr<MessageFilter> filter_;
  content::NotificationRegistrar notification_registrar_;
  QuitReason quit_reason_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailResponseWatcher);
};

// Maintains image search test state. In particular, note that |menu_observer_|
// must live until the right-click completes asynchronously.
class SearchByImageBrowserTest : public InProcessBrowserTest {
 protected:
  void SetupAndLoadImagePage(const std::string& image_path) {
    // The test server must start first, so that we know the port that the test
    // server is using.
    ASSERT_TRUE(embedded_test_server()->Start());
    SetupImageSearchEngine();

    // Go to a page with an image in it. The test server doesn't serve the image
    // with the right MIME type, so use a data URL to make a page containing it.
    GURL image_url(embedded_test_server()->GetURL(image_path));
    GURL page("data:text/html,<img src='" + image_url.spec() + "'>");
    ui_test_utils::NavigateToURL(browser(), page);
  }

  void AttemptImageSearch() {
    // Right-click where the image should be.
    // |menu_observer_| will cause the search-by-image menu item to be clicked.
    menu_observer_.reset(new ContextMenuNotificationObserver(
        IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE));
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::SimulateMouseClickAt(tab, 0, blink::WebMouseEvent::ButtonRight,
                                  gfx::Point(15, 15));
  }

  GURL GetImageSearchURL() {
    static const char kImageSearchURL[] = "/imagesearch";
    return embedded_test_server()->GetURL(kImageSearchURL);
  }

 private:
  void SetupImageSearchEngine() {
    static const char kShortName[] = "test";
    static const char kSearchURL[] = "/search?q={searchTerms}";
    static const char kImageSearchPostParams[] =
        "thumb={google:imageThumbnail}";

    TemplateURLService* model =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(model);
    search_test_utils::WaitForTemplateURLServiceToLoad(model);
    ASSERT_TRUE(model->loaded());

    TemplateURLData data;
    data.SetShortName(base::ASCIIToUTF16(kShortName));
    data.SetKeyword(data.short_name());
    data.SetURL(embedded_test_server()->GetURL(kSearchURL).spec());
    data.image_url = GetImageSearchURL().spec();
    data.image_url_post_params = kImageSearchPostParams;

    // The model takes ownership of |template_url|.
    TemplateURL* template_url = new TemplateURL(data);
    ASSERT_TRUE(model->Add(template_url));
    model->SetUserSelectedDefaultSearchProvider(template_url);
  }

  void TearDownInProcessBrowserTestFixture() override {
    menu_observer_.reset();
  }

  std::unique_ptr<ContextMenuNotificationObserver> menu_observer_;
};

IN_PROC_BROWSER_TEST_F(SearchByImageBrowserTest, ImageSearchWithValidImage) {
  static const char kValidImage[] = "/image_search/valid.png";
  SetupAndLoadImagePage(kValidImage);

  ui_test_utils::WindowedTabAddedNotificationObserver tab_observer(
      content::NotificationService::AllSources());
  AttemptImageSearch();

  // The browser should open a new tab for an image search.
  tab_observer.Wait();
  content::WebContents* new_tab = tab_observer.GetTab();
  content::WaitForLoadStop(new_tab);
  EXPECT_EQ(GetImageSearchURL(), new_tab->GetURL());
}

IN_PROC_BROWSER_TEST_F(SearchByImageBrowserTest, ImageSearchWithCorruptImage) {
  static const char kCorruptImage[] = "/image_search/corrupt.png";
  SetupAndLoadImagePage(kCorruptImage);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ThumbnailResponseWatcher watcher(tab->GetRenderProcessHost());
  AttemptImageSearch();

  // The browser should receive a response from the renderer, because the
  // renderer should not crash.
  EXPECT_EQ(ThumbnailResponseWatcher::THUMBNAIL_RECEIVED, watcher.Wait());
}

class LoadImageRequestInterceptor : public net::URLRequestInterceptor {
 public:
  LoadImageRequestInterceptor() : num_requests_(0),
                                  requests_to_wait_for_(-1),
                                  weak_factory_(this) {
  }

  ~LoadImageRequestInterceptor() override {}

  // net::URLRequestInterceptor implementation
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    EXPECT_TRUE(request->load_flags() & net::LOAD_BYPASS_CACHE);
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&LoadImageRequestInterceptor::RequestCreated,
                   weak_factory_.GetWeakPtr()));
    return nullptr;
  }

  void WaitForRequests(int requests_to_wait_for) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK_EQ(-1, requests_to_wait_for_);
    DCHECK(!run_loop_);

    if (num_requests_ >= requests_to_wait_for)
      return;

    requests_to_wait_for_ = requests_to_wait_for;
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
    run_loop_.reset();
    requests_to_wait_for_ = -1;
    EXPECT_EQ(num_requests_, requests_to_wait_for);
  }

  // It is up to the caller to wait until all relevant requests has been
  // created, either through calling WaitForRequests or some other manner,
  // before calling this method.
  int num_requests() const {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    return num_requests_;
  }

 private:
  void RequestCreated() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    num_requests_++;
    if (num_requests_ == requests_to_wait_for_)
      run_loop_->Quit();
  }

  // These are only used on the UI thread.
  int num_requests_;
  int requests_to_wait_for_;
  std::unique_ptr<base::RunLoop> run_loop_;

  // This prevents any risk of flake if any test doesn't wait for a request
  // it sent.  Mutable so it can be accessed from a const function.
  mutable base::WeakPtrFactory<LoadImageRequestInterceptor> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LoadImageRequestInterceptor);
};

class LoadImageBrowserTest : public InProcessBrowserTest {
 protected:
  void SetupAndLoadImagePage(const std::string& image_path) {
    ASSERT_TRUE(embedded_test_server()->Start());
    // Go to a page with an image in it. The test server doesn't serve the image
    // with the right MIME type, so use a data URL to make a page containing it.
    GURL image_url(embedded_test_server()->GetURL(image_path));
    GURL page("data:text/html,<img src='" + image_url.spec() + "'>");
    ui_test_utils::NavigateToURL(browser(), page);
  }

  void AddLoadImageInterceptor(const std::string& image_path) {
    interceptor_ = new LoadImageRequestInterceptor();
    std::unique_ptr<net::URLRequestInterceptor> owned_interceptor(interceptor_);
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&LoadImageBrowserTest::AddInterceptorForURL,
                   base::Unretained(this),
                   GURL(embedded_test_server()->GetURL(image_path).spec()),
                   base::Passed(&owned_interceptor)));
  }

  void AttemptLoadImage() {
    // Right-click where the image should be.
    // |menu_observer_| will cause the "Load image" menu item to be clicked.
    menu_observer_.reset(new ContextMenuNotificationObserver(
        IDC_CONTENT_CONTEXT_LOAD_ORIGINAL_IMAGE));
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::SimulateMouseClickAt(tab, 0, blink::WebMouseEvent::ButtonRight,
                                  gfx::Point(15, 15));
  }

  void AddInterceptorForURL(
      const GURL& url,
      std::unique_ptr<net::URLRequestInterceptor> handler) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    net::URLRequestFilter::GetInstance()->AddUrlInterceptor(url,
                                                            std::move(handler));
  }

  LoadImageRequestInterceptor* interceptor_;

 private:
  std::unique_ptr<ContextMenuNotificationObserver> menu_observer_;
};

IN_PROC_BROWSER_TEST_F(LoadImageBrowserTest, LoadImage) {
  static const char kValidImage[] = "/load_image/image.png";
  SetupAndLoadImagePage(kValidImage);
  AddLoadImageInterceptor(kValidImage);
  AttemptLoadImage();
  interceptor_->WaitForRequests(1);
  EXPECT_EQ(1, interceptor_->num_requests());
}

}  // namespace
