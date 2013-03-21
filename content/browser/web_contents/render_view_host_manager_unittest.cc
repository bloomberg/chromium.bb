// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/navigation_controller_impl.h"
#include "content/browser/web_contents/navigation_entry_impl.h"
#include "content/browser/web_contents/render_view_host_manager.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/common/view_messages.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/javascript_message_type.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_notification_tracker.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/glue_serialize.h"

namespace content {
namespace {

class RenderViewHostManagerTestWebUIControllerFactory
    : public WebUIControllerFactory {
 public:
  RenderViewHostManagerTestWebUIControllerFactory()
    : should_create_webui_(false) {
  }
  virtual ~RenderViewHostManagerTestWebUIControllerFactory() {}

  void set_should_create_webui(bool should_create_webui) {
    should_create_webui_ = should_create_webui;
  }

  // WebUIFactory implementation.
  virtual WebUIController* CreateWebUIControllerForURL(
      WebUI* web_ui, const GURL& url) const OVERRIDE {
    if (!(should_create_webui_ && HasWebUIScheme(url)))
      return NULL;
    return new WebUIController(web_ui);
  }

   virtual WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
      const GURL& url) const OVERRIDE {
    return WebUI::kNoWebUI;
  }

  virtual bool UseWebUIForURL(BrowserContext* browser_context,
                              const GURL& url) const OVERRIDE {
    return HasWebUIScheme(url);
  }

  virtual bool UseWebUIBindingsForURL(BrowserContext* browser_context,
                                      const GURL& url) const OVERRIDE {
    return HasWebUIScheme(url);
  }

 private:
  bool should_create_webui_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostManagerTestWebUIControllerFactory);
};

}  // namespace

class RenderViewHostManagerTest
    : public RenderViewHostImplTestHarness {
 public:
  virtual void SetUp() OVERRIDE {
    RenderViewHostImplTestHarness::SetUp();
    WebUIControllerFactory::RegisterFactory(&factory_);
  }

  virtual void TearDown() OVERRIDE {
    RenderViewHostImplTestHarness::TearDown();
    WebUIControllerFactory::UnregisterFactoryForTesting(&factory_);
  }

  void set_should_create_webui(bool should_create_webui) {
    factory_.set_should_create_webui(should_create_webui);
  }

  void NavigateActiveAndCommit(const GURL& url) {
    // Note: we navigate the active RenderViewHost because previous navigations
    // won't have committed yet, so NavigateAndCommit does the wrong thing
    // for us.
    controller().LoadURL(url, Referrer(), PAGE_TRANSITION_LINK, std::string());
    TestRenderViewHost* old_rvh = test_rvh();

    // Simulate the ShouldClose_ACK that is received from the current renderer
    // for a cross-site navigation.
    if (old_rvh != active_rvh())
      old_rvh->SendShouldCloseACK(true);

    // Commit the navigation with a new page ID.
    int32 max_page_id = contents()->GetMaxPageIDForSiteInstance(
        active_rvh()->GetSiteInstance());
    active_test_rvh()->SendNavigate(max_page_id + 1, url);

    // Simulate the SwapOut_ACK that fires if you commit a cross-site navigation
    // without making any network requests.
    if (old_rvh != active_rvh())
      old_rvh->OnSwapOutACK(false);
  }

  bool ShouldSwapProcesses(RenderViewHostManager* manager,
                           const NavigationEntryImpl* cur_entry,
                           const NavigationEntryImpl* new_entry) const {
    return manager->ShouldSwapProcessesForNavigation(cur_entry, new_entry);
  }

 private:
  RenderViewHostManagerTestWebUIControllerFactory factory_;
};

