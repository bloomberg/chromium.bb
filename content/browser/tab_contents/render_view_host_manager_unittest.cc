// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_thread_impl.h"
#include "content/browser/browser_url_handler.h"
#include "content/browser/mock_content_browser_client.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/render_view_host_manager.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "content/browser/webui/empty_web_ui_factory.h"
#include "content/common/test_url_constants.h"
#include "content/common/view_messages.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/url_constants.h"
#include "content/test/test_browser_context.h"
#include "content/test/test_notification_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "googleurl/src/url_util.h"
#include "webkit/glue/webkit_glue.h"

using content::BrowserThread;
using content::BrowserThreadImpl;

namespace {

const char kChromeUISchemeButNotWebUIURL[] = "chrome://not-webui";

class RenderViewHostManagerTestWebUIFactory
    : public content::EmptyWebUIFactory {
 public:
  RenderViewHostManagerTestWebUIFactory()
    : should_create_webui_(false) {
  }
  virtual ~RenderViewHostManagerTestWebUIFactory() {}

  void set_should_create_webui(bool should_create_webui) {
    should_create_webui_ = should_create_webui;
  }

  // WebUIFactory implementation.
  virtual WebUI* CreateWebUIForURL(TabContents* source,
                                   const GURL& url) const OVERRIDE {
    if (!(should_create_webui_ && HasWebUIScheme(url)))
      return NULL;
    return new WebUI(source);
  }

  virtual bool UseWebUIForURL(content::BrowserContext* browser_context,
                              const GURL& url) const OVERRIDE {
    return HasWebUIScheme(url);
  }

  virtual bool HasWebUIScheme(const GURL& url) const OVERRIDE {
    return url.SchemeIs(chrome::kChromeUIScheme) &&
        url.spec() != kChromeUISchemeButNotWebUIURL;
  }

 private:
  bool should_create_webui_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostManagerTestWebUIFactory);
};

class RenderViewHostManagerTestBrowserClient
    : public content::MockContentBrowserClient {
 public:
  RenderViewHostManagerTestBrowserClient() {}
  virtual ~RenderViewHostManagerTestBrowserClient() {}

  void set_should_create_webui(bool should_create_webui) {
    factory_.set_should_create_webui(should_create_webui);
  }

  // content::MockContentBrowserClient implementation.
  virtual content::WebUIFactory* GetWebUIFactory() OVERRIDE {
    return &factory_;
  }

 private:
  RenderViewHostManagerTestWebUIFactory factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostManagerTestBrowserClient);
};

}  // namespace

class RenderViewHostManagerTest : public RenderViewHostTestHarness {
 public:
  virtual void SetUp() OVERRIDE {
    RenderViewHostTestHarness::SetUp();
    old_browser_client_ = content::GetContentClient()->browser();
    content::GetContentClient()->set_browser(&browser_client_);
    url_util::AddStandardScheme(chrome::kChromeUIScheme);
  }

  virtual void TearDown() OVERRIDE {
    RenderViewHostTestHarness::TearDown();
    content::GetContentClient()->set_browser(old_browser_client_);
  }

  void set_should_create_webui(bool should_create_webui) {
    browser_client_.set_should_create_webui(should_create_webui);
  }

  void NavigateActiveAndCommit(const GURL& url) {
    // Note: we navigate the active RenderViewHost because previous navigations
    // won't have committed yet, so NavigateAndCommit does the wrong thing
    // for us.
    controller().LoadURL(
        url, content::Referrer(), content::PAGE_TRANSITION_LINK, std::string());
    TestRenderViewHost* old_rvh = rvh();

    // Simulate the ShouldClose_ACK that is received from the current renderer
    // for a cross-site navigation.
    if (old_rvh != active_rvh())
      old_rvh->SendShouldCloseACK(true);

    // Commit the navigation.
    active_rvh()->SendNavigate(
        static_cast<MockRenderProcessHost*>(active_rvh()->process())->
            max_page_id() + 1,
        url);

    // Simulate the SwapOut_ACK that fires if you commit a cross-site navigation
    // without making any network requests.
    if (old_rvh != active_rvh())
      old_rvh->OnSwapOutACK();
  }

