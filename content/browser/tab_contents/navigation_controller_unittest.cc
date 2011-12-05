// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
//  These are only used for commented out tests.  If someone wants to enable
//  them, they should be moved to chrome first.
//  #include "chrome/browser/history/history.h"
//  #include "chrome/browser/profiles/profile_manager.h"
//  #include "chrome/browser/sessions/session_service.h"
//  #include "chrome/browser/sessions/session_service_factory.h"
//  #include "chrome/browser/sessions/session_service_test_helper.h"
//  #include "chrome/browser/sessions/session_types.h"
#include "content/browser/site_instance.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/navigation_details.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "content/common/view_messages.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"
#include "content/test/test_browser_context.h"
#include "content/test/test_notification_tracker.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/webkit_glue.h"

using base::Time;

// NavigationControllerTest ----------------------------------------------------

class NavigationControllerTest : public RenderViewHostTestHarness {
 public:
  NavigationControllerTest() {}
};

void RegisterForAllNavNotifications(TestNotificationTracker* tracker,
                                    NavigationController* controller) {
  tracker->ListenFor(content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                     content::Source<NavigationController>(controller));
  tracker->ListenFor(content::NOTIFICATION_NAV_LIST_PRUNED,
                     content::Source<NavigationController>(controller));
  tracker->ListenFor(content::NOTIFICATION_NAV_ENTRY_CHANGED,
                     content::Source<NavigationController>(controller));
}

class TestTabContentsDelegate : public TabContentsDelegate {
 public:
  explicit TestTabContentsDelegate() :
      navigation_state_change_count_(0) {}

  int navigation_state_change_count() {
    return navigation_state_change_count_;
  }

  // Keep track of whether the tab has notified us of a navigation state change.
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags) {
    navigation_state_change_count_++;
  }

 private:
  // The number of times NavigationStateChanged has been called.
  int navigation_state_change_count_;
};

// -----------------------------------------------------------------------------

TEST_F(NavigationControllerTest, Defaults) {
  EXPECT_FALSE(controller().pending_entry());
  EXPECT_FALSE(controller().GetLastCommittedEntry());
  EXPECT_EQ(controller().pending_entry_index(), -1);
  EXPECT_EQ(controller().last_committed_entry_index(), -1);
  EXPECT_EQ(controller().entry_count(), 0);
  EXPECT_FALSE(controller().CanGoBack());
  EXPECT_FALSE(controller().CanGoForward());
}

TEST_F(NavigationControllerTest, LoadURL) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  controller().LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  // Creating a pending notification should not have issued any of the
  // notifications we're listening for.
  EXPECT_EQ(0U, notifications.size());

  // The load should now be pending.
  EXPECT_EQ(controller().entry_count(), 0);
  EXPECT_EQ(controller().last_committed_entry_index(), -1);
  EXPECT_EQ(controller().pending_entry_index(), -1);
  EXPECT_FALSE(controller().GetLastCommittedEntry());
  EXPECT_TRUE(controller().pending_entry());
  EXPECT_FALSE(controller().CanGoBack());
  EXPECT_FALSE(controller().CanGoForward());
  EXPECT_EQ(contents()->GetMaxPageID(), -1);

  // We should have gotten no notifications from the preceeding checks.
  EXPECT_EQ(0U, notifications.size());

  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // The load should now be committed.
  EXPECT_EQ(controller().entry_count(), 1);
  EXPECT_EQ(controller().last_committed_entry_index(), 0);
  EXPECT_EQ(controller().pending_entry_index(), -1);
  EXPECT_TRUE(controller().GetLastCommittedEntry());
  EXPECT_FALSE(controller().pending_entry());
  EXPECT_FALSE(controller().CanGoBack());
  EXPECT_FALSE(controller().CanGoForward());
  EXPECT_EQ(contents()->GetMaxPageID(), 0);

  // Load another...
  controller().LoadURL(
      url2, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());

  // The load should now be pending.
  EXPECT_EQ(controller().entry_count(), 1);
  EXPECT_EQ(controller().last_committed_entry_index(), 0);
  EXPECT_EQ(controller().pending_entry_index(), -1);
  EXPECT_TRUE(controller().GetLastCommittedEntry());
  EXPECT_TRUE(controller().pending_entry());
  // TODO(darin): maybe this should really be true?
  EXPECT_FALSE(controller().CanGoBack());
  EXPECT_FALSE(controller().CanGoForward());
  EXPECT_EQ(contents()->GetMaxPageID(), 0);

  // Simulate the beforeunload ack for the cross-site transition, and then the
  // commit.
  rvh()->SendShouldCloseACK(true);
  contents()->pending_rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // The load should now be committed.
  EXPECT_EQ(controller().entry_count(), 2);
  EXPECT_EQ(controller().last_committed_entry_index(), 1);
  EXPECT_EQ(controller().pending_entry_index(), -1);
  EXPECT_TRUE(controller().GetLastCommittedEntry());
  EXPECT_FALSE(controller().pending_entry());
  EXPECT_TRUE(controller().CanGoBack());
  EXPECT_FALSE(controller().CanGoForward());
  EXPECT_EQ(contents()->GetMaxPageID(), 1);
}

// Tests what happens when the same page is loaded again.  Should not create a
// new session history entry. This is what happens when you press enter in the
// URL bar to reload: a pending entry is created and then it is discarded when
// the load commits (because WebCore didn't actually make a new entry).
TEST_F(NavigationControllerTest, LoadURL_SamePage) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  const GURL url1("http://foo1");

  controller().LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(0U, notifications.size());
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller().LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(0U, notifications.size());
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // We should not have produced a new session history entry.
  EXPECT_EQ(controller().entry_count(), 1);
  EXPECT_EQ(controller().last_committed_entry_index(), 0);
  EXPECT_EQ(controller().pending_entry_index(), -1);
  EXPECT_TRUE(controller().GetLastCommittedEntry());
  EXPECT_FALSE(controller().pending_entry());
  EXPECT_FALSE(controller().CanGoBack());
  EXPECT_FALSE(controller().CanGoForward());
}

// Tests loading a URL but discarding it before the load commits.
TEST_F(NavigationControllerTest, LoadURL_Discarded) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  controller().LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(0U, notifications.size());
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller().LoadURL(
      url2, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  controller().DiscardNonCommittedEntries();
  EXPECT_EQ(0U, notifications.size());

  // Should not have produced a new session history entry.
  EXPECT_EQ(controller().entry_count(), 1);
  EXPECT_EQ(controller().last_committed_entry_index(), 0);
  EXPECT_EQ(controller().pending_entry_index(), -1);
  EXPECT_TRUE(controller().GetLastCommittedEntry());
  EXPECT_FALSE(controller().pending_entry());
  EXPECT_FALSE(controller().CanGoBack());
  EXPECT_FALSE(controller().CanGoForward());
}

// Tests navigations that come in unrequested. This happens when the user
// navigates from the web page, and here we test that there is no pending entry.
TEST_F(NavigationControllerTest, LoadURL_NoPending) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  // First make an existing committed entry.
  const GURL kExistingURL1("http://eh");
  controller().LoadURL(kExistingURL1, content::Referrer(),
                       content::PAGE_TRANSITION_TYPED, std::string());
  rvh()->SendNavigate(0, kExistingURL1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Do a new navigation without making a pending one.
  const GURL kNewURL("http://see");
  rvh()->SendNavigate(99, kNewURL);

  // There should no longer be any pending entry, and the third navigation we
  // just made should be committed.
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(-1, controller().pending_entry_index());
  EXPECT_EQ(1, controller().last_committed_entry_index());
  EXPECT_EQ(kNewURL, controller().GetActiveEntry()->url());
}