// Tests that when you navigate from a chrome:// url to another page, and
// then do that same thing in another tab, that the two resulting pages have
// different SiteInstances, BrowsingInstances, and RenderProcessHosts. This is
// a regression test for bug 9364.
TEST_F(RenderViewHostManagerTest, NewTabPageProcesses) {
  set_should_create_webui(true);
  BrowserThreadImpl ui_thread(BrowserThread::UI, MessageLoop::current());
  const GURL kChromeUrl("chrome://foo");
  const GURL kDestUrl("http://www.google.com/");

  // Navigate our first tab to the chrome url and then to the destination,
  // ensuring we grant bindings to the chrome URL.
  NavigateActiveAndCommit(kChromeUrl);
  EXPECT_TRUE(active_rvh()->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);
  NavigateActiveAndCommit(kDestUrl);

  // Make a second tab.
  scoped_ptr<TestWebContents> contents2(
      TestWebContents::Create(browser_context(), NULL));

  // Load the two URLs in the second tab. Note that the first navigation creates
  // a RVH that's not pending (since there is no cross-site transition), so
  // we use the committed one.
  contents2->GetController().LoadURL(
      kChromeUrl, Referrer(), PAGE_TRANSITION_LINK, std::string());
  TestRenderViewHost* ntp_rvh2 = static_cast<TestRenderViewHost*>(
      contents2->GetRenderManagerForTesting()->current_host());
  EXPECT_FALSE(contents2->cross_navigation_pending());
  ntp_rvh2->SendNavigate(100, kChromeUrl);

  // The second one is the opposite, creating a cross-site transition and
  // requiring a beforeunload ack.
  contents2->GetController().LoadURL(
      kDestUrl, Referrer(), PAGE_TRANSITION_LINK, std::string());
  EXPECT_TRUE(contents2->cross_navigation_pending());
  TestRenderViewHost* dest_rvh2 = static_cast<TestRenderViewHost*>(
      contents2->GetRenderManagerForTesting()->pending_render_view_host());
  ASSERT_TRUE(dest_rvh2);
  ntp_rvh2->SendShouldCloseACK(true);
  dest_rvh2->SendNavigate(101, kDestUrl);
  ntp_rvh2->OnSwapOutACK(false);

  // The two RVH's should be different in every way.
  EXPECT_NE(active_rvh()->GetProcess(), dest_rvh2->GetProcess());
  EXPECT_NE(active_rvh()->GetSiteInstance(), dest_rvh2->GetSiteInstance());
  EXPECT_FALSE(active_rvh()->GetSiteInstance()->IsRelatedSiteInstance(
                   dest_rvh2->GetSiteInstance()));

  // Navigate both to the new tab page, and verify that they share a
  // RenderProcessHost (not a SiteInstance).
  NavigateActiveAndCommit(kChromeUrl);

  contents2->GetController().LoadURL(
      kChromeUrl, Referrer(), PAGE_TRANSITION_LINK, std::string());
  dest_rvh2->SendShouldCloseACK(true);
  static_cast<TestRenderViewHost*>(contents2->GetRenderManagerForTesting()->
     pending_render_view_host())->SendNavigate(102, kChromeUrl);
  dest_rvh2->OnSwapOutACK(false);

  EXPECT_NE(active_rvh()->GetSiteInstance(),
            contents2->GetRenderViewHost()->GetSiteInstance());
  EXPECT_EQ(active_rvh()->GetSiteInstance()->GetProcess(),
            contents2->GetRenderViewHost()->GetSiteInstance()->GetProcess());
}

// Ensure that the browser ignores most IPC messages that arrive from a
// RenderViewHost that has been swapped out.  We do not want to take
// action on requests from a non-active renderer.  The main exception is
// for synchronous messages, which cannot be ignored without leaving the
// renderer in a stuck state.  See http://crbug.com/93427.
TEST_F(RenderViewHostManagerTest, FilterMessagesWhileSwappedOut) {
  BrowserThreadImpl ui_thread(BrowserThread::UI, MessageLoop::current());
  const GURL kChromeURL("chrome://foo");
  const GURL kDestUrl("http://www.google.com/");

  // Navigate our first tab to a chrome url and then to the destination.
  NavigateActiveAndCommit(kChromeURL);
  TestRenderViewHost* ntp_rvh = static_cast<TestRenderViewHost*>(
      contents()->GetRenderManagerForTesting()->current_host());

  // Send an update title message and make sure it works.
  const string16 ntp_title = ASCIIToUTF16("NTP Title");
  WebKit::WebTextDirection direction = WebKit::WebTextDirectionLeftToRight;
  EXPECT_TRUE(ntp_rvh->OnMessageReceived(
      ViewHostMsg_UpdateTitle(rvh()->GetRoutingID(), 0, ntp_title, direction)));
  EXPECT_EQ(ntp_title, contents()->GetTitle());

  // Navigate to a cross-site URL.
  contents()->GetController().LoadURL(
      kDestUrl, Referrer(), PAGE_TRANSITION_LINK, std::string());
  EXPECT_TRUE(contents()->cross_navigation_pending());
  TestRenderViewHost* dest_rvh = static_cast<TestRenderViewHost*>(
      contents()->GetRenderManagerForTesting()->pending_render_view_host());
  ASSERT_TRUE(dest_rvh);
  EXPECT_NE(ntp_rvh, dest_rvh);

  // BeforeUnload finishes.
  ntp_rvh->SendShouldCloseACK(true);

  // Assume SwapOutACK times out, so the dest_rvh proceeds and commits.
  dest_rvh->SendNavigate(101, kDestUrl);

  // The new RVH should be able to update its title.
  const string16 dest_title = ASCIIToUTF16("Google");
  EXPECT_TRUE(dest_rvh->OnMessageReceived(
      ViewHostMsg_UpdateTitle(rvh()->GetRoutingID(), 101, dest_title,
                              direction)));
  EXPECT_EQ(dest_title, contents()->GetTitle());

  // The old renderer, being slow, now updates the title. It should be filtered
  // out and not take effect.
  EXPECT_TRUE(ntp_rvh->is_swapped_out());
  EXPECT_TRUE(ntp_rvh->OnMessageReceived(
      ViewHostMsg_UpdateTitle(rvh()->GetRoutingID(), 0, ntp_title, direction)));
  EXPECT_EQ(dest_title, contents()->GetTitle());

  // We cannot filter out synchronous IPC messages, because the renderer would
  // be left waiting for a reply.  We pick RunBeforeUnloadConfirm as an example
  // that can run easily within a unit test, and that needs to receive a reply
  // without showing an actual dialog.
  MockRenderProcessHost* ntp_process_host =
      static_cast<MockRenderProcessHost*>(ntp_rvh->GetProcess());
  ntp_process_host->sink().ClearMessages();
  const string16 msg = ASCIIToUTF16("Message");
  bool result = false;
  string16 unused;
  ViewHostMsg_RunBeforeUnloadConfirm before_unload_msg(
      rvh()->GetRoutingID(), kChromeURL, msg, false, &result, &unused);
  // Enable pumping for check in BrowserMessageFilter::CheckCanDispatchOnUI.
  before_unload_msg.EnableMessagePumping();
  EXPECT_TRUE(ntp_rvh->OnMessageReceived(before_unload_msg));
  EXPECT_TRUE(ntp_process_host->sink().GetUniqueMessageMatching(IPC_REPLY_ID));

  // Also test RunJavaScriptMessage.
  ntp_process_host->sink().ClearMessages();
  ViewHostMsg_RunJavaScriptMessage js_msg(
      rvh()->GetRoutingID(), msg, msg, kChromeURL,
      JAVASCRIPT_MESSAGE_TYPE_CONFIRM, &result, &unused);
  js_msg.EnableMessagePumping();
  EXPECT_TRUE(ntp_rvh->OnMessageReceived(js_msg));
  EXPECT_TRUE(ntp_process_host->sink().GetUniqueMessageMatching(IPC_REPLY_ID));
}