  bool ShouldSwapProcesses(RenderViewHostManager* manager,
                           const NavigationEntry* cur_entry,
                           const NavigationEntry* new_entry) const {
    return manager->ShouldSwapProcessesForNavigation(cur_entry, new_entry);
  }

 private:
  RenderViewHostManagerTestBrowserClient browser_client_;
  content::ContentBrowserClient* old_browser_client_;
};

// Tests that when you navigate from the New TabPage to another page, and
// then do that same thing in another tab, that the two resulting pages have
// different SiteInstances, BrowsingInstances, and RenderProcessHosts. This is
// a regression test for bug 9364.
TEST_F(RenderViewHostManagerTest, NewTabPageProcesses) {
  BrowserThreadImpl ui_thread(BrowserThread::UI, MessageLoop::current());
  const GURL kNtpUrl(chrome::kTestNewTabURL);
  const GURL kDestUrl("http://www.google.com/");

  // Navigate our first tab to the new tab page and then to the destination.
  NavigateActiveAndCommit(kNtpUrl);
  NavigateActiveAndCommit(kDestUrl);

  // Make a second tab.
  TestTabContents contents2(browser_context(), NULL);

  // Load the two URLs in the second tab. Note that the first navigation creates
  // a RVH that's not pending (since there is no cross-site transition), so
  // we use the committed one.
  contents2.controller().LoadURL(
      kNtpUrl, content::Referrer(), content::PAGE_TRANSITION_LINK,
      std::string());
  TestRenderViewHost* ntp_rvh2 = static_cast<TestRenderViewHost*>(
      contents2.render_manager_for_testing()->current_host());
  EXPECT_FALSE(contents2.cross_navigation_pending());
  ntp_rvh2->SendNavigate(100, kNtpUrl);

  // The second one is the opposite, creating a cross-site transition and
  // requiring a beforeunload ack.
  contents2.controller().LoadURL(
      kDestUrl, content::Referrer(), content::PAGE_TRANSITION_LINK,
      std::string());
  EXPECT_TRUE(contents2.cross_navigation_pending());
  TestRenderViewHost* dest_rvh2 = static_cast<TestRenderViewHost*>(
      contents2.render_manager_for_testing()->pending_render_view_host());
  ASSERT_TRUE(dest_rvh2);
  ntp_rvh2->SendShouldCloseACK(true);
  dest_rvh2->SendNavigate(101, kDestUrl);
  ntp_rvh2->OnSwapOutACK();

  // The two RVH's should be different in every way.
  EXPECT_NE(active_rvh()->process(), dest_rvh2->process());
  EXPECT_NE(active_rvh()->site_instance(), dest_rvh2->site_instance());
  EXPECT_NE(active_rvh()->site_instance()->browsing_instance(),
            dest_rvh2->site_instance()->browsing_instance());

  // Navigate both to the new tab page, and verify that they share a
  // SiteInstance.
  NavigateActiveAndCommit(kNtpUrl);

  contents2.controller().LoadURL(
      kNtpUrl, content::Referrer(), content::PAGE_TRANSITION_LINK,
      std::string());
  dest_rvh2->SendShouldCloseACK(true);
  static_cast<TestRenderViewHost*>(contents2.render_manager_for_testing()->
     pending_render_view_host())->SendNavigate(102, kNtpUrl);
  dest_rvh2->OnSwapOutACK();

  EXPECT_EQ(active_rvh()->site_instance(),
      contents2.render_view_host()->site_instance());
}

