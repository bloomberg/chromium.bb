// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_url_handler.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/test_notification_tracker.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/render_view_host_manager.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/webkit_glue.h"

class RenderViewHostManagerTest : public RenderViewHostTestHarness {
 public:
  void NavigateActiveAndCommit(const GURL& url) {
    // Note: we navigate the active RenderViewHost because previous navigations
    // won't have committed yet, so NavigateAndCommit does the wrong thing
    // for us.
    controller().LoadURL(url, GURL(), PageTransition::LINK);
    active_rvh()->SendNavigate(
        static_cast<MockRenderProcessHost*>(active_rvh()->process())->
            max_page_id() + 1,
        url);
  }

  bool ShouldSwapProcesses(RenderViewHostManager* manager,
                           const NavigationEntry* cur_entry,
                           const NavigationEntry* new_entry) const {
    return manager->ShouldSwapProcessesForNavigation(cur_entry, new_entry);
  }
};

// Tests that when you navigate from the New TabPage to another page, and
// then do that same thing in another tab, that the two resulting pages have
// different SiteInstances, BrowsingInstances, and RenderProcessHosts. This is
// a regression test for bug 9364.
TEST_F(RenderViewHostManagerTest, NewTabPageProcesses) {
  BrowserThread ui_thread(BrowserThread::UI, MessageLoop::current());
  GURL ntp(chrome::kChromeUINewTabURL);
  GURL dest("http://www.google.com/");

  // Navigate our first tab to the new tab page and then to the destination.
  NavigateActiveAndCommit(ntp);
  NavigateActiveAndCommit(dest);

  // Make a second tab.
  TestTabContents contents2(profile_.get(), NULL);

  // Load the two URLs in the second tab. Note that the first navigation creates
  // a RVH that's not pending (since there is no cross-site transition), so
  // we use the committed one, but the second one is the opposite.
  contents2.controller().LoadURL(ntp, GURL(), PageTransition::LINK);
  static_cast<TestRenderViewHost*>(contents2.render_manager()->
     current_host())->SendNavigate(100, ntp);
  contents2.controller().LoadURL(dest, GURL(), PageTransition::LINK);
  static_cast<TestRenderViewHost*>(contents2.render_manager()->
      pending_render_view_host())->SendNavigate(101, dest);

  // The two RVH's should be different in every way.
  EXPECT_NE(active_rvh()->process(), contents2.render_view_host()->process());
  EXPECT_NE(active_rvh()->site_instance(),
      contents2.render_view_host()->site_instance());
  EXPECT_NE(active_rvh()->site_instance()->browsing_instance(),
      contents2.render_view_host()->site_instance()->browsing_instance());

  // Navigate both to the new tab page, and verify that they share a
  // SiteInstance.
  NavigateActiveAndCommit(ntp);

  contents2.controller().LoadURL(ntp, GURL(), PageTransition::LINK);
  static_cast<TestRenderViewHost*>(contents2.render_manager()->
     pending_render_view_host())->SendNavigate(102, ntp);

  EXPECT_EQ(active_rvh()->site_instance(),
      contents2.render_view_host()->site_instance());
}

// When there is an error with the specified page, renderer exits view-source
// mode. See WebFrameImpl::DidFail(). We check by this test that
// EnableViewSourceMode message is sent on every navigation regardless
// RenderView is being newly created or reused.
TEST_F(RenderViewHostManagerTest, AlwaysSendEnableViewSourceMode) {
  BrowserThread ui_thread(BrowserThread::UI, MessageLoop::current());
  const GURL kNtpUrl(chrome::kChromeUINewTabURL);
  const GURL kUrl("view-source:http://foo");

  // We have to navigate to some page at first since without this, the first
  // navigation will reuse the SiteInstance created by Init(), and the second
  // one will create a new SiteInstance. Because current_instance and
  // new_instance will be different, a new RenderViewHost will be created for
  // the second navigation. We have to avoid this in order to exercise the
  // target code patch.
  NavigateActiveAndCommit(kNtpUrl);

  // Navigate.
  controller().LoadURL(kUrl, GURL() /* referer */, PageTransition::TYPED);
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
  controller().LoadURL(kUrl, GURL() /* referer */, PageTransition::TYPED);
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
  // Using TestingProfile.
  SiteInstance* instance = SiteInstance::CreateSiteInstance(profile_.get());
  EXPECT_FALSE(instance->has_site());

  TestTabContents tab_contents(profile_.get(), instance);
  RenderViewHostManager manager(&tab_contents, &tab_contents);

  manager.Init(profile_.get(), instance, MSG_ROUTING_NONE);

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

  SiteInstance* instance = SiteInstance::CreateSiteInstance(profile_.get());

  TestTabContents tab_contents(profile_.get(), instance);
  notifications.ListenFor(NotificationType::RENDER_VIEW_HOST_CHANGED,
                     Source<NavigationController>(&tab_contents.controller()));

  // Create.
  RenderViewHostManager manager(&tab_contents, &tab_contents);

  manager.Init(profile_.get(), instance, MSG_ROUTING_NONE);

  RenderViewHost* host;

  // 1) The first navigation. --------------------------
  GURL url1("http://www.google.com/");
  NavigationEntry entry1(NULL /* instance */, -1 /* page_id */, url1,
                         GURL() /* referrer */, string16() /* title */,
                         PageTransition::TYPED);
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
  host->site_instance()->SetSite(url1);

  // 2) Navigate to next site. -------------------------
  GURL url2("http://www.google.com/foo");
  NavigationEntry entry2(NULL /* instance */, -1 /* page_id */, url2,
                         url1 /* referrer */, string16() /* title */,
                         PageTransition::LINK);
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
  GURL url3("http://webkit.org/");
  NavigationEntry entry3(NULL /* instance */, -1 /* page_id */, url3,
                         url2 /* referrer */, string16() /* title */,
                         PageTransition::LINK);
  host = manager.Navigate(entry3);

  // A new RenderViewHost should be created.
  EXPECT_TRUE(manager.pending_render_view_host());
  EXPECT_TRUE(host == manager.pending_render_view_host());

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
      NotificationType::RENDER_VIEW_HOST_CHANGED));
}