// Tests navigating to a new URL when there is a new pending navigation that is
// not the one that just loaded. This will happen if the user types in a URL to
// somewhere slow, and then navigates the current page before the typed URL
// commits.
TEST_F(NavigationControllerTest, LoadURL_NewPending) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  // First make an existing committed entry.
  const GURL kExistingURL1("http://eh");
  controller().LoadURL(kExistingURL1, content::Referrer(),
                       content::PAGE_TRANSITION_TYPED, std::string());
  rvh()->SendNavigate(0, kExistingURL1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Make a pending entry to somewhere new.
  const GURL kExistingURL2("http://bee");
  controller().LoadURL(kExistingURL2, content::Referrer(),
                       content::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(0U, notifications.size());

  // After the beforeunload but before it commits, do a new navigation.
  rvh()->SendShouldCloseACK(true);
  const GURL kNewURL("http://see");
  contents()->pending_rvh()->SendNavigate(3, kNewURL);

  // There should no longer be any pending entry, and the third navigation we
  // just made should be committed.
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(-1, controller().pending_entry_index());
  EXPECT_EQ(1, controller().last_committed_entry_index());
  EXPECT_EQ(kNewURL, controller().GetActiveEntry()->url());
}

// Tests navigating to a new URL when there is a pending back/forward
// navigation. This will happen if the user hits back, but before that commits,
// they navigate somewhere new.
TEST_F(NavigationControllerTest, LoadURL_ExistingPending) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  // First make some history.
  const GURL kExistingURL1("http://foo/eh");
  controller().LoadURL(kExistingURL1, content::Referrer(),
                       content::PAGE_TRANSITION_TYPED, std::string());
  rvh()->SendNavigate(0, kExistingURL1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  const GURL kExistingURL2("http://foo/bee");
  controller().LoadURL(kExistingURL2, content::Referrer(),
                       content::PAGE_TRANSITION_TYPED, std::string());
  rvh()->SendNavigate(1, kExistingURL2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Now make a pending back/forward navigation. The zeroth entry should be
  // pending.
  controller().GoBack();
  EXPECT_EQ(0U, notifications.size());
  EXPECT_EQ(0, controller().pending_entry_index());
  EXPECT_EQ(1, controller().last_committed_entry_index());

  // Before that commits, do a new navigation.
  const GURL kNewURL("http://foo/see");
  content::LoadCommittedDetails details;
  rvh()->SendNavigate(3, kNewURL);

  // There should no longer be any pending entry, and the third navigation we
  // just made should be committed.
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(-1, controller().pending_entry_index());
  EXPECT_EQ(2, controller().last_committed_entry_index());
  EXPECT_EQ(kNewURL, controller().GetActiveEntry()->url());
}

// Tests navigating to an existing URL when there is a pending new navigation.
// This will happen if the user enters a URL, but before that commits, the
// current page fires history.back().
TEST_F(NavigationControllerTest, LoadURL_BackPreemptsPending) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  // First make some history.
  const GURL kExistingURL1("http://foo/eh");
  controller().LoadURL(kExistingURL1, content::Referrer(),
                       content::PAGE_TRANSITION_TYPED, std::string());
  rvh()->SendNavigate(0, kExistingURL1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  const GURL kExistingURL2("http://foo/bee");
  controller().LoadURL(kExistingURL2, content::Referrer(),
                       content::PAGE_TRANSITION_TYPED, std::string());
  rvh()->SendNavigate(1, kExistingURL2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Now make a pending new navigation.
  const GURL kNewURL("http://foo/see");
  controller().LoadURL(
      kNewURL, content::Referrer(), content::PAGE_TRANSITION_TYPED,
      std::string());
  EXPECT_EQ(0U, notifications.size());
  EXPECT_EQ(-1, controller().pending_entry_index());
  EXPECT_EQ(1, controller().last_committed_entry_index());

  // Before that commits, a back navigation from the renderer commits.
  rvh()->SendNavigate(0, kExistingURL1);

  // There should no longer be any pending entry, and the back navigation we
  // just made should be committed.
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(-1, controller().pending_entry_index());
  EXPECT_EQ(0, controller().last_committed_entry_index());
  EXPECT_EQ(kExistingURL1, controller().GetActiveEntry()->url());
}

// Tests an ignored navigation when there is a pending new navigation.
// This will happen if the user enters a URL, but before that commits, the
// current blank page reloads.  See http://crbug.com/77507.
TEST_F(NavigationControllerTest, LoadURL_IgnorePreemptsPending) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  // Set a TabContentsDelegate to listen for state changes.
  scoped_ptr<TestTabContentsDelegate> delegate(new TestTabContentsDelegate());
  EXPECT_FALSE(contents()->delegate());
  contents()->set_delegate(delegate.get());

  // Without any navigations, the renderer starts at about:blank.
  const GURL kExistingURL("about:blank");

  // Now make a pending new navigation.
  const GURL kNewURL("http://eh");
  controller().LoadURL(
      kNewURL, content::Referrer(), content::PAGE_TRANSITION_TYPED,
      std::string());
  EXPECT_EQ(0U, notifications.size());
  EXPECT_EQ(-1, controller().pending_entry_index());
  EXPECT_TRUE(controller().pending_entry());
  EXPECT_EQ(-1, controller().last_committed_entry_index());
  EXPECT_EQ(1, delegate->navigation_state_change_count());

  // Before that commits, a document.write and location.reload can cause the
  // renderer to send a FrameNavigate with page_id -1.
  rvh()->SendNavigate(-1, kExistingURL);

  // This should clear the pending entry and notify of a navigation state
  // change, so that we do not keep displaying kNewURL.
  EXPECT_EQ(-1, controller().pending_entry_index());
  EXPECT_FALSE(controller().pending_entry());
  EXPECT_EQ(-1, controller().last_committed_entry_index());
  EXPECT_EQ(2, delegate->navigation_state_change_count());

  contents()->set_delegate(NULL);
}

// Tests that the pending entry state is correct after an abort.
TEST_F(NavigationControllerTest, LoadURL_AbortCancelsPending) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  // Set a TabContentsDelegate to listen for state changes.
  scoped_ptr<TestTabContentsDelegate> delegate(new TestTabContentsDelegate());
  EXPECT_FALSE(contents()->delegate());
  contents()->set_delegate(delegate.get());

  // Without any navigations, the renderer starts at about:blank.
  const GURL kExistingURL("about:blank");

  // Now make a pending new navigation.
  const GURL kNewURL("http://eh");
  controller().LoadURL(
      kNewURL, content::Referrer(), content::PAGE_TRANSITION_TYPED,
      std::string());
  EXPECT_EQ(0U, notifications.size());
  EXPECT_EQ(-1, controller().pending_entry_index());
  EXPECT_TRUE(controller().pending_entry());
  EXPECT_EQ(-1, controller().last_committed_entry_index());
  EXPECT_EQ(1, delegate->navigation_state_change_count());

  // It may abort before committing, if it's a download or due to a stop or
  // a new navigation from the user.
  ViewHostMsg_DidFailProvisionalLoadWithError_Params params;
  params.frame_id = 1;
  params.is_main_frame = true;
  params.error_code = net::ERR_ABORTED;
  params.error_description = string16();
  params.url = kNewURL;
  params.showing_repost_interstitial = false;
  rvh()->TestOnMessageReceived(
          ViewHostMsg_DidFailProvisionalLoadWithError(0,  // routing_id
                                                      params));

  // This should clear the pending entry and notify of a navigation state
  // change, so that we do not keep displaying kNewURL.
  EXPECT_EQ(-1, controller().pending_entry_index());
  EXPECT_FALSE(controller().pending_entry());
  EXPECT_EQ(-1, controller().last_committed_entry_index());
  EXPECT_EQ(2, delegate->navigation_state_change_count());

  contents()->set_delegate(NULL);
}

// Tests that the pending entry state is correct after a redirect and abort.
// http://crbug.com/83031.
TEST_F(NavigationControllerTest, LoadURL_RedirectAbortCancelsPending) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  // Set a TabContentsDelegate to listen for state changes.
  scoped_ptr<TestTabContentsDelegate> delegate(new TestTabContentsDelegate());
  EXPECT_FALSE(contents()->delegate());
  contents()->set_delegate(delegate.get());

  // Without any navigations, the renderer starts at about:blank.
  const GURL kExistingURL("about:blank");

  // Now make a pending new navigation.
  const GURL kNewURL("http://eh");
  controller().LoadURL(
      kNewURL, content::Referrer(), content::PAGE_TRANSITION_TYPED,
      std::string());
  EXPECT_EQ(0U, notifications.size());
  EXPECT_EQ(-1, controller().pending_entry_index());
  EXPECT_TRUE(controller().pending_entry());
  EXPECT_EQ(-1, controller().last_committed_entry_index());
  EXPECT_EQ(1, delegate->navigation_state_change_count());

  // Now the navigation redirects.
  const GURL kRedirectURL("http://bee");
  rvh()->TestOnMessageReceived(
      ViewHostMsg_DidRedirectProvisionalLoad(0,  // routing_id
                                             -1,  // pending page_id
                                             GURL(),  // opener
                                             kNewURL,  // old url
                                             kRedirectURL));  // new url

  // We don't want to change the NavigationEntry's url, in case it cancels.
  // Prevents regression of http://crbug.com/77786.
  EXPECT_EQ(kNewURL, controller().pending_entry()->url());

  // It may abort before committing, if it's a download or due to a stop or
  // a new navigation from the user.
  ViewHostMsg_DidFailProvisionalLoadWithError_Params params;
  params.frame_id = 1;
  params.is_main_frame = true;
  params.error_code = net::ERR_ABORTED;
  params.error_description = string16();
  params.url = kRedirectURL;
  params.showing_repost_interstitial = false;
  rvh()->TestOnMessageReceived(
          ViewHostMsg_DidFailProvisionalLoadWithError(0,  // routing_id
                                                      params));

  // This should clear the pending entry and notify of a navigation state
  // change, so that we do not keep displaying kNewURL.
  EXPECT_EQ(-1, controller().pending_entry_index());
  EXPECT_FALSE(controller().pending_entry());
  EXPECT_EQ(-1, controller().last_committed_entry_index());
  EXPECT_EQ(2, delegate->navigation_state_change_count());

  contents()->set_delegate(NULL);
}

TEST_F(NavigationControllerTest, Reload) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  const GURL url1("http://foo1");

  controller().LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(0U, notifications.size());
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller().Reload(true);
  EXPECT_EQ(0U, notifications.size());

  // The reload is pending.
  EXPECT_EQ(controller().entry_count(), 1);
  EXPECT_EQ(controller().last_committed_entry_index(), 0);
  EXPECT_EQ(controller().pending_entry_index(), 0);
  EXPECT_TRUE(controller().GetLastCommittedEntry());
  EXPECT_TRUE(controller().pending_entry());
  EXPECT_FALSE(controller().CanGoBack());
  EXPECT_FALSE(controller().CanGoForward());

  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Now the reload is committed.
  EXPECT_EQ(controller().entry_count(), 1);
  EXPECT_EQ(controller().last_committed_entry_index(), 0);
  EXPECT_EQ(controller().pending_entry_index(), -1);
  EXPECT_TRUE(controller().GetLastCommittedEntry());
  EXPECT_FALSE(controller().pending_entry());
  EXPECT_FALSE(controller().CanGoBack());
  EXPECT_FALSE(controller().CanGoForward());
}

// Tests what happens when a reload navigation produces a new page.
TEST_F(NavigationControllerTest, Reload_GeneratesNewPage) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  controller().LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller().Reload(true);
  EXPECT_EQ(0U, notifications.size());

  rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Now the reload is committed.
  EXPECT_EQ(controller().entry_count(), 2);
  EXPECT_EQ(controller().last_committed_entry_index(), 1);
  EXPECT_EQ(controller().pending_entry_index(), -1);
  EXPECT_TRUE(controller().GetLastCommittedEntry());
  EXPECT_FALSE(controller().pending_entry());
  EXPECT_TRUE(controller().CanGoBack());
  EXPECT_FALSE(controller().CanGoForward());
}