// When there is an error with the specified page, renderer exits view-source
// mode. See WebFrameImpl::DidFail(). We check by this test that
// EnableViewSourceMode message is sent on every navigation regardless
// RenderView is being newly created or reused.
TEST_F(RenderViewHostManagerTest, AlwaysSendEnableViewSourceMode) {
  BrowserThreadImpl ui_thread(BrowserThread::UI, MessageLoop::current());
  const GURL kNtpUrl(chrome::kTestNewTabURL);
  const GURL kUrl("view-source:http://foo");

  // We have to navigate to some page at first since without this, the first
  // navigation will reuse the SiteInstance created by Init(), and the second
  // one will create a new SiteInstance. Because current_instance and
  // new_instance will be different, a new RenderViewHost will be created for
  // the second navigation. We have to avoid this in order to exercise the
  // target code patch.
  NavigateActiveAndCommit(kNtpUrl);

  // Navigate.
  controller().LoadURL(
      kUrl, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  // Simulate response from RenderView for FirePageBeforeUnload.
  rvh()->TestOnMessageReceived(
      ViewHostMsg_ShouldClose_ACK(rvh()->routing_id(), true));
  ASSERT_TRUE(pending_rvh());  // New pending RenderViewHost will be created.
  RenderViewHost* last_rvh = pending_rvh();
  int new_id = static_cast<MockRenderProcessHost*>(pending_rvh()->process())->
               max_page_id() + 1;
  pending_rvh()->SendNavigate(new_id, kUrl);
  EXPECT_EQ(controller().last_committed_entry_index(), 1);
  ASSERT_TRUE(controller().GetLastCommittedEntry());
  EXPECT_TRUE(kUrl == controller().GetLastCommittedEntry()->url());
  EXPECT_FALSE(controller().pending_entry());
  // Because we're using TestTabContents and TestRenderViewHost in this
  // unittest, no one calls TabContents::RenderViewCreated(). So, we see no
  // EnableViewSourceMode message, here.

  // Clear queued messages before load.
  process()->sink().ClearMessages();
  // Navigate, again.
  controller().LoadURL(
      kUrl, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  // The same RenderViewHost should be reused.
  EXPECT_FALSE(pending_rvh());
  EXPECT_TRUE(last_rvh == rvh());
  rvh()->SendNavigate(new_id, kUrl);  // The same page_id returned.
  EXPECT_EQ(controller().last_committed_entry_index(), 1);
  EXPECT_FALSE(controller().pending_entry());
  // New message should be sent out to make sure to enter view-source mode.
  EXPECT_TRUE(process()->sink().GetUniqueMessageMatching(
      ViewMsg_EnableViewSourceMode::ID));
}

// Tests the Init function by checking the initial RenderViewHost.
TEST_F(RenderViewHostManagerTest, Init) {
  // Using TestBrowserContext.
  SiteInstance* instance = SiteInstance::CreateSiteInstance(browser_context());
  EXPECT_FALSE(instance->has_site());

  TestTabContents tab_contents(browser_context(), instance);
  RenderViewHostManager manager(&tab_contents, &tab_contents);

  manager.Init(browser_context(), instance, MSG_ROUTING_NONE);

  RenderViewHost* host = manager.current_host();
  ASSERT_TRUE(host);
  EXPECT_TRUE(instance == host->site_instance());
  EXPECT_TRUE(&tab_contents == host->delegate());
  EXPECT_TRUE(manager.GetRenderWidgetHostView());
  EXPECT_FALSE(manager.pending_render_view_host());
}

// Tests the Navigate function. We navigate three sites consecutively and check
// how the pending/committed RenderViewHost are modified.
TEST_F(RenderViewHostManagerTest, Navigate) {
  TestNotificationTracker notifications;

  SiteInstance* instance = SiteInstance::CreateSiteInstance(browser_context());

  TestTabContents tab_contents(browser_context(), instance);
  notifications.ListenFor(
      content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
      content::Source<NavigationController>(&tab_contents.controller()));

  // Create.
  RenderViewHostManager manager(&tab_contents, &tab_contents);

  manager.Init(browser_context(), instance, MSG_ROUTING_NONE);

  RenderViewHost* host;

  // 1) The first navigation. --------------------------
  const GURL kUrl1("http://www.google.com/");
  NavigationEntry entry1(NULL /* instance */, -1 /* page_id */, kUrl1,
                         GURL() /* referrer */, string16() /* title */,
                         content::PAGE_TRANSITION_TYPED,
                         false /* is_renderer_init */);
  host = manager.Navigate(entry1);

  // The RenderViewHost created in Init will be reused.
  EXPECT_TRUE(host == manager.current_host());
  EXPECT_FALSE(manager.pending_render_view_host());

  // Commit.
  manager.DidNavigateMainFrame(host);
  // Commit to SiteInstance should be delayed until RenderView commit.
  EXPECT_TRUE(host == manager.current_host());
  ASSERT_TRUE(host);
  EXPECT_FALSE(host->site_instance()->has_site());
  host->site_instance()->SetSite(kUrl1);

  // 2) Navigate to next site. -------------------------
  const GURL kUrl2("http://www.google.com/foo");
  NavigationEntry entry2(NULL /* instance */, -1 /* page_id */, kUrl2,
                         kUrl1 /* referrer */, string16() /* title */,
                         content::PAGE_TRANSITION_LINK,
                         true /* is_renderer_init */);
  host = manager.Navigate(entry2);

  // The RenderViewHost created in Init will be reused.
  EXPECT_TRUE(host == manager.current_host());
  EXPECT_FALSE(manager.pending_render_view_host());

  // Commit.
  manager.DidNavigateMainFrame(host);
  EXPECT_TRUE(host == manager.current_host());
  ASSERT_TRUE(host);
  EXPECT_TRUE(host->site_instance()->has_site());

  // 3) Cross-site navigate to next site. --------------
  const GURL kUrl3("http://webkit.org/");
  NavigationEntry entry3(NULL /* instance */, -1 /* page_id */, kUrl3,
                         kUrl2 /* referrer */, string16() /* title */,
                         content::PAGE_TRANSITION_LINK,
                         false /* is_renderer_init */);
  host = manager.Navigate(entry3);

  // A new RenderViewHost should be created.
  EXPECT_TRUE(manager.pending_render_view_host());
  ASSERT_EQ(host, manager.pending_render_view_host());

  notifications.Reset();

  // Commit.
  manager.DidNavigateMainFrame(manager.pending_render_view_host());
  EXPECT_TRUE(host == manager.current_host());
  ASSERT_TRUE(host);
  EXPECT_TRUE(host->site_instance()->has_site());
  // Check the pending RenderViewHost has been committed.
  EXPECT_FALSE(manager.pending_render_view_host());

  // We should observe a notification.
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED));
}