// Tests WebUI creation.
TEST_F(RenderViewHostManagerTest, WebUI) {
  BrowserThread ui_thread(BrowserThread::UI, MessageLoop::current());
  SiteInstance* instance = SiteInstance::CreateSiteInstance(profile_.get());

  TestTabContents tab_contents(profile_.get(), instance);
  RenderViewHostManager manager(&tab_contents, &tab_contents);

  manager.Init(profile_.get(), instance, MSG_ROUTING_NONE);

  GURL url(chrome::kChromeUINewTabURL);
  NavigationEntry entry(NULL /* instance */, -1 /* page_id */, url,
                        GURL() /* referrer */, string16() /* title */,
                        PageTransition::TYPED);
  RenderViewHost* host = manager.Navigate(entry);

  EXPECT_TRUE(host);
  EXPECT_TRUE(host == manager.current_host());
  EXPECT_FALSE(manager.pending_render_view_host());

  // It's important that the site instance get set on the Web UI page as soon
  // as the navigation starts, rather than lazily after it commits, so we don't
  // try to re-use the SiteInstance/process for non DOM-UI things that may
  // get loaded in between.
  EXPECT_TRUE(host->site_instance()->has_site());
  EXPECT_EQ(url, host->site_instance()->site());

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
  BrowserThread thread(BrowserThread::UI, &message_loop_);
  SiteInstance* instance = SiteInstance::CreateSiteInstance(profile_.get());
  TestTabContents tab_contents(profile_.get(), instance);
  RenderViewHostManager manager(&tab_contents, &tab_contents);
  manager.Init(profile_.get(), instance, MSG_ROUTING_NONE);

  // NTP is a Web UI page.
  GURL ntp_url(chrome::kChromeUINewTabURL);
  NavigationEntry ntp_entry(NULL /* instance */, -1 /* page_id */, ntp_url,
                            GURL() /* referrer */, string16() /* title */,
                            PageTransition::TYPED);

  // about: URLs are not Web UI pages.
  GURL about_url(chrome::kAboutMemoryURL);
  // Rewrite so it looks like chrome://about/memory
  bool reverse_on_redirect = false;
  BrowserURLHandler::RewriteURLIfNecessary(
      &about_url, profile_.get(), &reverse_on_redirect);
  NavigationEntry about_entry(NULL /* instance */, -1 /* page_id */, about_url,
                              GURL() /* referrer */, string16() /* title */,
                              PageTransition::TYPED);

  EXPECT_TRUE(ShouldSwapProcesses(&manager, &ntp_entry, &about_entry));
}

// Tests that we don't end up in an inconsistent state if a page does a back and
// then reload. http://crbug.com/51680
TEST_F(RenderViewHostManagerTest, PageDoesBackAndReload) {
  GURL url1("http://www.google.com/");
  GURL url2("http://www.evil-site.com/");

  // Navigate to a safe site, then an evil site.
  // This will switch RenderViewHosts.  We cannot assert that the first and
  // second RVHs are different, though, because the first one may be promptly
  // deleted.
  contents()->NavigateAndCommit(url1);
  contents()->NavigateAndCommit(url2);
  RenderViewHost* evil_rvh = contents()->render_view_host();

  // Now let's simulate the evil page calling history.back().
  contents()->OnGoToEntryAtOffset(-1);
  // We should have a new pending RVH.
  // Note that in this case, the navigation has not committed, so evil_rvh will
  // not be deleted yet.
  EXPECT_NE(evil_rvh, contents()->render_manager()->pending_render_view_host());

  // Before that RVH has committed, the evil page reloads itself.
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 1;
  params.url = url2;
  params.transition = PageTransition::CLIENT_REDIRECT;
  params.should_update_history = false;
  params.gesture = NavigationGestureAuto;
  params.was_within_same_page = false;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url2));
  contents()->TestDidNavigate(evil_rvh, params);

  // That should have cancelled the pending RVH, and the evil RVH should be the
  // current one.
  EXPECT_TRUE(contents()->render_manager()->pending_render_view_host() == NULL);
  EXPECT_EQ(evil_rvh, contents()->render_manager()->current_host());

  // Also we should not have a pending navigation entry.
  NavigationEntry* entry = contents()->controller().GetActiveEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(url2, entry->url());
}