// Tests what happens when we navigate back successfully
TEST_F(NavigationControllerTest, Back) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  const GURL url1("http://foo1");
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  const GURL url2("http://foo2");
  rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller().GoBack();
  EXPECT_EQ(0U, notifications.size());

  // We should now have a pending navigation to go back.
  EXPECT_EQ(controller().entry_count(), 2);
  EXPECT_EQ(controller().last_committed_entry_index(), 1);
  EXPECT_EQ(controller().pending_entry_index(), 0);
  EXPECT_TRUE(controller().GetLastCommittedEntry());
  EXPECT_TRUE(controller().pending_entry());
  EXPECT_FALSE(controller().CanGoBack());
  EXPECT_TRUE(controller().CanGoForward());

  rvh()->SendNavigate(0, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // The back navigation completed successfully.
  EXPECT_EQ(controller().entry_count(), 2);
  EXPECT_EQ(controller().last_committed_entry_index(), 0);
  EXPECT_EQ(controller().pending_entry_index(), -1);
  EXPECT_TRUE(controller().GetLastCommittedEntry());
  EXPECT_FALSE(controller().pending_entry());
  EXPECT_FALSE(controller().CanGoBack());
  EXPECT_TRUE(controller().CanGoForward());
}

// Tests what happens when a back navigation produces a new page.
TEST_F(NavigationControllerTest, Back_GeneratesNewPage) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");

  controller().LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller().LoadURL(
      url2, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller().GoBack();
  EXPECT_EQ(0U, notifications.size());

  // We should now have a pending navigation to go back.
  EXPECT_EQ(controller().entry_count(), 2);
  EXPECT_EQ(controller().last_committed_entry_index(), 1);
  EXPECT_EQ(controller().pending_entry_index(), 0);
  EXPECT_TRUE(controller().GetLastCommittedEntry());
  EXPECT_TRUE(controller().pending_entry());
  EXPECT_FALSE(controller().CanGoBack());
  EXPECT_TRUE(controller().CanGoForward());

  rvh()->SendNavigate(2, url3);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // The back navigation resulted in a completely new navigation.
  // TODO(darin): perhaps this behavior will be confusing to users?
  EXPECT_EQ(controller().entry_count(), 3);
  EXPECT_EQ(controller().last_committed_entry_index(), 2);
  EXPECT_EQ(controller().pending_entry_index(), -1);
  EXPECT_TRUE(controller().GetLastCommittedEntry());
  EXPECT_FALSE(controller().pending_entry());
  EXPECT_TRUE(controller().CanGoBack());
  EXPECT_FALSE(controller().CanGoForward());
}

// Receives a back message when there is a new pending navigation entry.
TEST_F(NavigationControllerTest, Back_NewPending) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  const GURL kUrl1("http://foo1");
  const GURL kUrl2("http://foo2");
  const GURL kUrl3("http://foo3");

  // First navigate two places so we have some back history.
  rvh()->SendNavigate(0, kUrl1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // controller().LoadURL(kUrl2, content::PAGE_TRANSITION_TYPED);
  rvh()->SendNavigate(1, kUrl2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Now start a new pending navigation and go back before it commits.
  controller().LoadURL(
      kUrl3, content::Referrer(), content::PAGE_TRANSITION_TYPED,
      std::string());
  EXPECT_EQ(-1, controller().pending_entry_index());
  EXPECT_EQ(kUrl3, controller().pending_entry()->url());
  controller().GoBack();

  // The pending navigation should now be the "back" item and the new one
  // should be gone.
  EXPECT_EQ(0, controller().pending_entry_index());
  EXPECT_EQ(kUrl1, controller().pending_entry()->url());
}

// Receives a back message when there is a different renavigation already
// pending.
TEST_F(NavigationControllerTest, Back_OtherBackPending) {
  const GURL kUrl1("http://foo/1");
  const GURL kUrl2("http://foo/2");
  const GURL kUrl3("http://foo/3");

  // First navigate three places so we have some back history.
  rvh()->SendNavigate(0, kUrl1);
  rvh()->SendNavigate(1, kUrl2);
  rvh()->SendNavigate(2, kUrl3);

  // With nothing pending, say we get a navigation to the second entry.
  rvh()->SendNavigate(1, kUrl2);

  // We know all the entries have the same site instance, so we can just grab
  // a random one for looking up other entries.
  SiteInstance* site_instance =
      controller().GetLastCommittedEntry()->site_instance();

  // That second URL should be the last committed and it should have gotten the
  // new title.
  EXPECT_EQ(kUrl2, controller().GetEntryWithPageID(site_instance, 1)->url());
  EXPECT_EQ(1, controller().last_committed_entry_index());
  EXPECT_EQ(-1, controller().pending_entry_index());

  // Now go forward to the last item again and say it was committed.
  controller().GoForward();
  rvh()->SendNavigate(2, kUrl3);

  // Now start going back one to the second page. It will be pending.
  controller().GoBack();
  EXPECT_EQ(1, controller().pending_entry_index());
  EXPECT_EQ(2, controller().last_committed_entry_index());

  // Not synthesize a totally new back event to the first page. This will not
  // match the pending one.
  rvh()->SendNavigate(0, kUrl1);

  // The committed navigation should clear the pending entry.
  EXPECT_EQ(-1, controller().pending_entry_index());

  // But the navigated entry should be the last committed.
  EXPECT_EQ(0, controller().last_committed_entry_index());
  EXPECT_EQ(kUrl1, controller().GetLastCommittedEntry()->url());
}

// Tests what happens when we navigate forward successfully.
TEST_F(NavigationControllerTest, Forward) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller().GoBack();
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller().GoForward();

  // We should now have a pending navigation to go forward.
  EXPECT_EQ(controller().entry_count(), 2);
  EXPECT_EQ(controller().last_committed_entry_index(), 0);
  EXPECT_EQ(controller().pending_entry_index(), 1);
  EXPECT_TRUE(controller().GetLastCommittedEntry());
  EXPECT_TRUE(controller().pending_entry());
  EXPECT_TRUE(controller().CanGoBack());
  EXPECT_FALSE(controller().CanGoForward());

  rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // The forward navigation completed successfully.
  EXPECT_EQ(controller().entry_count(), 2);
  EXPECT_EQ(controller().last_committed_entry_index(), 1);
  EXPECT_EQ(controller().pending_entry_index(), -1);
  EXPECT_TRUE(controller().GetLastCommittedEntry());
  EXPECT_FALSE(controller().pending_entry());
  EXPECT_TRUE(controller().CanGoBack());
  EXPECT_FALSE(controller().CanGoForward());
}

// Tests what happens when a forward navigation produces a new page.
TEST_F(NavigationControllerTest, Forward_GeneratesNewPage) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");
  const GURL url3("http://foo3");

  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller().GoBack();
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller().GoForward();
  EXPECT_EQ(0U, notifications.size());

  // Should now have a pending navigation to go forward.
  EXPECT_EQ(controller().entry_count(), 2);
  EXPECT_EQ(controller().last_committed_entry_index(), 0);
  EXPECT_EQ(controller().pending_entry_index(), 1);
  EXPECT_TRUE(controller().GetLastCommittedEntry());
  EXPECT_TRUE(controller().pending_entry());
  EXPECT_TRUE(controller().CanGoBack());
  EXPECT_FALSE(controller().CanGoForward());

  rvh()->SendNavigate(2, url3);
  EXPECT_TRUE(notifications.Check2AndReset(
      content::NOTIFICATION_NAV_LIST_PRUNED,
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  EXPECT_EQ(controller().entry_count(), 2);
  EXPECT_EQ(controller().last_committed_entry_index(), 1);
  EXPECT_EQ(controller().pending_entry_index(), -1);
  EXPECT_TRUE(controller().GetLastCommittedEntry());
  EXPECT_FALSE(controller().pending_entry());
  EXPECT_TRUE(controller().CanGoBack());
  EXPECT_FALSE(controller().CanGoForward());
}

// Two consequent navigation for the same URL entered in should be considered
// as SAME_PAGE navigation even when we are redirected to some other page.
TEST_F(NavigationControllerTest, Redirect) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");  // Redirection target

  // First request
  controller().LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());

  EXPECT_EQ(0U, notifications.size());
  rvh()->SendNavigate(0, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Second request
  controller().LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());

  EXPECT_TRUE(controller().pending_entry());
  EXPECT_EQ(controller().pending_entry_index(), -1);
  EXPECT_EQ(url1, controller().GetActiveEntry()->url());

  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 0;
  params.url = url2;
  params.transition = content::PAGE_TRANSITION_SERVER_REDIRECT;
  params.redirects.push_back(GURL("http://foo1"));
  params.redirects.push_back(GURL("http://foo2"));
  params.should_update_history = false;
  params.gesture = NavigationGestureAuto;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url2));

  content::LoadCommittedDetails details;

  EXPECT_EQ(0U, notifications.size());
  EXPECT_TRUE(controller().RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  EXPECT_TRUE(details.type == content::NAVIGATION_TYPE_SAME_PAGE);
  EXPECT_EQ(controller().entry_count(), 1);
  EXPECT_EQ(controller().last_committed_entry_index(), 0);
  EXPECT_TRUE(controller().GetLastCommittedEntry());
  EXPECT_EQ(controller().pending_entry_index(), -1);
  EXPECT_FALSE(controller().pending_entry());
  EXPECT_EQ(url2, controller().GetActiveEntry()->url());

  EXPECT_FALSE(controller().CanGoBack());
  EXPECT_FALSE(controller().CanGoForward());
}

// Similar to Redirect above, but the first URL is requested by POST,
// the second URL is requested by GET. NavigationEntry::has_post_data_
// must be cleared. http://crbug.com/21245
TEST_F(NavigationControllerTest, PostThenRedirect) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");  // Redirection target

  // First request as POST
  controller().LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  controller().GetActiveEntry()->set_has_post_data(true);

  EXPECT_EQ(0U, notifications.size());
  rvh()->SendNavigate(0, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Second request
  controller().LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());

  EXPECT_TRUE(controller().pending_entry());
  EXPECT_EQ(controller().pending_entry_index(), -1);
  EXPECT_EQ(url1, controller().GetActiveEntry()->url());

  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 0;
  params.url = url2;
  params.transition = content::PAGE_TRANSITION_SERVER_REDIRECT;
  params.redirects.push_back(GURL("http://foo1"));
  params.redirects.push_back(GURL("http://foo2"));
  params.should_update_history = false;
  params.gesture = NavigationGestureAuto;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url2));

  content::LoadCommittedDetails details;

  EXPECT_EQ(0U, notifications.size());
  EXPECT_TRUE(controller().RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  EXPECT_TRUE(details.type == content::NAVIGATION_TYPE_SAME_PAGE);
  EXPECT_EQ(controller().entry_count(), 1);
  EXPECT_EQ(controller().last_committed_entry_index(), 0);
  EXPECT_TRUE(controller().GetLastCommittedEntry());
  EXPECT_EQ(controller().pending_entry_index(), -1);
  EXPECT_FALSE(controller().pending_entry());
  EXPECT_EQ(url2, controller().GetActiveEntry()->url());
  EXPECT_FALSE(controller().GetActiveEntry()->has_post_data());

  EXPECT_FALSE(controller().CanGoBack());
  EXPECT_FALSE(controller().CanGoForward());
}