// Tests the Navigate function. In this unit test we verify that the Navigate
// function can handle a new navigation event before the previous navigation
// has been committed. This is also a regression test for
// http://crbug.com/104600.
TEST_F(RenderViewHostManagerTest, NavigateWithEarlyReNavigation) {
  TestNotificationTracker notifications;

  SiteInstance* instance = SiteInstance::CreateSiteInstance(browser_context());

  TestTabContents tab_contents(browser_context(), instance);
  notifications.ListenFor(
      content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
      content::Source<NavigationController>(&tab_contents.controller()));

  // Create.
  RenderViewHostManager manager(&tab_contents, &tab_contents);

  manager.Init(browser_context(), instance, MSG_ROUTING_NONE);

  // 1) The first navigation. --------------------------
  const GURL kUrl1("http://www.google.com/");
  NavigationEntry entry1(NULL /* instance */, -1 /* page_id */, kUrl1,
                         GURL() /* referrer */, string16() /* title */,
                         content::PAGE_TRANSITION_TYPED,
                         false /* is_renderer_init */);
  RenderViewHost* host = manager.Navigate(entry1);

  // The RenderViewHost created in Init will be reused.
  EXPECT_TRUE(host == manager.current_host());
  EXPECT_FALSE(manager.pending_render_view_host());

  // We should observe a notification.
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED));
  notifications.Reset();

  // Commit.
  manager.DidNavigateMainFrame(host);

  // Commit to SiteInstance should be delayed until RenderView commit.
  EXPECT_TRUE(host == manager.current_host());
  ASSERT_TRUE(host);
  EXPECT_FALSE(host->site_instance()->has_site());
  host->site_instance()->SetSite(kUrl1);

  // 2) Cross-site navigate to next site. -------------------------
  const GURL kUrl2("http://www.example.com");
  NavigationEntry entry2(NULL /* instance */, -1 /* page_id */, kUrl2,
                         GURL() /* referrer */, string16() /* title */,
                         content::PAGE_TRANSITION_TYPED,
                         false /* is_renderer_init */);
  RenderViewHost* host2 = manager.Navigate(entry2);

  // A new RenderViewHost should be created.
  EXPECT_TRUE(manager.pending_render_view_host());
  ASSERT_EQ(host2, manager.pending_render_view_host());

  // Check that the navigation is still suspended because the old RVH
  // is not swapped out, yet.
  MockRenderProcessHost* test_process_host2 =
      static_cast<MockRenderProcessHost*>(host2->process());
  test_process_host2->sink().ClearMessages();
  host2->NavigateToURL(kUrl2);
  EXPECT_FALSE(test_process_host2->sink().GetUniqueMessageMatching(
      ViewMsg_Navigate::ID));

  // Allow closing the current Render View (precondition for swapping out
  // the RVH): Simulate response from RenderView for ViewMsg_ShouldClose sent by
  // FirePageBeforeUnload
  TestRenderViewHost* test_host = static_cast<TestRenderViewHost*>(host);
  MockRenderProcessHost* test_process_host =
      static_cast<MockRenderProcessHost*>(test_host->process());
  EXPECT_TRUE(test_process_host->sink().GetUniqueMessageMatching(
      ViewMsg_ShouldClose::ID));
  test_host->SendShouldCloseACK(true);

  // CrossSiteResourceHandler::StartCrossSiteTransition can trigger a
  // call of RenderViewHostManager::OnCrossSiteResponse before
  // RenderViewHostManager::DidNavigateMainFrame is called. In this case the
  // RVH is swapped out.
  manager.OnCrossSiteResponse(host2->process()->GetID(),
                              host2->GetPendingRequestId());
  EXPECT_TRUE(test_process_host->sink().GetUniqueMessageMatching(
      ViewMsg_SwapOut::ID));
  test_host->OnSwapOutACK();

  EXPECT_EQ(host, manager.current_host());
  EXPECT_TRUE(manager.current_host()->is_swapped_out());
  EXPECT_EQ(host2, manager.pending_render_view_host());
  // There should be still no navigation messages being sent.
  EXPECT_FALSE(test_process_host2->sink().GetUniqueMessageMatching(
      ViewMsg_Navigate::ID));

  // 3) Cross-site navigate to next site before 2) has committed. --------------
  const GURL kUrl3("http://webkit.org/");
  NavigationEntry entry3(NULL /* instance */, -1 /* page_id */, kUrl3,
                         GURL() /* referrer */, string16() /* title */,
                         content::PAGE_TRANSITION_TYPED,
                         false /* is_renderer_init */);
  RenderViewHost* host3 = manager.Navigate(entry3);

  // A new RenderViewHost should be created.
  EXPECT_TRUE(manager.pending_render_view_host());
  ASSERT_EQ(host3, manager.pending_render_view_host());

  EXPECT_EQ(host, manager.current_host());
  EXPECT_TRUE(manager.current_host()->is_swapped_out());

  // The navigation should not be suspended because the RVH |host| has been
  // swapped out already. Therefore, the RVH should send a navigation event
  // immediately.
  MockRenderProcessHost* test_process_host3 =
      static_cast<MockRenderProcessHost*>(host3->process());
  test_process_host3->sink().ClearMessages();

  // Usually TabContents::NavigateToEntry would call
  // RenderViewHostManager::Navigate followed by RenderViewHost::Navigate.
  // Here we need to call the latter ourselves.
  host3->NavigateToURL(kUrl3);
  EXPECT_TRUE(test_process_host3->sink().GetUniqueMessageMatching(
      ViewMsg_Navigate::ID));

  // Commit.
  manager.DidNavigateMainFrame(host3);
  EXPECT_TRUE(host3 == manager.current_host());
  ASSERT_TRUE(host3);
  EXPECT_TRUE(host3->site_instance()->has_site());
  // Check the pending RenderViewHost has been committed.
  EXPECT_FALSE(manager.pending_render_view_host());

  // We should observe a notification.
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED));
}

