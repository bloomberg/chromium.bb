// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/base/in_process_browser_test.h"
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
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "third_party/WebKit/public/web/WebContextMenuData.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using content::WebContents;

namespace {

class ContextMenuBrowserTest : public InProcessBrowserTest {
 public:
  ContextMenuBrowserTest() {}

  TestRenderViewContextMenu* CreateContextMenu(
      const GURL& unfiltered_url,
      const GURL& url,
      blink::WebContextMenuData::MediaType media_type) {
    content::ContextMenuParams params;
    params.media_type = media_type;
    params.unfiltered_link_url = unfiltered_url;
    params.link_url = url;
    params.src_url = url;
    WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    params.page_url = web_contents->GetController().GetActiveEntry()->GetURL();
#if defined(OS_MACOSX)
    params.writing_direction_default = 0;
    params.writing_direction_left_to_right = 0;
    params.writing_direction_right_to_left = 0;
#endif  // OS_MACOSX
    TestRenderViewContextMenu* menu = new TestRenderViewContextMenu(
        browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
        params);
    menu->Init();
    return menu;
  }

  TestRenderViewContextMenu* CreateContextMenuMediaTypeNone(
      const GURL& unfiltered_url,
      const GURL& url) {
    return CreateContextMenu(unfiltered_url, url,
                             blink::WebContextMenuData::MediaTypeNone);
  }
};

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       OpenEntryPresentForNormalURLs) {
  scoped_ptr<TestRenderViewContextMenu> menu(CreateContextMenuMediaTypeNone(
      GURL("http://www.google.com/"), GURL("http://www.google.com/")));

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       OpenEntryAbsentForFilteredURLs) {
  scoped_ptr<TestRenderViewContextMenu> menu(
      CreateContextMenuMediaTypeNone(GURL("chrome://history"), GURL()));

  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       ContextMenuForCanvas) {
  content::ContextMenuParams params;
  params.media_type = blink::WebContextMenuData::MediaTypeCanvas;

  TestRenderViewContextMenu menu(
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      params);
  menu.Init();

  ASSERT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_SAVEIMAGEAS));
  ASSERT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_COPYIMAGE));
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
  tab->GetRenderViewHost()->ForwardMouseEvent(mouse_event);
  mouse_event.type = blink::WebInputEvent::MouseUp;
  tab->GetRenderViewHost()->ForwardMouseEvent(mouse_event);

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

  ASSERT_TRUE(test_server()->Start());
  GURL echoheader(test_server()->GetURL("echoheader?Referer"));

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

  ASSERT_TRUE(test_server()->Start());
  GURL echoheader(test_server()->GetURL("echoheader?Referer"));

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
  SaveLinkAsContextMenuObserver menu_observer(
      content::NotificationService::AllSources());

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
  tab->GetRenderViewHost()->ForwardMouseEvent(mouse_event);
  mouse_event.type = blink::WebInputEvent::MouseUp;
  tab->GetRenderViewHost()->ForwardMouseEvent(mouse_event);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenu();

  // Compare filename.
  base::string16 suggested_filename = menu_observer.GetSuggestedFilename();
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

  scoped_ptr<TestRenderViewContextMenu> menu(
      CreateContextMenu(GURL(), GURL("http://url.com/image.png"),
                        blink::WebContextMenuData::MediaTypeImage));

  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB));
  ASSERT_TRUE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_OPEN_ORIGINAL_IMAGE_NEW_TAB));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       DataSaverHttpsOpenImageInNewTab) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(
      data_reduction_proxy::switches::kEnableDataReductionProxy);

  scoped_ptr<TestRenderViewContextMenu> menu(
      CreateContextMenu(GURL(), GURL("https://url.com/image.png"),
                        blink::WebContextMenuData::MediaTypeImage));

  ASSERT_FALSE(
      menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPEN_ORIGINAL_IMAGE_NEW_TAB));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest, OpenImageInNewTab) {
  scoped_ptr<TestRenderViewContextMenu> menu(
      CreateContextMenu(GURL(), GURL("http://url.com/image.png"),
                        blink::WebContextMenuData::MediaTypeImage));
  ASSERT_FALSE(
      menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPEN_ORIGINAL_IMAGE_NEW_TAB));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB));
}

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
    ASSERT_TRUE(test_server()->Start());
    SetupImageSearchEngine();

    // Go to a page with an image in it. The test server doesn't serve the image
    // with the right MIME type, so use a data URL to make a page containing it.
    GURL image_url(test_server()->GetURL(image_path));
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
    static const char kImageSearchURL[] = "imagesearch";
    return test_server()->GetURL(kImageSearchURL);
  }

 private:
  void SetupImageSearchEngine() {
    static const char kShortName[] = "test";
    static const char kSearchURL[] = "search?q={searchTerms}";
    static const char kImageSearchPostParams[] =
        "thumb={google:imageThumbnail}";

    TemplateURLService* model =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(model);
    ui_test_utils::WaitForTemplateURLServiceToLoad(model);
    ASSERT_TRUE(model->loaded());

    TemplateURLData data;
    data.short_name = base::ASCIIToUTF16(kShortName);
    data.SetKeyword(data.short_name);
    data.SetURL(test_server()->GetURL(kSearchURL).spec());
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

  scoped_ptr<ContextMenuNotificationObserver> menu_observer_;
};

IN_PROC_BROWSER_TEST_F(SearchByImageBrowserTest, ImageSearchWithValidImage) {
  static const char kValidImage[] = "files/image_search/valid.png";
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
  static const char kCorruptImage[] = "files/image_search/corrupt.png";
  SetupAndLoadImagePage(kCorruptImage);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ThumbnailResponseWatcher watcher(tab->GetRenderProcessHost());
  AttemptImageSearch();

  // The browser should receive a response from the renderer, because the
  // renderer should not crash.
  EXPECT_EQ(ThumbnailResponseWatcher::THUMBNAIL_RECEIVED, watcher.Wait());
}

}  // namespace