// A redirect right off the bat should be a NEW_PAGE.
TEST_F(NavigationControllerTest, ImmediateRedirect) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");  // Redirection target

  // First request
  controller().LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());

  EXPECT_TRUE(controller().pending_entry());
  EXPECT_EQ(controller().pending_entry_index(), -1);
  EXPECT_EQ(url1, controller().GetActiveEntry()->url());

  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 0;
  params.url = url2;
  params.transition = content::PAGE_TRANSITION_SERVER_REDIRECT;
  params.redirects.push_back(GURL("http://foo1"));
  params.redirects.push_back(GURL("http://foo2"));
  params.should_update_history = false;
  params.gesture = NavigationGestureAuto;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url2));

  content::LoadCommittedDetails details;

  EXPECT_EQ(0U, notifications.size());
  EXPECT_TRUE(controller().RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  EXPECT_TRUE(details.type == content::NAVIGATION_TYPE_NEW_PAGE);
  EXPECT_EQ(controller().entry_count(), 1);
  EXPECT_EQ(controller().last_committed_entry_index(), 0);
  EXPECT_TRUE(controller().GetLastCommittedEntry());
  EXPECT_EQ(controller().pending_entry_index(), -1);
  EXPECT_FALSE(controller().pending_entry());
  EXPECT_EQ(url2, controller().GetActiveEntry()->url());

  EXPECT_FALSE(controller().CanGoBack());
  EXPECT_FALSE(controller().CanGoForward());
}

// Tests navigation via link click within a subframe. A new navigation entry
// should be created.
TEST_F(NavigationControllerTest, NewSubframe) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  const GURL url1("http://foo1");
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  const GURL url2("http://foo2");
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 1;
  params.url = url2;
  params.transition = content::PAGE_TRANSITION_MANUAL_SUBFRAME;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url2));

  content::LoadCommittedDetails details;
  EXPECT_TRUE(controller().RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(url1, details.previous_url);
  EXPECT_FALSE(details.is_in_page);
  EXPECT_FALSE(details.is_main_frame);

  // The new entry should be appended.
  EXPECT_EQ(2, controller().entry_count());

  // New entry should refer to the new page, but the old URL (entries only
  // reflect the toplevel URL).
  EXPECT_EQ(url1, details.entry->url());
  EXPECT_EQ(params.page_id, details.entry->page_id());
}

// Some pages create a popup, then write an iframe into it. This causes a
// subframe navigation without having any committed entry. Such navigations
// just get thrown on the ground, but we shouldn't crash.
TEST_F(NavigationControllerTest, SubframeOnEmptyPage) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  // Navigation controller currently has no entries.
  const GURL url("http://foo2");
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 1;
  params.url = url;
  params.transition = content::PAGE_TRANSITION_AUTO_SUBFRAME;
  params.should_update_history = false;
  params.gesture = NavigationGestureAuto;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url));

  content::LoadCommittedDetails details;
  EXPECT_FALSE(controller().RendererDidNavigate(params, &details));
  EXPECT_EQ(0U, notifications.size());
}

// Auto subframes are ones the page loads automatically like ads. They should
// not create new navigation entries.
TEST_F(NavigationControllerTest, AutoSubframe) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  const GURL url1("http://foo1");
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  const GURL url2("http://foo2");
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 0;
  params.url = url2;
  params.transition = content::PAGE_TRANSITION_AUTO_SUBFRAME;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url2));

  // Navigating should do nothing.
  content::LoadCommittedDetails details;
  EXPECT_FALSE(controller().RendererDidNavigate(params, &details));
  EXPECT_EQ(0U, notifications.size());

  // There should still be only one entry.
  EXPECT_EQ(1, controller().entry_count());
}