// Tests WebUI creation.
TEST_F(RenderViewHostManagerTest, WebUI) {
  set_should_create_webui(true);
  BrowserThreadImpl ui_thread(BrowserThread::UI, MessageLoop::current());
  SiteInstance* instance = SiteInstance::CreateSiteInstance(browser_context());

  TestTabContents tab_contents(browser_context(), instance);
  RenderViewHostManager manager(&tab_contents, &tab_contents);

  manager.Init(browser_context(), instance, MSG_ROUTING_NONE);

  const GURL kUrl(chrome::kTestNewTabURL);
  NavigationEntry entry(NULL /* instance */, -1 /* page_id */, kUrl,
                        GURL() /* referrer */, string16() /* title */,
                        content::PAGE_TRANSITION_TYPED,
                        false /* is_renderer_init */);
  RenderViewHost* host = manager.Navigate(entry);

  EXPECT_TRUE(host);
  EXPECT_TRUE(host == manager.current_host());
  EXPECT_FALSE(manager.pending_render_view_host());

  // It's important that the site instance get set on the Web UI page as soon
  // as the navigation starts, rather than lazily after it commits, so we don't
  // try to re-use the SiteInstance/process for non DOM-UI things that may
  // get loaded in between.
  EXPECT_TRUE(host->site_instance()->has_site());
  EXPECT_EQ(kUrl, host->site_instance()->site());

  // The Web UI is committed immediately because the RenderViewHost has not been
  // used yet. UpdateRendererStateForNavigate() took the short cut path.
  EXPECT_FALSE(manager.pending_web_ui());
  EXPECT_TRUE(manager.web_ui());

  // Commit.
  manager.DidNavigateMainFrame(host);
}