// When there is an error with the specified page, renderer exits view-source
// mode. See WebFrameImpl::DidFail(). We check by this test that
// EnableViewSourceMode message is sent on every navigation regardless
// RenderView is being newly created or reused.
TEST_F(RenderViewHostManagerTest, AlwaysSendEnableViewSourceMode) {
  BrowserThreadImpl ui_thread(BrowserThread::UI, MessageLoop::current());
  const GURL kChromeUrl("chrome://foo");
  const GURL kUrl("view-source:http://foo");

  // We have to navigate to some page at first since without this, the first
  // navigation will reuse the SiteInstance created by Init(), and the second
  // one will create a new SiteInstance. Because current_instance and
  // new_instance will be different, a new RenderViewHost will be created for
  // the second navigation. We have to avoid this in order to exercise the
  // target code patch.
  NavigateActiveAndCommit(kChromeUrl);

  // Navigate.
  controller().LoadURL(
      kUrl, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  // Simulate response from RenderView for FirePageBeforeUnload.
  test_rvh()->OnMessageReceived(ViewHostMsg_ShouldClose_ACK(
      rvh()->GetRoutingID(), true, base::TimeTicks(), base::TimeTicks()));
  ASSERT_TRUE(pending_rvh());  // New pending RenderViewHost will be created.
  RenderViewHost* last_rvh = pending_rvh();
  int32 new_id = contents()->GetMaxPageIDForSiteInstance(
      active_rvh()->GetSiteInstance()) + 1;
  pending_test_rvh()->SendNavigate(new_id, kUrl);
  EXPECT_EQ(controller().GetLastCommittedEntryIndex(), 1);
  ASSERT_TRUE(controller().GetLastCommittedEntry());
  EXPECT_TRUE(kUrl == controller().GetLastCommittedEntry()->GetURL());
  EXPECT_FALSE(controller().GetPendingEntry());
  // Because we're using TestWebContents and TestRenderViewHost in this
  // unittest, no one calls WebContentsImpl::RenderViewCreated(). So, we see no
  // EnableViewSourceMode message, here.

  // Clear queued messages before load.
  process()->sink().ClearMessages();
  // Navigate, again.
  controller().LoadURL(
      kUrl, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  // The same RenderViewHost should be reused.
  EXPECT_FALSE(pending_rvh());
  EXPECT_TRUE(last_rvh == rvh());
  test_rvh()->SendNavigate(new_id, kUrl);  // The same page_id returned.
  EXPECT_EQ(controller().GetLastCommittedEntryIndex(), 1);
  EXPECT_FALSE(controller().GetPendingEntry());
  // New message should be sent out to make sure to enter view-source mode.
  EXPECT_TRUE(process()->sink().GetUniqueMessageMatching(
      ViewMsg_EnableViewSourceMode::ID));
}

// Tests the Init function by checking the initial RenderViewHost.
TEST_F(RenderViewHostManagerTest, Init) {
  // Using TestBrowserContext.
  SiteInstanceImpl* instance =
      static_cast<SiteInstanceImpl*>(SiteInstance::Create(browser_context()));
  EXPECT_FALSE(instance->HasSite());

  scoped_ptr<TestWebContents> web_contents(
      TestWebContents::Create(browser_context(), instance));
  RenderViewHostManager manager(web_contents.get(), web_contents.get(),
                                web_contents.get());

  manager.Init(browser_context(), instance, MSG_ROUTING_NONE);

  RenderViewHost* host = manager.current_host();
  ASSERT_TRUE(host);
  EXPECT_EQ(instance, host->GetSiteInstance());
  EXPECT_EQ(web_contents.get(), host->GetDelegate());
  EXPECT_TRUE(manager.GetRenderWidgetHostView());
  EXPECT_FALSE(manager.pending_render_view_host());
}

// Tests the Navigate function. We navigate three sites consecutively and check
// how the pending/committed RenderViewHost are modified.
TEST_F(RenderViewHostManagerTest, Navigate) {
  TestNotificationTracker notifications;

  SiteInstance* instance = SiteInstance::Create(browser_context());

  scoped_ptr<TestWebContents> web_contents(
      TestWebContents::Create(browser_context(), instance));
  notifications.ListenFor(
      NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
      Source<NavigationController>(&web_contents->GetController()));

  // Create.
  RenderViewHostManager manager(web_contents.get(), web_contents.get(),
                                web_contents.get());

  manager.Init(browser_context(), instance, MSG_ROUTING_NONE);

  RenderViewHost* host;

  // 1) The first navigation. --------------------------
  const GURL kUrl1("http://www.google.com/");
  NavigationEntryImpl entry1(
      NULL /* instance */, -1 /* page_id */, kUrl1, Referrer(),
      string16() /* title */, PAGE_TRANSITION_TYPED,
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
  EXPECT_FALSE(static_cast<SiteInstanceImpl*>(host->GetSiteInstance())->
      HasSite());
  static_cast<SiteInstanceImpl*>(host->GetSiteInstance())->SetSite(kUrl1);

  // 2) Navigate to next site. -------------------------
  const GURL kUrl2("http://www.google.com/foo");
  NavigationEntryImpl entry2(
      NULL /* instance */, -1 /* page_id */, kUrl2,
      Referrer(kUrl1, WebKit::WebReferrerPolicyDefault),
      string16() /* title */, PAGE_TRANSITION_LINK,
      true /* is_renderer_init */);
  host = manager.Navigate(entry2);

  // The RenderViewHost created in Init will be reused.
  EXPECT_TRUE(host == manager.current_host());
  EXPECT_FALSE(manager.pending_render_view_host());

  // Commit.
  manager.DidNavigateMainFrame(host);
  EXPECT_TRUE(host == manager.current_host());
  ASSERT_TRUE(host);
  EXPECT_TRUE(static_cast<SiteInstanceImpl*>(host->GetSiteInstance())->
      HasSite());

  // 3) Cross-site navigate to next site. --------------
  const GURL kUrl3("http://webkit.org/");
  NavigationEntryImpl entry3(
      NULL /* instance */, -1 /* page_id */, kUrl3,
      Referrer(kUrl2, WebKit::WebReferrerPolicyDefault),
      string16() /* title */, PAGE_TRANSITION_LINK,
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
  EXPECT_TRUE(static_cast<SiteInstanceImpl*>(host->GetSiteInstance())->
      HasSite());
  // Check the pending RenderViewHost has been committed.
  EXPECT_FALSE(manager.pending_render_view_host());

  // We should observe a notification.
  EXPECT_TRUE(notifications.Check1AndReset(
      NOTIFICATION_RENDER_VIEW_HOST_CHANGED));
}

// Tests the Navigate function. In this unit test we verify that the Navigate
// function can handle a new navigation event before the previous navigation
// has been committed. This is also a regression test for
// http://crbug.com/104600.
TEST_F(RenderViewHostManagerTest, NavigateWithEarlyReNavigation) {
  TestNotificationTracker notifications;

  SiteInstance* instance = SiteInstance::Create(browser_context());

  scoped_ptr<TestWebContents> web_contents(
      TestWebContents::Create(browser_context(), instance));
  notifications.ListenFor(
      NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
      Source<NavigationController>(&web_contents->GetController()));

  // Create.
  RenderViewHostManager manager(web_contents.get(), web_contents.get(),
                                web_contents.get());

  manager.Init(browser_context(), instance, MSG_ROUTING_NONE);

  // 1) The first navigation. --------------------------
  const GURL kUrl1("http://www.google.com/");
  NavigationEntryImpl entry1(NULL /* instance */, -1 /* page_id */, kUrl1,
                             Referrer(), string16() /* title */,
                             PAGE_TRANSITION_TYPED,
                             false /* is_renderer_init */);
  RenderViewHost* host = manager.Navigate(entry1);

  // The RenderViewHost created in Init will be reused.
  EXPECT_TRUE(host == manager.current_host());
  EXPECT_FALSE(manager.pending_render_view_host());

  // We should observe a notification.
  EXPECT_TRUE(notifications.Check1AndReset(
      NOTIFICATION_RENDER_VIEW_HOST_CHANGED));
  notifications.Reset();

  // Commit.
  manager.DidNavigateMainFrame(host);

  // Commit to SiteInstance should be delayed until RenderView commit.
  EXPECT_TRUE(host == manager.current_host());
  ASSERT_TRUE(host);
  EXPECT_FALSE(static_cast<SiteInstanceImpl*>(host->GetSiteInstance())->
      HasSite());
  static_cast<SiteInstanceImpl*>(host->GetSiteInstance())->SetSite(kUrl1);

  // 2) Cross-site navigate to next site. -------------------------
  const GURL kUrl2("http://www.example.com");
  NavigationEntryImpl entry2(
      NULL /* instance */, -1 /* page_id */, kUrl2, Referrer(),
      string16() /* title */, PAGE_TRANSITION_TYPED,
      false /* is_renderer_init */);
  RenderViewHostImpl* host2 = static_cast<RenderViewHostImpl*>(
      manager.Navigate(entry2));
  int host2_process_id = host2->GetProcess()->GetID();

  // A new RenderViewHost should be created.
  EXPECT_TRUE(manager.pending_render_view_host());
  ASSERT_EQ(host2, manager.pending_render_view_host());
  EXPECT_NE(host2, host);

  // Check that the navigation is still suspended because the old RVH
  // is not swapped out, yet.
  EXPECT_TRUE(host2->are_navigations_suspended());
  MockRenderProcessHost* test_process_host2 =
      static_cast<MockRenderProcessHost*>(host2->GetProcess());
  test_process_host2->sink().ClearMessages();
  host2->NavigateToURL(kUrl2);
  EXPECT_FALSE(test_process_host2->sink().GetUniqueMessageMatching(
      ViewMsg_Navigate::ID));

  // Allow closing the current Render View (precondition for swapping out
  // the RVH): Simulate response from RenderView for ViewMsg_ShouldClose sent by
  // FirePageBeforeUnload.
  TestRenderViewHost* test_host = static_cast<TestRenderViewHost*>(host);
  MockRenderProcessHost* test_process_host =
      static_cast<MockRenderProcessHost*>(test_host->GetProcess());
  EXPECT_TRUE(test_process_host->sink().GetUniqueMessageMatching(
      ViewMsg_ShouldClose::ID));
  test_host->SendShouldCloseACK(true);

  // CrossSiteResourceHandler::StartCrossSiteTransition triggers a
  // call of RenderViewHostManager::OnCrossSiteResponse before
  // RenderViewHostManager::DidNavigateMainFrame is called.
  // The RVH is not swapped out until the commit.
  manager.OnCrossSiteResponse(host2->GetProcess()->GetID(),
                              host2->GetPendingRequestId());
  EXPECT_TRUE(test_process_host->sink().GetUniqueMessageMatching(
      ViewMsg_SwapOut::ID));
  test_host->OnSwapOutACK(false);

  EXPECT_EQ(host, manager.current_host());
  EXPECT_FALSE(static_cast<RenderViewHostImpl*>(
      manager.current_host())->is_swapped_out());
  EXPECT_EQ(host2, manager.pending_render_view_host());
  // There should be still no navigation messages being sent.
  EXPECT_FALSE(test_process_host2->sink().GetUniqueMessageMatching(
      ViewMsg_Navigate::ID));

  // 3) Cross-site navigate to next site before 2) has committed. --------------
  const GURL kUrl3("http://webkit.org/");
  NavigationEntryImpl entry3(NULL /* instance */, -1 /* page_id */, kUrl3,
                             Referrer(), string16() /* title */,
                             PAGE_TRANSITION_TYPED,
                             false /* is_renderer_init */);
  test_process_host->sink().ClearMessages();
  RenderViewHost* host3 = manager.Navigate(entry3);

  // A new RenderViewHost should be created. host2 is now deleted.
  EXPECT_TRUE(manager.pending_render_view_host());
  ASSERT_EQ(host3, manager.pending_render_view_host());
  EXPECT_NE(host3, host);
  EXPECT_NE(host3->GetProcess()->GetID(), host2_process_id);

  // Navigations in the new RVH should be suspended, which is ok because the
  // old RVH is not yet swapped out and can respond to a second beforeunload
  // request.
  EXPECT_TRUE(static_cast<RenderViewHostImpl*>(
      host3)->are_navigations_suspended());
  EXPECT_EQ(host, manager.current_host());
  EXPECT_FALSE(static_cast<RenderViewHostImpl*>(
      manager.current_host())->is_swapped_out());

  // Simulate a response to the second beforeunload request.
  EXPECT_TRUE(test_process_host->sink().GetUniqueMessageMatching(
      ViewMsg_ShouldClose::ID));
  test_host->SendShouldCloseACK(true);

  // CrossSiteResourceHandler::StartCrossSiteTransition triggers a
  // call of RenderViewHostManager::OnCrossSiteResponse before
  // RenderViewHostManager::DidNavigateMainFrame is called.
  // The RVH is not swapped out until the commit.
  manager.OnCrossSiteResponse(host3->GetProcess()->GetID(),
                              static_cast<RenderViewHostImpl*>(
                                  host3)->GetPendingRequestId());
  EXPECT_TRUE(test_process_host->sink().GetUniqueMessageMatching(
      ViewMsg_SwapOut::ID));
  test_host->OnSwapOutACK(false);

  // Commit.
  manager.DidNavigateMainFrame(host3);
  EXPECT_TRUE(host3 == manager.current_host());
  ASSERT_TRUE(host3);
  EXPECT_TRUE(static_cast<SiteInstanceImpl*>(host3->GetSiteInstance())->
      HasSite());
  // Check the pending RenderViewHost has been committed.
  EXPECT_FALSE(manager.pending_render_view_host());

  // We should observe a notification.
  EXPECT_TRUE(notifications.Check1AndReset(
      NOTIFICATION_RENDER_VIEW_HOST_CHANGED));
}

// Tests WebUI creation.
TEST_F(RenderViewHostManagerTest, WebUI) {
  set_should_create_webui(true);
  BrowserThreadImpl ui_thread(BrowserThread::UI, MessageLoop::current());
  SiteInstance* instance = SiteInstance::Create(browser_context());

  scoped_ptr<TestWebContents> web_contents(
      TestWebContents::Create(browser_context(), instance));
  RenderViewHostManager manager(web_contents.get(), web_contents.get(),
                                web_contents.get());

  manager.Init(browser_context(), instance, MSG_ROUTING_NONE);
  EXPECT_FALSE(manager.current_host()->IsRenderViewLive());

  const GURL kUrl("chrome://foo");
  NavigationEntryImpl entry(NULL /* instance */, -1 /* page_id */, kUrl,
                            Referrer(), string16() /* title */,
                            PAGE_TRANSITION_TYPED,
                            false /* is_renderer_init */);
  RenderViewHost* host = manager.Navigate(entry);

  // We commit the pending RenderViewHost immediately because the previous
  // RenderViewHost was not live.  We test a case where it is live in
  // WebUIInNewTab.
  EXPECT_TRUE(host);
  EXPECT_EQ(host, manager.current_host());
  EXPECT_FALSE(manager.pending_render_view_host());

  // It's important that the site instance get set on the Web UI page as soon
  // as the navigation starts, rather than lazily after it commits, so we don't
  // try to re-use the SiteInstance/process for non Web UI things that may
  // get loaded in between.
  EXPECT_TRUE(static_cast<SiteInstanceImpl*>(host->GetSiteInstance())->
      HasSite());
  EXPECT_EQ(kUrl, host->GetSiteInstance()->GetSiteURL());

  // The Web UI is committed immediately because the RenderViewHost has not been
  // used yet. UpdateRendererStateForNavigate() took the short cut path.
  EXPECT_FALSE(manager.pending_web_ui());
  EXPECT_TRUE(manager.web_ui());

  // Commit.
  manager.DidNavigateMainFrame(host);
  EXPECT_TRUE(host->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);
}

// Tests that we can open a WebUI link in a new tab from a WebUI page and still
// grant the correct bindings.  http://crbug.com/189101.
TEST_F(RenderViewHostManagerTest, WebUIInNewTab) {
  set_should_create_webui(true);
  SiteInstance* blank_instance = SiteInstance::Create(browser_context());

  // Create a blank tab.
  scoped_ptr<TestWebContents> web_contents1(
      TestWebContents::Create(browser_context(), blank_instance));
  RenderViewHostManager manager1(web_contents1.get(), web_contents1.get(),
                                 web_contents1.get());
  manager1.Init(browser_context(), blank_instance, MSG_ROUTING_NONE);
  // Test the case that new RVH is considered live.
  manager1.current_host()->CreateRenderView(string16(), -1, -1);

  // Navigate to a WebUI page.
  const GURL kUrl1("chrome://foo");
  NavigationEntryImpl entry1(NULL /* instance */, -1 /* page_id */, kUrl1,
                             Referrer(), string16() /* title */,
                             PAGE_TRANSITION_TYPED,
                             false /* is_renderer_init */);
  RenderViewHost* host1 = manager1.Navigate(entry1);

  // We should have a pending navigation to the WebUI RenderViewHost.
  // It should already have bindings.
  EXPECT_EQ(host1, manager1.pending_render_view_host());
  EXPECT_NE(host1, manager1.current_host());
  EXPECT_TRUE(host1->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);

  // Commit and ensure we still have bindings.
  manager1.DidNavigateMainFrame(host1);
  SiteInstance* webui_instance = host1->GetSiteInstance();
  EXPECT_EQ(host1, manager1.current_host());
  EXPECT_TRUE(host1->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);

  // Now simulate clicking a link that opens in a new tab.
  scoped_ptr<TestWebContents> web_contents2(
      TestWebContents::Create(browser_context(), webui_instance));
  RenderViewHostManager manager2(web_contents2.get(), web_contents2.get(),
                                 web_contents2.get());
  manager2.Init(browser_context(), webui_instance, MSG_ROUTING_NONE);
  // Make sure the new RVH is considered live.  This is usually done in
  // RenderWidgetHost::Init when opening a new tab from a link.
  manager2.current_host()->CreateRenderView(string16(), -1, -1);

  const GURL kUrl2("chrome://foo/bar");
  NavigationEntryImpl entry2(NULL /* instance */, -1 /* page_id */, kUrl2,
                             Referrer(), string16() /* title */,
                             PAGE_TRANSITION_LINK,
                             true /* is_renderer_init */);
  RenderViewHost* host2 = manager2.Navigate(entry2);

  // No cross-process transition happens because we are already in the right
  // SiteInstance.  We should grant bindings immediately.
  EXPECT_EQ(host2, manager2.current_host());
  EXPECT_TRUE(host2->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);

  manager2.DidNavigateMainFrame(host2);
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
  RenderViewHost* evil_rvh = contents()->GetRenderViewHost();

  // Now let's simulate the evil page calling history.back().
  contents()->OnGoToEntryAtOffset(-1);
  // We should have a new pending RVH.
  // Note that in this case, the navigation has not committed, so evil_rvh will
  // not be deleted yet.
  EXPECT_NE(evil_rvh, contents()->GetRenderManagerForTesting()->
      pending_render_view_host());

  // Before that RVH has committed, the evil page reloads itself.
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 1;
  params.url = kUrl2;
  params.transition = PAGE_TRANSITION_CLIENT_REDIRECT;
  params.should_update_history = false;
  params.gesture = NavigationGestureAuto;
  params.was_within_same_page = false;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(kUrl2));
  contents()->DidNavigate(evil_rvh, params);

  // That should have cancelled the pending RVH, and the evil RVH should be the
  // current one.
  EXPECT_TRUE(contents()->GetRenderManagerForTesting()->
      pending_render_view_host() == NULL);
  EXPECT_EQ(evil_rvh, contents()->GetRenderManagerForTesting()->current_host());

  // Also we should not have a pending navigation entry.
  NavigationEntry* entry = contents()->GetController().GetActiveEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(kUrl2, entry->GetURL());
}

// Ensure that we can go back and forward even if a SwapOut ACK isn't received.
// See http://crbug.com/93427.
TEST_F(RenderViewHostManagerTest, NavigateAfterMissingSwapOutACK) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  // Navigate to two pages.
  contents()->NavigateAndCommit(kUrl1);
  TestRenderViewHost* rvh1 = test_rvh();
  contents()->NavigateAndCommit(kUrl2);
  TestRenderViewHost* rvh2 = test_rvh();

  // Now go back, but suppose the SwapOut_ACK isn't received.  This shouldn't
  // happen, but we have seen it when going back quickly across many entries
  // (http://crbug.com/93427).
  contents()->GetController().GoBack();
  EXPECT_TRUE(rvh2->is_waiting_for_beforeunload_ack());
  contents()->ProceedWithCrossSiteNavigation();
  EXPECT_FALSE(rvh2->is_waiting_for_beforeunload_ack());
  rvh2->SwapOut(1, 1);
  EXPECT_TRUE(rvh2->is_waiting_for_unload_ack());

  // The back navigation commits.  We should proactively clear the
  // is_waiting_for_unload_ack state to be safe.
  const NavigationEntry* entry1 = contents()->GetController().GetPendingEntry();
  rvh1->SendNavigate(entry1->GetPageID(), entry1->GetURL());
  EXPECT_TRUE(rvh2->is_swapped_out());
  EXPECT_FALSE(rvh2->is_waiting_for_unload_ack());

  // We should be able to navigate forward.
  contents()->GetController().GoForward();
  contents()->ProceedWithCrossSiteNavigation();
  const NavigationEntry* entry2 = contents()->GetController().GetPendingEntry();
  rvh2->SendNavigate(entry2->GetPageID(), entry2->GetURL());
  EXPECT_EQ(rvh2, rvh());
  EXPECT_FALSE(rvh2->is_swapped_out());
  EXPECT_TRUE(rvh1->is_swapped_out());
}

// Test that we create swapped out RVHs for the opener chain when navigating an
// opened tab cross-process.  This allows us to support certain cross-process
// JavaScript calls (http://crbug.com/99202).
TEST_F(RenderViewHostManagerTest, CreateSwappedOutOpenerRVHs) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");
  const GURL kChromeUrl("chrome://foo");

  // Navigate to an initial URL.
  contents()->NavigateAndCommit(kUrl1);
  RenderViewHostManager* manager = contents()->GetRenderManagerForTesting();
  TestRenderViewHost* rvh1 = test_rvh();

  // Create 2 new tabs and simulate them being the opener chain for the main
  // tab.  They should be in the same SiteInstance.
  scoped_ptr<TestWebContents> opener1(
      TestWebContents::Create(browser_context(), rvh1->GetSiteInstance()));
  RenderViewHostManager* opener1_manager =
      opener1->GetRenderManagerForTesting();
  contents()->SetOpener(opener1.get());

  scoped_ptr<TestWebContents> opener2(
      TestWebContents::Create(browser_context(), rvh1->GetSiteInstance()));
  RenderViewHostManager* opener2_manager =
      opener2->GetRenderManagerForTesting();
  opener1->SetOpener(opener2.get());

  // Navigate to a cross-site URL (different SiteInstance but same
  // BrowsingInstance).
  contents()->NavigateAndCommit(kUrl2);
  TestRenderViewHost* rvh2 = test_rvh();
  EXPECT_NE(rvh1->GetSiteInstance(), rvh2->GetSiteInstance());
  EXPECT_TRUE(rvh1->GetSiteInstance()->IsRelatedSiteInstance(
                  rvh2->GetSiteInstance()));

  // Ensure rvh1 is placed on swapped out list of the current tab.
  EXPECT_TRUE(manager->IsSwappedOut(rvh1));
  EXPECT_EQ(rvh1,
            manager->GetSwappedOutRenderViewHost(rvh1->GetSiteInstance()));

  // Ensure a swapped out RVH is created in the first opener tab.
  TestRenderViewHost* opener1_rvh = static_cast<TestRenderViewHost*>(
      opener1_manager->GetSwappedOutRenderViewHost(rvh2->GetSiteInstance()));
  EXPECT_TRUE(opener1_manager->IsSwappedOut(opener1_rvh));
  EXPECT_TRUE(opener1_rvh->is_swapped_out());

  // Ensure a swapped out RVH is created in the second opener tab.
  TestRenderViewHost* opener2_rvh = static_cast<TestRenderViewHost*>(
      opener2_manager->GetSwappedOutRenderViewHost(rvh2->GetSiteInstance()));
  EXPECT_TRUE(opener2_manager->IsSwappedOut(opener2_rvh));
  EXPECT_TRUE(opener2_rvh->is_swapped_out());

  // Navigate to a cross-BrowsingInstance URL.
  contents()->NavigateAndCommit(kChromeUrl);
  TestRenderViewHost* rvh3 = test_rvh();
  EXPECT_NE(rvh1->GetSiteInstance(), rvh3->GetSiteInstance());
  EXPECT_FALSE(rvh1->GetSiteInstance()->IsRelatedSiteInstance(
                   rvh3->GetSiteInstance()));

  // No scripting is allowed across BrowsingInstances, so we should not create
  // swapped out RVHs for the opener chain in this case.
  EXPECT_FALSE(opener1_manager->GetSwappedOutRenderViewHost(
                   rvh3->GetSiteInstance()));
  EXPECT_FALSE(opener2_manager->GetSwappedOutRenderViewHost(
                   rvh3->GetSiteInstance()));
}

// Test that RenderViewHosts created for WebUI navigations are properly
// granted WebUI bindings even if an unprivileged swapped out RenderViewHost
// is in the same process (http://crbug.com/79918).
TEST_F(RenderViewHostManagerTest, EnableWebUIWithSwappedOutOpener) {
  set_should_create_webui(true);
  const GURL kSettingsUrl("chrome://chrome/settings");
  const GURL kPluginUrl("chrome://plugins");

  // Navigate to an initial WebUI URL.
  contents()->NavigateAndCommit(kSettingsUrl);

  // Ensure the RVH has WebUI bindings.
  TestRenderViewHost* rvh1 = test_rvh();
  EXPECT_TRUE(rvh1->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);

  // Create a new tab and simulate it being the opener for the main
  // tab.  It should be in the same SiteInstance.
  scoped_ptr<TestWebContents> opener1(
      TestWebContents::Create(browser_context(), rvh1->GetSiteInstance()));
  RenderViewHostManager* opener1_manager =
      opener1->GetRenderManagerForTesting();
  contents()->SetOpener(opener1.get());

  // Navigate to a different WebUI URL (different SiteInstance, same
  // BrowsingInstance).
  contents()->NavigateAndCommit(kPluginUrl);
  TestRenderViewHost* rvh2 = test_rvh();
  EXPECT_NE(rvh1->GetSiteInstance(), rvh2->GetSiteInstance());
  EXPECT_TRUE(rvh1->GetSiteInstance()->IsRelatedSiteInstance(
                  rvh2->GetSiteInstance()));

  // Ensure a swapped out RVH is created in the first opener tab.
  TestRenderViewHost* opener1_rvh = static_cast<TestRenderViewHost*>(
      opener1_manager->GetSwappedOutRenderViewHost(rvh2->GetSiteInstance()));
  EXPECT_TRUE(opener1_manager->IsSwappedOut(opener1_rvh));
  EXPECT_TRUE(opener1_rvh->is_swapped_out());

  // Ensure the new RVH has WebUI bindings.
  EXPECT_TRUE(rvh2->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);
}

// Test that we reuse the same guest SiteInstance if we navigate across sites.
TEST_F(RenderViewHostManagerTest, NoSwapOnGuestNavigations) {
  TestNotificationTracker notifications;

  GURL guest_url(std::string(chrome::kGuestScheme).append("://abc123"));
  SiteInstance* instance =
      SiteInstance::CreateForURL(browser_context(), guest_url);
  scoped_ptr<TestWebContents> web_contents(
      TestWebContents::Create(browser_context(), instance));

  // Create.
  RenderViewHostManager manager(web_contents.get(), web_contents.get(),
                                web_contents.get());

  manager.Init(browser_context(), instance, MSG_ROUTING_NONE);

  RenderViewHost* host;

  // 1) The first navigation. --------------------------
  const GURL kUrl1("http://www.google.com/");
  NavigationEntryImpl entry1(
      NULL /* instance */, -1 /* page_id */, kUrl1, Referrer(),
      string16() /* title */, PAGE_TRANSITION_TYPED,
      false /* is_renderer_init */);
  host = manager.Navigate(entry1);

  // The RenderViewHost created in Init will be reused.
  EXPECT_TRUE(host == manager.current_host());
  EXPECT_FALSE(manager.pending_render_view_host());
  EXPECT_EQ(manager.current_host()->GetSiteInstance(), instance);

  // Commit.
  manager.DidNavigateMainFrame(host);
  // Commit to SiteInstance should be delayed until RenderView commit.
  EXPECT_EQ(host, manager.current_host());
  ASSERT_TRUE(host);
  EXPECT_TRUE(static_cast<SiteInstanceImpl*>(host->GetSiteInstance())->
      HasSite());

  // 2) Navigate to a different domain. -------------------------
  // Guests stay in the same process on navigation.
  const GURL kUrl2("http://www.chromium.org");
  NavigationEntryImpl entry2(
      NULL /* instance */, -1 /* page_id */, kUrl2,
      Referrer(kUrl1, WebKit::WebReferrerPolicyDefault),
      string16() /* title */, PAGE_TRANSITION_LINK,
      true /* is_renderer_init */);
  host = manager.Navigate(entry2);

  // The RenderViewHost created in Init will be reused.
  EXPECT_EQ(host, manager.current_host());
  EXPECT_FALSE(manager.pending_render_view_host());

  // Commit.
  manager.DidNavigateMainFrame(host);
  EXPECT_EQ(host, manager.current_host());
  ASSERT_TRUE(host);
  EXPECT_EQ(static_cast<SiteInstanceImpl*>(host->GetSiteInstance()),
      instance);
}

}  // namespace content