// Tests navigation and then going back to a subframe navigation.
TEST_F(NavigationControllerTest, BackSubframe) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  // Main page.
  const GURL url1("http://foo1");
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // First manual subframe navigation.
  const GURL url2("http://foo2");
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 1;
  params.url = url2;
  params.transition = content::PAGE_TRANSITION_MANUAL_SUBFRAME;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url2));

  // This should generate a new entry.
  content::LoadCommittedDetails details;
  EXPECT_TRUE(controller().RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(2, controller().entry_count());

  // Second manual subframe navigation should also make a new entry.
  const GURL url3("http://foo3");
  params.page_id = 2;
  params.url = url3;
  EXPECT_TRUE(controller().RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(3, controller().entry_count());
  EXPECT_EQ(2, controller().GetCurrentEntryIndex());

  // Go back one.
  controller().GoBack();
  params.url = url2;
  params.page_id = 1;
  EXPECT_TRUE(controller().RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(3, controller().entry_count());
  EXPECT_EQ(1, controller().GetCurrentEntryIndex());

  // Go back one more.
  controller().GoBack();
  params.url = url1;
  params.page_id = 0;
  EXPECT_TRUE(controller().RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(3, controller().entry_count());
  EXPECT_EQ(0, controller().GetCurrentEntryIndex());
}

TEST_F(NavigationControllerTest, LinkClick) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Should not have produced a new session history entry.
  EXPECT_EQ(controller().entry_count(), 2);
  EXPECT_EQ(controller().last_committed_entry_index(), 1);
  EXPECT_EQ(controller().pending_entry_index(), -1);
  EXPECT_TRUE(controller().GetLastCommittedEntry());
  EXPECT_FALSE(controller().pending_entry());
  EXPECT_TRUE(controller().CanGoBack());
  EXPECT_FALSE(controller().CanGoForward());
}

TEST_F(NavigationControllerTest, InPage) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  // Main page.
  const GURL url1("http://foo");
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // First navigation.
  const GURL url2("http://foo#a");
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 1;
  params.url = url2;
  params.transition = content::PAGE_TRANSITION_LINK;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url2));

  // This should generate a new entry.
  content::LoadCommittedDetails details;
  EXPECT_TRUE(controller().RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_TRUE(details.is_in_page);
  EXPECT_FALSE(details.did_replace_entry);
  EXPECT_EQ(2, controller().entry_count());

  // Go back one.
  ViewHostMsg_FrameNavigate_Params back_params(params);
  controller().GoBack();
  back_params.url = url1;
  back_params.page_id = 0;
  EXPECT_TRUE(controller().RendererDidNavigate(back_params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  // is_in_page is false in that case but should be true.
  // See comment in AreURLsInPageNavigation() in navigation_controller.cc
  // EXPECT_TRUE(details.is_in_page);
  EXPECT_EQ(2, controller().entry_count());
  EXPECT_EQ(0, controller().GetCurrentEntryIndex());
  EXPECT_EQ(back_params.url, controller().GetActiveEntry()->url());

  // Go forward
  ViewHostMsg_FrameNavigate_Params forward_params(params);
  controller().GoForward();
  forward_params.url = url2;
  forward_params.page_id = 1;
  EXPECT_TRUE(controller().RendererDidNavigate(forward_params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_TRUE(details.is_in_page);
  EXPECT_EQ(2, controller().entry_count());
  EXPECT_EQ(1, controller().GetCurrentEntryIndex());
  EXPECT_EQ(forward_params.url,
            controller().GetActiveEntry()->url());

  // Now go back and forward again. This is to work around a bug where we would
  // compare the incoming URL with the last committed entry rather than the
  // one identified by an existing page ID. This would result in the second URL
  // losing the reference fragment when you navigate away from it and then back.
  controller().GoBack();
  EXPECT_TRUE(controller().RendererDidNavigate(back_params, &details));
  controller().GoForward();
  EXPECT_TRUE(controller().RendererDidNavigate(forward_params, &details));
  EXPECT_EQ(forward_params.url,
            controller().GetActiveEntry()->url());

  // Finally, navigate to an unrelated URL to make sure in_page is not sticky.
  const GURL url3("http://bar");
  params.page_id = 2;
  params.url = url3;
  notifications.Reset();
  EXPECT_TRUE(controller().RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_FALSE(details.is_in_page);
}

TEST_F(NavigationControllerTest, InPage_Replace) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  // Main page.
  const GURL url1("http://foo");
  rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // First navigation.
  const GURL url2("http://foo#a");
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 0;  // Same page_id
  params.url = url2;
  params.transition = content::PAGE_TRANSITION_LINK;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url2));

  // This should NOT generate a new entry, nor prune the list.
  content::LoadCommittedDetails details;
  EXPECT_TRUE(controller().RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_TRUE(details.is_in_page);
  EXPECT_TRUE(details.did_replace_entry);
  EXPECT_EQ(1, controller().entry_count());
}

// Tests for http://crbug.com/40395
// Simulates this:
//   <script>
//     window.location.replace("#a");
//     window.location='http://foo3/';
//   </script>
TEST_F(NavigationControllerTest, ClientRedirectAfterInPageNavigation) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  // Load an initial page.
  {
    const GURL url("http://foo/");
    rvh()->SendNavigate(0, url);
    EXPECT_TRUE(notifications.Check1AndReset(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  }

  // Navigate to a new page.
  {
    const GURL url("http://foo2/");
    rvh()->SendNavigate(1, url);
    controller().DocumentLoadedInFrame();
    EXPECT_TRUE(notifications.Check1AndReset(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  }

  // Navigate within the page.
  {
    const GURL url("http://foo2/#a");
    ViewHostMsg_FrameNavigate_Params params;
    params.page_id = 1;  // Same page_id
    params.url = url;
    params.transition = content::PAGE_TRANSITION_LINK;
    params.redirects.push_back(url);
    params.should_update_history = true;
    params.gesture = NavigationGestureUnknown;
    params.is_post = false;
    params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url));

    // This should NOT generate a new entry, nor prune the list.
    content::LoadCommittedDetails details;
    EXPECT_TRUE(controller().RendererDidNavigate(params, &details));
    EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
    EXPECT_TRUE(details.is_in_page);
    EXPECT_TRUE(details.did_replace_entry);
    EXPECT_EQ(2, controller().entry_count());
  }

  // Perform a client redirect to a new page.
  {
    const GURL url("http://foo3/");
    ViewHostMsg_FrameNavigate_Params params;
    params.page_id = 2;  // New page_id
    params.url = url;
    params.transition = content::PAGE_TRANSITION_CLIENT_REDIRECT;
    params.redirects.push_back(GURL("http://foo2/#a"));
    params.redirects.push_back(url);
    params.should_update_history = true;
    params.gesture = NavigationGestureUnknown;
    params.is_post = false;
    params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url));

    // This SHOULD generate a new entry.
    content::LoadCommittedDetails details;
    EXPECT_TRUE(controller().RendererDidNavigate(params, &details));
    EXPECT_TRUE(notifications.Check1AndReset(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED));
    EXPECT_FALSE(details.is_in_page);
    EXPECT_EQ(3, controller().entry_count());
  }

  // Verify that BACK brings us back to http://foo2/.
  {
    const GURL url("http://foo2/");
    controller().GoBack();
    rvh()->SendNavigate(1, url);
    EXPECT_TRUE(notifications.Check1AndReset(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED));
    EXPECT_EQ(url, controller().GetActiveEntry()->url());
  }
}

// NotificationObserver implementation used in verifying we've received the
// content::NOTIFICATION_NAV_LIST_PRUNED method.
class PrunedListener : public content::NotificationObserver {
 public:
  explicit PrunedListener(NavigationController* controller)
      : notification_count_(0) {
    registrar_.Add(this, content::NOTIFICATION_NAV_LIST_PRUNED,
                   content::Source<NavigationController>(controller));
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
    if (type == content::NOTIFICATION_NAV_LIST_PRUNED) {
      notification_count_++;
      details_ = *(content::Details<content::PrunedDetails>(details).ptr());
    }
  }

  // Number of times NAV_LIST_PRUNED has been observed.
  int notification_count_;

  // Details from the last NAV_LIST_PRUNED.
  content::PrunedDetails details_;

 private:
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(PrunedListener);
};

// Tests that we limit the number of navigation entries created correctly.
TEST_F(NavigationControllerTest, EnforceMaxNavigationCount) {
  size_t original_count = NavigationController::max_entry_count();
  const int kMaxEntryCount = 5;

  NavigationController::set_max_entry_count_for_testing(kMaxEntryCount);

  int url_index;
  // Load up to the max count, all entries should be there.
  for (url_index = 0; url_index < kMaxEntryCount; url_index++) {
    GURL url(StringPrintf("http://www.a.com/%d", url_index));
    controller().LoadURL(
        url, content::Referrer(), content::PAGE_TRANSITION_TYPED,
        std::string());
    rvh()->SendNavigate(url_index, url);
  }

  EXPECT_EQ(controller().entry_count(), kMaxEntryCount);

  // Created a PrunedListener to observe prune notifications.
  PrunedListener listener(&controller());

  // Navigate some more.
  GURL url(StringPrintf("http://www.a.com/%d", url_index));
  controller().LoadURL(
      url, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  rvh()->SendNavigate(url_index, url);
  url_index++;

  // We should have got a pruned navigation.
  EXPECT_EQ(1, listener.notification_count_);
  EXPECT_TRUE(listener.details_.from_front);
  EXPECT_EQ(1, listener.details_.count);

  // We expect http://www.a.com/0 to be gone.
  EXPECT_EQ(controller().entry_count(), kMaxEntryCount);
  EXPECT_EQ(controller().GetEntryAtIndex(0)->url(),
            GURL("http:////www.a.com/1"));

  // More navigations.
  for (int i = 0; i < 3; i++) {
    url = GURL(StringPrintf("http:////www.a.com/%d", url_index));
    controller().LoadURL(
        url, content::Referrer(), content::PAGE_TRANSITION_TYPED,
        std::string());
    rvh()->SendNavigate(url_index, url);
    url_index++;
  }
  EXPECT_EQ(controller().entry_count(), kMaxEntryCount);
  EXPECT_EQ(controller().GetEntryAtIndex(0)->url(),
            GURL("http:////www.a.com/4"));

  NavigationController::set_max_entry_count_for_testing(original_count);
}

// Tests that we can do a restore and navigate to the restored entries and
// everything is updated properly. This can be tricky since there is no
// SiteInstance for the entries created initially.
TEST_F(NavigationControllerTest, RestoreNavigate) {
  // Create a NavigationController with a restored set of tabs.
  GURL url("http://foo");
  std::vector<NavigationEntry*> entries;
  NavigationEntry* entry = NavigationController::CreateNavigationEntry(
      url, content::Referrer(), content::PAGE_TRANSITION_RELOAD, false,
      std::string(), browser_context());
  entry->set_page_id(0);
  entry->set_title(ASCIIToUTF16("Title"));
  entry->set_content_state("state");
  entries.push_back(entry);
  TabContents our_contents(
      browser_context(), NULL, MSG_ROUTING_NONE, NULL, NULL);
  NavigationController& our_controller = our_contents.controller();
  our_controller.Restore(0, true, &entries);
  ASSERT_EQ(0u, entries.size());

  // Before navigating to the restored entry, it should have a restore_type
  // and no SiteInstance.
  EXPECT_EQ(NavigationEntry::RESTORE_LAST_SESSION,
            our_controller.GetEntryAtIndex(0)->restore_type());
  EXPECT_FALSE(our_controller.GetEntryAtIndex(0)->site_instance());

  // After navigating, we should have one entry, and it should be "pending".
  // It should now have a SiteInstance and no restore_type.
  our_controller.GoToIndex(0);
  EXPECT_EQ(1, our_controller.entry_count());
  EXPECT_EQ(our_controller.GetEntryAtIndex(0),
            our_controller.pending_entry());
  EXPECT_EQ(0, our_controller.GetEntryAtIndex(0)->page_id());
  EXPECT_EQ(NavigationEntry::RESTORE_NONE,
            our_controller.GetEntryAtIndex(0)->restore_type());
  EXPECT_TRUE(our_controller.GetEntryAtIndex(0)->site_instance());

  // Say we navigated to that entry.
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 0;
  params.url = url;
  params.transition = content::PAGE_TRANSITION_LINK;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url));
  content::LoadCommittedDetails details;
  our_controller.RendererDidNavigate(params, &details);

  // There should be no longer any pending entry and one committed one. This
  // means that we were able to locate the entry, assign its site instance, and
  // commit it properly.
  EXPECT_EQ(1, our_controller.entry_count());
  EXPECT_EQ(0, our_controller.last_committed_entry_index());
  EXPECT_FALSE(our_controller.pending_entry());
  EXPECT_EQ(url,
            our_controller.GetLastCommittedEntry()->site_instance()->site());
  EXPECT_EQ(NavigationEntry::RESTORE_NONE,
            our_controller.GetEntryAtIndex(0)->restore_type());
}

// Tests that we can still navigate to a restored entry after a different
// navigation fails and clears the pending entry.  http://crbug.com/90085
TEST_F(NavigationControllerTest, RestoreNavigateAfterFailure) {
  // Create a NavigationController with a restored set of tabs.
  GURL url("http://foo");
  std::vector<NavigationEntry*> entries;
  NavigationEntry* entry = NavigationController::CreateNavigationEntry(
      url, content::Referrer(), content::PAGE_TRANSITION_RELOAD, false,
      std::string(), browser_context());
  entry->set_page_id(0);
  entry->set_title(ASCIIToUTF16("Title"));
  entry->set_content_state("state");
  entries.push_back(entry);
  TabContents our_contents(
      browser_context(), NULL, MSG_ROUTING_NONE, NULL, NULL);
  NavigationController& our_controller = our_contents.controller();
  our_controller.Restore(0, true, &entries);
  ASSERT_EQ(0u, entries.size());

  // Before navigating to the restored entry, it should have a restore_type
  // and no SiteInstance.
  EXPECT_EQ(NavigationEntry::RESTORE_LAST_SESSION,
            our_controller.GetEntryAtIndex(0)->restore_type());
  EXPECT_FALSE(our_controller.GetEntryAtIndex(0)->site_instance());

  // After navigating, we should have one entry, and it should be "pending".
  // It should now have a SiteInstance and no restore_type.
  our_controller.GoToIndex(0);
  EXPECT_EQ(1, our_controller.entry_count());
  EXPECT_EQ(our_controller.GetEntryAtIndex(0),
            our_controller.pending_entry());
  EXPECT_EQ(0, our_controller.GetEntryAtIndex(0)->page_id());
  EXPECT_EQ(NavigationEntry::RESTORE_NONE,
            our_controller.GetEntryAtIndex(0)->restore_type());
  EXPECT_TRUE(our_controller.GetEntryAtIndex(0)->site_instance());

  // This pending navigation may have caused a different navigation to fail,
  // which causes the pending entry to be cleared.
  TestRenderViewHost* rvh =
      static_cast<TestRenderViewHost*>(our_contents.render_view_host());
  ViewHostMsg_DidFailProvisionalLoadWithError_Params fail_load_params;
  fail_load_params.frame_id = 1;
  fail_load_params.is_main_frame = true;
  fail_load_params.error_code = net::ERR_ABORTED;
  fail_load_params.error_description = string16();
  fail_load_params.url = url;
  fail_load_params.showing_repost_interstitial = false;
  rvh->TestOnMessageReceived(
          ViewHostMsg_DidFailProvisionalLoadWithError(0,  // routing_id
                                                      fail_load_params));

  // Now the pending restored entry commits.
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 0;
  params.url = url;
  params.transition = content::PAGE_TRANSITION_LINK;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url));
  content::LoadCommittedDetails details;
  our_controller.RendererDidNavigate(params, &details);

  // There should be no pending entry and one committed one.
  EXPECT_EQ(1, our_controller.entry_count());
  EXPECT_EQ(0, our_controller.last_committed_entry_index());
  EXPECT_FALSE(our_controller.pending_entry());
  EXPECT_EQ(url,
            our_controller.GetLastCommittedEntry()->site_instance()->site());
  EXPECT_EQ(NavigationEntry::RESTORE_NONE,
            our_controller.GetEntryAtIndex(0)->restore_type());
}