// Tests that chrome: URLs that are not Web UI pages do not get grouped into
// Web UI renderers, even if --process-per-tab is enabled.  In that mode, we
// still swap processes if ShouldSwapProcessesForNavigation is true.
// Regression test for bug 46290.
TEST_F(RenderViewHostManagerTest, NonWebUIChromeURLs) {
  BrowserThreadImpl thread(BrowserThread::UI, &message_loop_);
  SiteInstance* instance = SiteInstance::CreateSiteInstance(browser_context());
  TestTabContents tab_contents(browser_context(), instance);
  RenderViewHostManager manager(&tab_contents, &tab_contents);
  manager.Init(browser_context(), instance, MSG_ROUTING_NONE);

  // NTP is a Web UI page.
  const GURL kNtpUrl(chrome::kTestNewTabURL);
  NavigationEntry ntp_entry(NULL /* instance */, -1 /* page_id */, kNtpUrl,
                            GURL() /* referrer */, string16() /* title */,
                            content::PAGE_TRANSITION_TYPED,
                            false /* is_renderer_init */);

  // A URL with the Chrome UI scheme, that isn't handled by Web UI.
  GURL about_url(kChromeUISchemeButNotWebUIURL);
  NavigationEntry about_entry(NULL /* instance */, -1 /* page_id */, about_url,
                              GURL() /* referrer */, string16() /* title */,
                              content::PAGE_TRANSITION_TYPED,
                              false /* is_renderer_init */);

  EXPECT_TRUE(ShouldSwapProcesses(&manager, &ntp_entry, &about_entry));
}

// Tests that we don't end up in an inconsistent state if a page does a back and
// then reload. http://crbug.com/51680
TEST_F(RenderViewHostManagerTest, PageDoesBackAndReload) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.evil-site.com/");

  // Navigate to a safe site, then an evil site.
  // This will switch RenderViewHosts.  We cannot assert that the first and
  // second RVHs are different, though, because the first one may be promptly
  // deleted.
  contents()->NavigateAndCommit(kUrl1);
  contents()->NavigateAndCommit(kUrl2);
  RenderViewHost* evil_rvh = contents()->render_view_host();

  // Now let's simulate the evil page calling history.back().
  contents()->OnGoToEntryAtOffset(-1);
  // We should have a new pending RVH.
  // Note that in this case, the navigation has not committed, so evil_rvh will
  // not be deleted yet.
  EXPECT_NE(evil_rvh, contents()->render_manager_for_testing()->
      pending_render_view_host());

  // Before that RVH has committed, the evil page reloads itself.
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 1;
  params.url = kUrl2;
  params.transition = content::PAGE_TRANSITION_CLIENT_REDIRECT;
  params.should_update_history = false;
  params.gesture = NavigationGestureAuto;
  params.was_within_same_page = false;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(kUrl2));
  contents()->DidNavigate(evil_rvh, params);

  // That should have cancelled the pending RVH, and the evil RVH should be the
  // current one.
  EXPECT_TRUE(contents()->render_manager_for_testing()->
      pending_render_view_host() == NULL);
  EXPECT_EQ(evil_rvh, contents()->render_manager_for_testing()->current_host());

  // Also we should not have a pending navigation entry.
  NavigationEntry* entry = contents()->controller().GetActiveEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(kUrl2, entry->url());
}