// Make sure that the page type and stuff is correct after an interstitial.
TEST_F(NavigationControllerTest, Interstitial) {
  // First navigate somewhere normal.
  const GURL url1("http://foo");
  controller().LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  rvh()->SendNavigate(0, url1);

  // Now navigate somewhere with an interstitial.
  const GURL url2("http://bar");
  controller().LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  controller().pending_entry()->set_page_type(content::PAGE_TYPE_INTERSTITIAL);

  // At this point the interstitial will be displayed and the load will still
  // be pending. If the user continues, the load will commit.
  rvh()->SendNavigate(1, url2);

  // The page should be a normal page again.
  EXPECT_EQ(url2, controller().GetLastCommittedEntry()->url());
  EXPECT_EQ(content::PAGE_TYPE_NORMAL,
            controller().GetLastCommittedEntry()->page_type());
}

TEST_F(NavigationControllerTest, RemoveEntry) {
  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");
  const GURL url4("http://foo/4");
  const GURL url5("http://foo/5");
  const GURL pending_url("http://foo/pending");
  const GURL default_url("http://foo/default");

  controller().LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  rvh()->SendNavigate(0, url1);
  controller().LoadURL(
      url2, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  rvh()->SendNavigate(1, url2);
  controller().LoadURL(
      url3, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  rvh()->SendNavigate(2, url3);
  controller().LoadURL(
      url4, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  rvh()->SendNavigate(3, url4);
  controller().LoadURL(
      url5, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  rvh()->SendNavigate(4, url5);

  // Remove the last entry.
  controller().RemoveEntryAtIndex(
      controller().entry_count() - 1, default_url);
  EXPECT_EQ(4, controller().entry_count());
  EXPECT_EQ(3, controller().last_committed_entry_index());
  NavigationEntry* pending_entry = controller().pending_entry();
  EXPECT_TRUE(pending_entry && pending_entry->url() == url4);

  // Add a pending entry.
  controller().LoadURL(pending_url, content::Referrer(),
                       content::PAGE_TRANSITION_TYPED, std::string());
  // Now remove the last entry.
  controller().RemoveEntryAtIndex(
      controller().entry_count() - 1, default_url);
  // The pending entry should have been discarded and the last committed entry
  // removed.
  EXPECT_EQ(3, controller().entry_count());
  EXPECT_EQ(2, controller().last_committed_entry_index());
  pending_entry = controller().pending_entry();
  EXPECT_TRUE(pending_entry && pending_entry->url() == url3);

  // Remove an entry which is not the last committed one.
  controller().RemoveEntryAtIndex(0, default_url);
  EXPECT_EQ(2, controller().entry_count());
  EXPECT_EQ(1, controller().last_committed_entry_index());
  // No navigation should have been initiated since we did not remove the
  // current entry.
  EXPECT_FALSE(controller().pending_entry());

  // Remove the 2 remaining entries.
  controller().RemoveEntryAtIndex(1, default_url);
  controller().RemoveEntryAtIndex(0, default_url);

  // This should have created a pending default entry.
  EXPECT_EQ(0, controller().entry_count());
  EXPECT_EQ(-1, controller().last_committed_entry_index());
  pending_entry = controller().pending_entry();
  EXPECT_TRUE(pending_entry && pending_entry->url() == default_url);
}

// Tests the transient entry, making sure it goes away with all navigations.
TEST_F(NavigationControllerTest, TransientEntry) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  const GURL url0("http://foo/0");
  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");
  const GURL url4("http://foo/4");
  const GURL transient_url("http://foo/transient");

  controller().LoadURL(
      url0, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  rvh()->SendNavigate(0, url0);
  controller().LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  rvh()->SendNavigate(1, url1);

  notifications.Reset();

  // Adding a transient with no pending entry.
  NavigationEntry* transient_entry = new NavigationEntry;
  transient_entry->set_url(transient_url);
  controller().AddTransientEntry(transient_entry);

  // We should not have received any notifications.
  EXPECT_EQ(0U, notifications.size());

  // Check our state.
  EXPECT_EQ(transient_url, controller().GetActiveEntry()->url());
  EXPECT_EQ(controller().entry_count(), 3);
  EXPECT_EQ(controller().last_committed_entry_index(), 1);
  EXPECT_EQ(controller().pending_entry_index(), -1);
  EXPECT_TRUE(controller().GetLastCommittedEntry());
  EXPECT_FALSE(controller().pending_entry());
  EXPECT_TRUE(controller().CanGoBack());
  EXPECT_FALSE(controller().CanGoForward());
  EXPECT_EQ(contents()->GetMaxPageID(), 1);

  // Navigate.
  controller().LoadURL(
      url2, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  rvh()->SendNavigate(2, url2);

  // We should have navigated, transient entry should be gone.
  EXPECT_EQ(url2, controller().GetActiveEntry()->url());
  EXPECT_EQ(controller().entry_count(), 3);

  // Add a transient again, then navigate with no pending entry this time.
  transient_entry = new NavigationEntry;
  transient_entry->set_url(transient_url);
  controller().AddTransientEntry(transient_entry);
  EXPECT_EQ(transient_url, controller().GetActiveEntry()->url());
  rvh()->SendNavigate(3, url3);
  // Transient entry should be gone.
  EXPECT_EQ(url3, controller().GetActiveEntry()->url());
  EXPECT_EQ(controller().entry_count(), 4);

  // Initiate a navigation, add a transient then commit navigation.
  controller().LoadURL(
      url4, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  transient_entry = new NavigationEntry;
  transient_entry->set_url(transient_url);
  controller().AddTransientEntry(transient_entry);
  EXPECT_EQ(transient_url, controller().GetActiveEntry()->url());
  rvh()->SendNavigate(4, url4);
  EXPECT_EQ(url4, controller().GetActiveEntry()->url());
  EXPECT_EQ(controller().entry_count(), 5);

  // Add a transient and go back.  This should simply remove the transient.
  transient_entry = new NavigationEntry;
  transient_entry->set_url(transient_url);
  controller().AddTransientEntry(transient_entry);
  EXPECT_EQ(transient_url, controller().GetActiveEntry()->url());
  EXPECT_TRUE(controller().CanGoBack());
  EXPECT_FALSE(controller().CanGoForward());
  controller().GoBack();
  // Transient entry should be gone.
  EXPECT_EQ(url4, controller().GetActiveEntry()->url());
  EXPECT_EQ(controller().entry_count(), 5);
  rvh()->SendNavigate(3, url3);

  // Add a transient and go to an entry before the current one.
  transient_entry = new NavigationEntry;
  transient_entry->set_url(transient_url);
  controller().AddTransientEntry(transient_entry);
  EXPECT_EQ(transient_url, controller().GetActiveEntry()->url());
  controller().GoToIndex(1);
  // The navigation should have been initiated, transient entry should be gone.
  EXPECT_EQ(url1, controller().GetActiveEntry()->url());
  // Visible entry does not update for history navigations until commit.
  EXPECT_EQ(url3, controller().GetVisibleEntry()->url());
  rvh()->SendNavigate(1, url1);
  EXPECT_EQ(url1, controller().GetVisibleEntry()->url());

  // Add a transient and go to an entry after the current one.
  transient_entry = new NavigationEntry;
  transient_entry->set_url(transient_url);
  controller().AddTransientEntry(transient_entry);
  EXPECT_EQ(transient_url, controller().GetActiveEntry()->url());
  controller().GoToIndex(3);
  // The navigation should have been initiated, transient entry should be gone.
  // Because of the transient entry that is removed, going to index 3 makes us
  // land on url2 (which is visible after the commit).
  EXPECT_EQ(url2, controller().GetActiveEntry()->url());
  EXPECT_EQ(url1, controller().GetVisibleEntry()->url());
  rvh()->SendNavigate(2, url2);
  EXPECT_EQ(url2, controller().GetVisibleEntry()->url());

  // Add a transient and go forward.
  transient_entry = new NavigationEntry;
  transient_entry->set_url(transient_url);
  controller().AddTransientEntry(transient_entry);
  EXPECT_EQ(transient_url, controller().GetActiveEntry()->url());
  EXPECT_TRUE(controller().CanGoForward());
  controller().GoForward();
  // We should have navigated, transient entry should be gone.
  EXPECT_EQ(url3, controller().GetActiveEntry()->url());
  EXPECT_EQ(url2, controller().GetVisibleEntry()->url());
  rvh()->SendNavigate(3, url3);
  EXPECT_EQ(url3, controller().GetVisibleEntry()->url());

  // Ensure the URLS are correct.
  EXPECT_EQ(controller().entry_count(), 5);
  EXPECT_EQ(controller().GetEntryAtIndex(0)->url(), url0);
  EXPECT_EQ(controller().GetEntryAtIndex(1)->url(), url1);
  EXPECT_EQ(controller().GetEntryAtIndex(2)->url(), url2);
  EXPECT_EQ(controller().GetEntryAtIndex(3)->url(), url3);
  EXPECT_EQ(controller().GetEntryAtIndex(4)->url(), url4);
}

// Tests that the URLs for renderer-initiated navigations are not displayed to
// the user until the navigation commits, to prevent URL spoof attacks.
// See http://crbug.com/99016.
TEST_F(NavigationControllerTest, DontShowRendererURLUntilCommit) {
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller());

  const GURL url0("http://foo/0");
  const GURL url1("http://foo/1");

  // For typed navigations (browser-initiated), both active and visible entries
  // should update before commit.
  controller().LoadURL(url0, content::Referrer(),
                       content::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(url0, controller().GetActiveEntry()->url());
  EXPECT_EQ(url0, controller().GetVisibleEntry()->url());
  rvh()->SendNavigate(0, url0);

  // For link clicks (renderer-initiated navigations), the active entry should
  // update before commit but the visible should not.
  controller().LoadURLFromRenderer(url1, content::Referrer(),
                                   content::PAGE_TRANSITION_LINK,
                                   std::string());
  EXPECT_EQ(url1, controller().GetActiveEntry()->url());
  EXPECT_EQ(url0, controller().GetVisibleEntry()->url());
  EXPECT_TRUE(controller().pending_entry()->is_renderer_initiated());

  // After commit, both should be updated, and we should no longer treat the
  // entry as renderer-initiated.
  rvh()->SendNavigate(1, url1);
  EXPECT_EQ(url1, controller().GetActiveEntry()->url());
  EXPECT_EQ(url1, controller().GetVisibleEntry()->url());
  EXPECT_FALSE(controller().GetLastCommittedEntry()->is_renderer_initiated());

  notifications.Reset();
}

// Tests that IsInPageNavigation returns appropriate results.  Prevents
// regression for bug 1126349.
TEST_F(NavigationControllerTest, IsInPageNavigation) {
  // Navigate to URL with no refs.
  const GURL url("http://www.google.com/home.html");
  rvh()->SendNavigate(0, url);

  // Reloading the page is not an in-page navigation.
  EXPECT_FALSE(controller().IsURLInPageNavigation(url));
  const GURL other_url("http://www.google.com/add.html");
  EXPECT_FALSE(controller().IsURLInPageNavigation(other_url));
  const GURL url_with_ref("http://www.google.com/home.html#my_ref");
  EXPECT_TRUE(controller().IsURLInPageNavigation(url_with_ref));

  // Navigate to URL with refs.
  rvh()->SendNavigate(1, url_with_ref);

  // Reloading the page is not an in-page navigation.
  EXPECT_FALSE(controller().IsURLInPageNavigation(url_with_ref));
  EXPECT_FALSE(controller().IsURLInPageNavigation(url));
  EXPECT_FALSE(controller().IsURLInPageNavigation(other_url));
  const GURL other_url_with_ref("http://www.google.com/home.html#my_other_ref");
  EXPECT_TRUE(controller().IsURLInPageNavigation(
      other_url_with_ref));
}

// Some pages can have subframes with the same base URL (minus the reference) as
// the main page. Even though this is hard, it can happen, and we don't want
// these subframe navigations to affect the toplevel document. They should
// instead be ignored.  http://crbug.com/5585
TEST_F(NavigationControllerTest, SameSubframe) {
  // Navigate the main frame.
  const GURL url("http://www.google.com/");
  rvh()->SendNavigate(0, url);

  // We should be at the first navigation entry.
  EXPECT_EQ(controller().entry_count(), 1);
  EXPECT_EQ(controller().last_committed_entry_index(), 0);

  // Navigate a subframe that would normally count as in-page.
  const GURL subframe("http://www.google.com/#");
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 0;
  params.url = subframe;
  params.transition = content::PAGE_TRANSITION_AUTO_SUBFRAME;
  params.should_update_history = false;
  params.gesture = NavigationGestureAuto;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(subframe));
  content::LoadCommittedDetails details;
  EXPECT_FALSE(controller().RendererDidNavigate(params, &details));

  // Nothing should have changed.
  EXPECT_EQ(controller().entry_count(), 1);
  EXPECT_EQ(controller().last_committed_entry_index(), 0);
}

// Make sure that on cloning a tabcontents and going back needs_reload is false.
TEST_F(NavigationControllerTest, CloneAndGoBack) {
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);

  scoped_ptr<TabContents> clone(controller().tab_contents()->Clone());

  ASSERT_EQ(2, clone->controller().entry_count());
  EXPECT_TRUE(clone->controller().needs_reload());
  clone->controller().GoBack();
  // Navigating back should have triggered needs_reload_ to go false.
  EXPECT_FALSE(clone->controller().needs_reload());
}

// Make sure that cloning a tabcontents doesn't copy interstitials.
TEST_F(NavigationControllerTest, CloneOmitsInterstitials) {
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);

  // Add an interstitial entry.  Should be deleted with controller.
  NavigationEntry* interstitial_entry = new NavigationEntry();
  interstitial_entry->set_page_type(content::PAGE_TYPE_INTERSTITIAL);
  controller().AddTransientEntry(interstitial_entry);

  scoped_ptr<TabContents> clone(controller().tab_contents()->Clone());

  ASSERT_EQ(2, clone->controller().entry_count());
}

// Tests a subframe navigation while a toplevel navigation is pending.
// http://crbug.com/43967
TEST_F(NavigationControllerTest, SubframeWhilePending) {
  // Load the first page.
  const GURL url1("http://foo/");
  NavigateAndCommit(url1);

  // Now start a pending load to a totally different page, but don't commit it.
  const GURL url2("http://bar/");
  controller().LoadURL(
      url2, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());

  // Send a subframe update from the first page, as if one had just
  // automatically loaded. Auto subframes don't increment the page ID.
  const GURL url1_sub("http://foo/subframe");
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = controller().GetLastCommittedEntry()->page_id();
  params.url = url1_sub;
  params.transition = content::PAGE_TRANSITION_AUTO_SUBFRAME;
  params.should_update_history = false;
  params.gesture = NavigationGestureAuto;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url1_sub));
  content::LoadCommittedDetails details;

  // This should return false meaning that nothing was actually updated.
  EXPECT_FALSE(controller().RendererDidNavigate(params, &details));

  // The notification should have updated the last committed one, and not
  // the pending load.
  EXPECT_EQ(url1, controller().GetLastCommittedEntry()->url());

  // The active entry should be unchanged by the subframe load.
  EXPECT_EQ(url2, controller().GetActiveEntry()->url());
}

// Tests CopyStateFromAndPrune with 2 urls in source, 1 in dest.
TEST_F(NavigationControllerTest, CopyStateFromAndPrune) {
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");
  const GURL url3("http://foo3");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);

  scoped_ptr<TestTabContents> other_contents(CreateTestTabContents());
  NavigationController& other_controller = other_contents->controller();
  other_contents->NavigateAndCommit(url3);
  other_contents->ExpectSetHistoryLengthAndPrune(
      other_controller.GetEntryAtIndex(0)->site_instance(), 2,
      other_controller.GetEntryAtIndex(0)->page_id());
  other_controller.CopyStateFromAndPrune(&controller());

  // other_controller should now contain the 3 urls: url1, url2 and url3.

  ASSERT_EQ(3, other_controller.entry_count());

  ASSERT_EQ(2, other_controller.GetCurrentEntryIndex());

  EXPECT_EQ(url1, other_controller.GetEntryAtIndex(0)->url());
  EXPECT_EQ(url2, other_controller.GetEntryAtIndex(1)->url());
  EXPECT_EQ(url3, other_controller.GetEntryAtIndex(2)->url());
}

// Test CopyStateFromAndPrune with 2 urls, the first selected and nothing in
// the target.
TEST_F(NavigationControllerTest, CopyStateFromAndPrune2) {
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");
  const GURL url3("http://foo3");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);
  controller().GoBack();

  scoped_ptr<TestTabContents> other_contents(CreateTestTabContents());
  NavigationController& other_controller = other_contents->controller();
  other_contents->ExpectSetHistoryLengthAndPrune(NULL, 1, -1);
  other_controller.CopyStateFromAndPrune(&controller());

  // other_controller should now contain the 1 url: url1.

  ASSERT_EQ(1, other_controller.entry_count());

  ASSERT_EQ(0, other_controller.GetCurrentEntryIndex());

  EXPECT_EQ(url1, other_controller.GetEntryAtIndex(0)->url());
}

// Test CopyStateFromAndPrune with 2 urls, the first selected and nothing in
// the target.
TEST_F(NavigationControllerTest, CopyStateFromAndPrune3) {
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");
  const GURL url3("http://foo3");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);
  controller().GoBack();

  scoped_ptr<TestTabContents> other_contents(CreateTestTabContents());
  NavigationController& other_controller = other_contents->controller();
  other_controller.LoadURL(
      url3, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  other_contents->ExpectSetHistoryLengthAndPrune(NULL, 1, -1);
  other_controller.CopyStateFromAndPrune(&controller());

  // other_controller should now contain 1 entry for url1, and a pending entry
  // for url3.

  ASSERT_EQ(1, other_controller.entry_count());

  EXPECT_EQ(0, other_controller.GetCurrentEntryIndex());

  EXPECT_EQ(url1, other_controller.GetEntryAtIndex(0)->url());

  // And there should be a pending entry for url3.
  ASSERT_TRUE(other_controller.pending_entry());

  EXPECT_EQ(url3, other_controller.pending_entry()->url());
}

// Tests that navigations initiated from the page (with the history object)
// work as expected without navigation entries.
TEST_F(NavigationControllerTest, HistoryNavigate) {
  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);
  NavigateAndCommit(url3);
  controller().GoBack();
  contents()->CommitPendingNavigation();

  // Simulate the page calling history.back(), it should not create a pending
  // entry.
  contents()->OnGoToEntryAtOffset(-1);
  EXPECT_EQ(-1, controller().pending_entry_index());
  // The actual cross-navigation is suspended until the current RVH tells us
  // it unloaded, simulate that.
  contents()->ProceedWithCrossSiteNavigation();
  // Also make sure we told the page to navigate.
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(ViewMsg_Navigate::ID);
  ASSERT_TRUE(message != NULL);
  Tuple1<ViewMsg_Navigate_Params> nav_params;
  ViewMsg_Navigate::Read(message, &nav_params);
  EXPECT_EQ(url1, nav_params.a.url);
  process()->sink().ClearMessages();

  // Now test history.forward()
  contents()->OnGoToEntryAtOffset(1);
  EXPECT_EQ(-1, controller().pending_entry_index());
  // The actual cross-navigation is suspended until the current RVH tells us
  // it unloaded, simulate that.
  contents()->ProceedWithCrossSiteNavigation();
  message = process()->sink().GetFirstMessageMatching(ViewMsg_Navigate::ID);
  ASSERT_TRUE(message != NULL);
  ViewMsg_Navigate::Read(message, &nav_params);
  EXPECT_EQ(url3, nav_params.a.url);
  process()->sink().ClearMessages();

  // Make sure an extravagant history.go() doesn't break.
  contents()->OnGoToEntryAtOffset(120);  // Out of bounds.
  EXPECT_EQ(-1, controller().pending_entry_index());
  message = process()->sink().GetFirstMessageMatching(ViewMsg_Navigate::ID);
  EXPECT_TRUE(message == NULL);
}

// Test call to PruneAllButActive for the only entry.
TEST_F(NavigationControllerTest, PruneAllButActiveForSingle) {
  const GURL url1("http://foo1");
  NavigateAndCommit(url1);
  controller().PruneAllButActive();

  EXPECT_EQ(-1, controller().pending_entry_index());
  EXPECT_EQ(controller().GetEntryAtIndex(0)->url(), url1);
}

// Test call to PruneAllButActive for last entry.
TEST_F(NavigationControllerTest, PruneAllButActiveForLast) {
  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);
  NavigateAndCommit(url3);
  controller().GoBack();
  controller().GoBack();
  contents()->CommitPendingNavigation();

  controller().PruneAllButActive();

  EXPECT_EQ(-1, controller().pending_entry_index());
  EXPECT_EQ(controller().GetEntryAtIndex(0)->url(), url1);
}

// Test call to PruneAllButActive for intermediate entry.
TEST_F(NavigationControllerTest, PruneAllButActiveForIntermediate) {
  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);
  NavigateAndCommit(url3);
  controller().GoBack();
  contents()->CommitPendingNavigation();

  controller().PruneAllButActive();

  EXPECT_EQ(-1, controller().pending_entry_index());
  EXPECT_EQ(controller().GetEntryAtIndex(0)->url(), url2);
}

// Test call to PruneAllButActive for intermediate entry.
TEST_F(NavigationControllerTest, PruneAllButActiveForPending) {
  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);
  NavigateAndCommit(url3);
  controller().GoBack();

  controller().PruneAllButActive();

  EXPECT_EQ(0, controller().pending_entry_index());
}

// Test call to PruneAllButActive for transient entry.
TEST_F(NavigationControllerTest, PruneAllButActiveForTransient) {
  const GURL url0("http://foo/0");
  const GURL url1("http://foo/1");
  const GURL transient_url("http://foo/transient");

  controller().LoadURL(
      url0, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  rvh()->SendNavigate(0, url0);
  controller().LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  rvh()->SendNavigate(1, url1);

  // Adding a transient with no pending entry.
  NavigationEntry* transient_entry = new NavigationEntry;
  transient_entry->set_url(transient_url);
  controller().AddTransientEntry(transient_entry);

  controller().PruneAllButActive();

  EXPECT_EQ(-1, controller().pending_entry_index());
  EXPECT_EQ(-1, controller().pending_entry_index());
  EXPECT_EQ(controller().GetTransientEntry()->url(), transient_url);
}

// Test to ensure that when we do a history navigation back to the current
// committed page (e.g., going forward to a slow-loading page, then pressing
// the back button), we just stop the navigation to prevent the throbber from
// running continuously. Otherwise, the RenderViewHost forces the throbber to
// start, but WebKit essentially ignores the navigation and never sends a
// message to stop the throbber.
TEST_F(NavigationControllerTest, StopOnHistoryNavigationToCurrentPage) {
  const GURL url0("http://foo/0");
  const GURL url1("http://foo/1");

  NavigateAndCommit(url0);
  NavigateAndCommit(url1);

  // Go back to the original page, then forward to the slow page, then back
  controller().GoBack();
  contents()->CommitPendingNavigation();

  controller().GoForward();
  EXPECT_EQ(1, controller().pending_entry_index());

  controller().GoBack();
  EXPECT_EQ(-1, controller().pending_entry_index());
}

/* TODO(brettw) These test pass on my local machine but fail on the XP buildbot
   (but not Vista) cleaning up the directory after they run.
   This should be fixed.

// NavigationControllerHistoryTest ---------------------------------------------

class NavigationControllerHistoryTest : public NavigationControllerTest {
 public:
  NavigationControllerHistoryTest()
      : url0("http://foo1"),
        url1("http://foo1"),
        url2("http://foo1"),
        profile_manager_(NULL) {
  }

  virtual ~NavigationControllerHistoryTest() {
    // Prevent our base class from deleting the profile since profile's
    // lifetime is managed by profile_manager_.
    STLDeleteElements(&windows_);
  }

  // testing::Test overrides.
  virtual void SetUp() {
    NavigationControllerTest::SetUp();

    // Force the session service to be created.
    SessionService* service = new SessionService(profile());
    SessionServiceFactory::SetForTestProfile(profile(), service);
    service->SetWindowType(window_id, Browser::TYPE_TABBED);
    service->SetWindowBounds(window_id, gfx::Rect(0, 1, 2, 3), false);
    service->SetTabIndexInWindow(window_id,
                                 controller().session_id(), 0);
    controller().SetWindowID(window_id);

    session_helper_.set_service(service);
  }

  virtual void TearDown() {
    // Release profile's reference to the session service. Otherwise the file
    // will still be open and we won't be able to delete the directory below.
    session_helper_.ReleaseService(); // profile owns this
    SessionServiceFactory::SetForTestProfile(profile(), NULL);

    // Make sure we wait for history to shut down before continuing. The task
    // we add will cause our message loop to quit once it is destroyed.
    HistoryService* history =
        profile()->GetHistoryService(Profile::IMPLICIT_ACCESS);
    if (history) {
      history->SetOnBackendDestroyTask(new MessageLoop::QuitTask);
      MessageLoop::current()->Run();
    }

    // Do normal cleanup before deleting the profile directory below.
    NavigationControllerTest::TearDown();

    ASSERT_TRUE(file_util::Delete(test_dir_, true));
    ASSERT_FALSE(file_util::PathExists(test_dir_));
  }

  // Deletes the current profile manager and creates a new one. Indirectly this
  // shuts down the history database and reopens it.
  void ReopenDatabase() {
    session_helper_.set_service(NULL);
    SessionServiceFactory::SetForTestProfile(profile(), NULL);

    SessionService* service = new SessionService(profile());
    SessionServiceFactory::SetForTestProfile(profile(), service);
    session_helper_.set_service(service);
  }

  void GetLastSession() {
    SessionServiceFactory::GetForProfile(profile())->TabClosed(
        controller().window_id(), controller().session_id(), false);

    ReopenDatabase();
    Time close_time;

    session_helper_.ReadWindows(&windows_);
  }

  CancelableRequestConsumer consumer;

  // URLs for testing.
  const GURL url0;
  const GURL url1;
  const GURL url2;

  std::vector<SessionWindow*> windows_;

  SessionID window_id;

  SessionServiceTestHelper session_helper_;

 private:
  ProfileManager* profile_manager_;
  FilePath test_dir_;
};

// A basic test case. Navigates to a single url, and make sure the history
// db matches.
TEST_F(NavigationControllerHistoryTest, Basic) {
  controller().LoadURL(url0, GURL(), content::PAGE_TRANSITION_LINK);
  rvh()->SendNavigate(0, url0);

  GetLastSession();

  session_helper_.AssertSingleWindowWithSingleTab(windows_, 1);
  session_helper_.AssertTabEquals(0, 0, 1, *(windows_[0]->tabs[0]));
  TabNavigation nav1(0, url0, GURL(), string16(),
                     webkit_glue::CreateHistoryStateForURL(url0),
                     content::PAGE_TRANSITION_LINK);
  session_helper_.AssertNavigationEquals(nav1,
                                         windows_[0]->tabs[0]->navigations[0]);
}

// Navigates to three urls, then goes back and make sure the history database
// is in sync.
TEST_F(NavigationControllerHistoryTest, NavigationThenBack) {
  rvh()->SendNavigate(0, url0);
  rvh()->SendNavigate(1, url1);
  rvh()->SendNavigate(2, url2);

  controller().GoBack();
  rvh()->SendNavigate(1, url1);

  GetLastSession();

  session_helper_.AssertSingleWindowWithSingleTab(windows_, 3);
  session_helper_.AssertTabEquals(0, 1, 3, *(windows_[0]->tabs[0]));

  TabNavigation nav(0, url0, GURL(), string16(),
                    webkit_glue::CreateHistoryStateForURL(url0),
                    content::PAGE_TRANSITION_LINK);
  session_helper_.AssertNavigationEquals(nav,
                                         windows_[0]->tabs[0]->navigations[0]);
  nav.set_url(url1);
  session_helper_.AssertNavigationEquals(nav,
                                         windows_[0]->tabs[0]->navigations[1]);
  nav.set_url(url2);
  session_helper_.AssertNavigationEquals(nav,
                                         windows_[0]->tabs[0]->navigations[2]);
}

// Navigates to three urls, then goes back twice, then loads a new url.
TEST_F(NavigationControllerHistoryTest, NavigationPruning) {
  rvh()->SendNavigate(0, url0);
  rvh()->SendNavigate(1, url1);
  rvh()->SendNavigate(2, url2);

  controller().GoBack();
  rvh()->SendNavigate(1, url1);

  controller().GoBack();
  rvh()->SendNavigate(0, url0);

  rvh()->SendNavigate(3, url2);

  // Now have url0, and url2.

  GetLastSession();

  session_helper_.AssertSingleWindowWithSingleTab(windows_, 2);
  session_helper_.AssertTabEquals(0, 1, 2, *(windows_[0]->tabs[0]));

  TabNavigation nav(0, url0, GURL(), string16(),
                    webkit_glue::CreateHistoryStateForURL(url0),
                    content::PAGE_TRANSITION_LINK);
  session_helper_.AssertNavigationEquals(nav,
                                         windows_[0]->tabs[0]->navigations[0]);
  nav.set_url(url2);
  session_helper_.AssertNavigationEquals(nav,
                                         windows_[0]->tabs[0]->navigations[1]);
}
*/
