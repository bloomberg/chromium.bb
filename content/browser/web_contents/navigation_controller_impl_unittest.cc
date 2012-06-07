// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/navigation_controller_impl.h"
#include "content/browser/web_contents/navigation_entry_impl.h"
#include "content/browser/web_contents/test_web_contents.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_notification_tracker.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/webkit_glue.h"

using base::Time;
using content::NavigationController;
using content::NavigationEntry;
using content::NavigationEntryImpl;
using content::RenderViewHostImplTestHarness;
using content::SiteInstance;
using content::TestNotificationTracker;
using content::TestRenderViewHost;
using content::TestWebContents;
using content::WebContents;

// NavigationControllerTest ----------------------------------------------------

class NavigationControllerTest : public RenderViewHostImplTestHarness {
 public:
  NavigationControllerTest() {}

  NavigationControllerImpl& controller_impl() {
    return static_cast<NavigationControllerImpl&>(controller());
  }
};

void RegisterForAllNavNotifications(TestNotificationTracker* tracker,
                                    NavigationController* controller) {
  tracker->ListenFor(content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                     content::Source<NavigationController>(
                         controller));
  tracker->ListenFor(content::NOTIFICATION_NAV_LIST_PRUNED,
                     content::Source<NavigationController>(
                         controller));
  tracker->ListenFor(content::NOTIFICATION_NAV_ENTRY_CHANGED,
                     content::Source<NavigationController>(
                         controller));
}

SiteInstance* GetSiteInstanceFromEntry(NavigationEntry* entry) {
  return NavigationEntryImpl::FromNavigationEntry(entry)->site_instance();
}

class TestWebContentsDelegate : public content::WebContentsDelegate {
 public:
  explicit TestWebContentsDelegate() :
      navigation_state_change_count_(0) {}

  int navigation_state_change_count() {
    return navigation_state_change_count_;
  }

  // Keep track of whether the tab has notified us of a navigation state change.
  virtual void NavigationStateChanged(const WebContents* source,
                                      unsigned changed_flags) {
    navigation_state_change_count_++;
  }

 private:
  // The number of times NavigationStateChanged has been called.
  int navigation_state_change_count_;
};

// -----------------------------------------------------------------------------

TEST_F(NavigationControllerTest, Defaults) {
  NavigationControllerImpl& controller = controller_impl();

  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_FALSE(controller.GetLastCommittedEntry());
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), -1);
  EXPECT_EQ(controller.GetEntryCount(), 0);
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

TEST_F(NavigationControllerTest, LoadURL) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  controller.LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  // Creating a pending notification should not have issued any of the
  // notifications we're listening for.
  EXPECT_EQ(0U, notifications.size());

  // The load should now be pending.
  EXPECT_EQ(controller.GetEntryCount(), 0);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), -1);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_FALSE(controller.GetLastCommittedEntry());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
  EXPECT_EQ(contents()->GetMaxPageID(), -1);

  // We should have gotten no notifications from the preceeding checks.
  EXPECT_EQ(0U, notifications.size());

  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // The load should now be committed.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
  EXPECT_EQ(contents()->GetMaxPageID(), 0);

  // Load another...
  controller.LoadURL(
      url2, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());

  // The load should now be pending.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_TRUE(controller.GetPendingEntry());
  // TODO(darin): maybe this should really be true?
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
  EXPECT_EQ(contents()->GetMaxPageID(), 0);

  // Simulate the beforeunload ack for the cross-site transition, and then the
  // commit.
  test_rvh()->SendShouldCloseACK(true);
  static_cast<TestRenderViewHost*>(
      contents()->GetPendingRenderViewHost())->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // The load should now be committed.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
  EXPECT_EQ(contents()->GetMaxPageID(), 1);
}

// Tests what happens when the same page is loaded again.  Should not create a
// new session history entry. This is what happens when you press enter in the
// URL bar to reload: a pending entry is created and then it is discarded when
// the load commits (because WebCore didn't actually make a new entry).
TEST_F(NavigationControllerTest, LoadURL_SamePage) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  const GURL url1("http://foo1");

  controller.LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(0U, notifications.size());
  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller.LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(0U, notifications.size());
  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // We should not have produced a new session history entry.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

// Tests loading a URL but discarding it before the load commits.
TEST_F(NavigationControllerTest, LoadURL_Discarded) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  controller.LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(0U, notifications.size());
  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller.LoadURL(
      url2, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  controller.DiscardNonCommittedEntries();
  EXPECT_EQ(0U, notifications.size());

  // Should not have produced a new session history entry.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

// Tests navigations that come in unrequested. This happens when the user
// navigates from the web page, and here we test that there is no pending entry.
TEST_F(NavigationControllerTest, LoadURL_NoPending) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  // First make an existing committed entry.
  const GURL kExistingURL1("http://eh");
  controller.LoadURL(kExistingURL1, content::Referrer(),
                     content::PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(0, kExistingURL1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Do a new navigation without making a pending one.
  const GURL kNewURL("http://see");
  test_rvh()->SendNavigate(99, kNewURL);

  // There should no longer be any pending entry, and the third navigation we
  // just made should be committed.
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(kNewURL, controller.GetActiveEntry()->GetURL());
}

// Tests navigating to a new URL when there is a new pending navigation that is
// not the one that just loaded. This will happen if the user types in a URL to
// somewhere slow, and then navigates the current page before the typed URL
// commits.
TEST_F(NavigationControllerTest, LoadURL_NewPending) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  // First make an existing committed entry.
  const GURL kExistingURL1("http://eh");
  controller.LoadURL(kExistingURL1, content::Referrer(),
                     content::PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(0, kExistingURL1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Make a pending entry to somewhere new.
  const GURL kExistingURL2("http://bee");
  controller.LoadURL(kExistingURL2, content::Referrer(),
                     content::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(0U, notifications.size());

  // After the beforeunload but before it commits, do a new navigation.
  test_rvh()->SendShouldCloseACK(true);
  const GURL kNewURL("http://see");
  static_cast<TestRenderViewHost*>(
      contents()->GetPendingRenderViewHost())->SendNavigate(3, kNewURL);

  // There should no longer be any pending entry, and the third navigation we
  // just made should be committed.
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(kNewURL, controller.GetActiveEntry()->GetURL());
}

// Tests navigating to a new URL when there is a pending back/forward
// navigation. This will happen if the user hits back, but before that commits,
// they navigate somewhere new.
TEST_F(NavigationControllerTest, LoadURL_ExistingPending) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  // First make some history.
  const GURL kExistingURL1("http://foo/eh");
  controller.LoadURL(kExistingURL1, content::Referrer(),
                     content::PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(0, kExistingURL1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  const GURL kExistingURL2("http://foo/bee");
  controller.LoadURL(kExistingURL2, content::Referrer(),
                     content::PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(1, kExistingURL2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Now make a pending back/forward navigation. The zeroth entry should be
  // pending.
  controller.GoBack();
  EXPECT_EQ(0U, notifications.size());
  EXPECT_EQ(0, controller.GetPendingEntryIndex());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // Before that commits, do a new navigation.
  const GURL kNewURL("http://foo/see");
  content::LoadCommittedDetails details;
  test_rvh()->SendNavigate(3, kNewURL);

  // There should no longer be any pending entry, and the third navigation we
  // just made should be committed.
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(kNewURL, controller.GetActiveEntry()->GetURL());
}

// Tests navigating to an existing URL when there is a pending new navigation.
// This will happen if the user enters a URL, but before that commits, the
// current page fires history.back().
TEST_F(NavigationControllerTest, LoadURL_BackPreemptsPending) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  // First make some history.
  const GURL kExistingURL1("http://foo/eh");
  controller.LoadURL(kExistingURL1, content::Referrer(),
                     content::PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(0, kExistingURL1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  const GURL kExistingURL2("http://foo/bee");
  controller.LoadURL(kExistingURL2, content::Referrer(),
                     content::PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(1, kExistingURL2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Now make a pending new navigation.
  const GURL kNewURL("http://foo/see");
  controller.LoadURL(
      kNewURL, content::Referrer(), content::PAGE_TRANSITION_TYPED,
      std::string());
  EXPECT_EQ(0U, notifications.size());
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // Before that commits, a back navigation from the renderer commits.
  test_rvh()->SendNavigate(0, kExistingURL1);

  // There should no longer be any pending entry, and the back navigation we
  // just made should be committed.
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(kExistingURL1, controller.GetActiveEntry()->GetURL());
}

// Tests an ignored navigation when there is a pending new navigation.
// This will happen if the user enters a URL, but before that commits, the
// current blank page reloads.  See http://crbug.com/77507.
TEST_F(NavigationControllerTest, LoadURL_IgnorePreemptsPending) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  // Set a WebContentsDelegate to listen for state changes.
  scoped_ptr<TestWebContentsDelegate> delegate(new TestWebContentsDelegate());
  EXPECT_FALSE(contents()->GetDelegate());
  contents()->SetDelegate(delegate.get());

  // Without any navigations, the renderer starts at about:blank.
  const GURL kExistingURL("about:blank");

  // Now make a pending new navigation.
  const GURL kNewURL("http://eh");
  controller.LoadURL(
      kNewURL, content::Referrer(), content::PAGE_TRANSITION_TYPED,
      std::string());
  EXPECT_EQ(0U, notifications.size());
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(-1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(1, delegate->navigation_state_change_count());

  // Before that commits, a document.write and location.reload can cause the
  // renderer to send a FrameNavigate with page_id -1.
  test_rvh()->SendNavigate(-1, kExistingURL);

  // This should clear the pending entry and notify of a navigation state
  // change, so that we do not keep displaying kNewURL.
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_EQ(-1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(2, delegate->navigation_state_change_count());

  contents()->SetDelegate(NULL);
}

// Tests that the pending entry state is correct after an abort.
TEST_F(NavigationControllerTest, LoadURL_AbortCancelsPending) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  // Set a WebContentsDelegate to listen for state changes.
  scoped_ptr<TestWebContentsDelegate> delegate(new TestWebContentsDelegate());
  EXPECT_FALSE(contents()->GetDelegate());
  contents()->SetDelegate(delegate.get());

  // Without any navigations, the renderer starts at about:blank.
  const GURL kExistingURL("about:blank");

  // Now make a pending new navigation.
  const GURL kNewURL("http://eh");
  controller.LoadURL(
      kNewURL, content::Referrer(), content::PAGE_TRANSITION_TYPED,
      std::string());
  EXPECT_EQ(0U, notifications.size());
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(-1, controller.GetLastCommittedEntryIndex());
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
  test_rvh()->OnMessageReceived(
          ViewHostMsg_DidFailProvisionalLoadWithError(0,  // routing_id
                                                      params));

  // This should clear the pending entry and notify of a navigation state
  // change, so that we do not keep displaying kNewURL.
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_EQ(-1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(2, delegate->navigation_state_change_count());

  contents()->SetDelegate(NULL);
}

// Tests that the pending entry state is correct after a redirect and abort.
// http://crbug.com/83031.
TEST_F(NavigationControllerTest, LoadURL_RedirectAbortCancelsPending) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  // Set a WebContentsDelegate to listen for state changes.
  scoped_ptr<TestWebContentsDelegate> delegate(new TestWebContentsDelegate());
  EXPECT_FALSE(contents()->GetDelegate());
  contents()->SetDelegate(delegate.get());

  // Without any navigations, the renderer starts at about:blank.
  const GURL kExistingURL("about:blank");

  // Now make a pending new navigation.
  const GURL kNewURL("http://eh");
  controller.LoadURL(
      kNewURL, content::Referrer(), content::PAGE_TRANSITION_TYPED,
      std::string());
  EXPECT_EQ(0U, notifications.size());
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(-1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(1, delegate->navigation_state_change_count());

  // Now the navigation redirects.  The NavigationController no longer
  // receives a notification of the redirect, so nothing happens here.
  // See https://chromiumcodereview.appspot.com/10316020
  const GURL kRedirectURL("http://bee");

  // It may abort before committing, if it's a download or due to a stop or
  // a new navigation from the user.
  ViewHostMsg_DidFailProvisionalLoadWithError_Params params;
  params.frame_id = 1;
  params.is_main_frame = true;
  params.error_code = net::ERR_ABORTED;
  params.error_description = string16();
  params.url = kRedirectURL;
  params.showing_repost_interstitial = false;
  test_rvh()->OnMessageReceived(
          ViewHostMsg_DidFailProvisionalLoadWithError(0,  // routing_id
                                                      params));

  // This should clear the pending entry and notify of a navigation state
  // change, so that we do not keep displaying kNewURL.
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_EQ(-1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(2, delegate->navigation_state_change_count());

  contents()->SetDelegate(NULL);
}

TEST_F(NavigationControllerTest, Reload) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  const GURL url1("http://foo1");

  controller.LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(0U, notifications.size());
  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  controller.GetActiveEntry()->SetTitle(ASCIIToUTF16("Title"));
  controller.Reload(true);
  EXPECT_EQ(0U, notifications.size());

  // The reload is pending.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), 0);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
  // Make sure the title has been cleared (will be redrawn just after reload).
  // Avoids a stale cached title when the new page being reloaded has no title.
  // See http://crbug.com/96041.
  EXPECT_TRUE(controller.GetActiveEntry()->GetTitle().empty());

  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Now the reload is committed.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

// Tests what happens when a reload navigation produces a new page.
TEST_F(NavigationControllerTest, Reload_GeneratesNewPage) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  controller.LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller.Reload(true);
  EXPECT_EQ(0U, notifications.size());

  test_rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Now the reload is committed.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

// Tests what happens when we navigate back successfully
TEST_F(NavigationControllerTest, Back) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  const GURL url1("http://foo1");
  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  const GURL url2("http://foo2");
  test_rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller.GoBack();
  EXPECT_EQ(0U, notifications.size());

  // We should now have a pending navigation to go back.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller.GetPendingEntryIndex(), 0);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_TRUE(controller.CanGoForward());

  test_rvh()->SendNavigate(0, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // The back navigation completed successfully.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_TRUE(controller.CanGoForward());
}

// Tests what happens when a back navigation produces a new page.
TEST_F(NavigationControllerTest, Back_GeneratesNewPage) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");

  controller.LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller.LoadURL(
      url2, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller.GoBack();
  EXPECT_EQ(0U, notifications.size());

  // We should now have a pending navigation to go back.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller.GetPendingEntryIndex(), 0);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_TRUE(controller.CanGoForward());

  test_rvh()->SendNavigate(2, url3);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // The back navigation resulted in a completely new navigation.
  // TODO(darin): perhaps this behavior will be confusing to users?
  EXPECT_EQ(controller.GetEntryCount(), 3);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 2);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

// Receives a back message when there is a new pending navigation entry.
TEST_F(NavigationControllerTest, Back_NewPending) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  const GURL kUrl1("http://foo1");
  const GURL kUrl2("http://foo2");
  const GURL kUrl3("http://foo3");

  // First navigate two places so we have some back history.
  test_rvh()->SendNavigate(0, kUrl1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // controller.LoadURL(kUrl2, content::PAGE_TRANSITION_TYPED);
  test_rvh()->SendNavigate(1, kUrl2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Now start a new pending navigation and go back before it commits.
  controller.LoadURL(
      kUrl3, content::Referrer(), content::PAGE_TRANSITION_TYPED,
      std::string());
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(kUrl3, controller.GetPendingEntry()->GetURL());
  controller.GoBack();

  // The pending navigation should now be the "back" item and the new one
  // should be gone.
  EXPECT_EQ(0, controller.GetPendingEntryIndex());
  EXPECT_EQ(kUrl1, controller.GetPendingEntry()->GetURL());
}

// Receives a back message when there is a different renavigation already
// pending.
TEST_F(NavigationControllerTest, Back_OtherBackPending) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL kUrl1("http://foo/1");
  const GURL kUrl2("http://foo/2");
  const GURL kUrl3("http://foo/3");

  // First navigate three places so we have some back history.
  test_rvh()->SendNavigate(0, kUrl1);
  test_rvh()->SendNavigate(1, kUrl2);
  test_rvh()->SendNavigate(2, kUrl3);

  // With nothing pending, say we get a navigation to the second entry.
  test_rvh()->SendNavigate(1, kUrl2);

  // We know all the entries have the same site instance, so we can just grab
  // a random one for looking up other entries.
  SiteInstance* site_instance =
      NavigationEntryImpl::FromNavigationEntry(
          controller.GetLastCommittedEntry())->site_instance();

  // That second URL should be the last committed and it should have gotten the
  // new title.
  EXPECT_EQ(kUrl2, controller.GetEntryWithPageID(site_instance, 1)->GetURL());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());

  // Now go forward to the last item again and say it was committed.
  controller.GoForward();
  test_rvh()->SendNavigate(2, kUrl3);

  // Now start going back one to the second page. It will be pending.
  controller.GoBack();
  EXPECT_EQ(1, controller.GetPendingEntryIndex());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());

  // Not synthesize a totally new back event to the first page. This will not
  // match the pending one.
  test_rvh()->SendNavigate(0, kUrl1);

  // The committed navigation should clear the pending entry.
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());

  // But the navigated entry should be the last committed.
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(kUrl1, controller.GetLastCommittedEntry()->GetURL());
}

// Tests what happens when we navigate forward successfully.
TEST_F(NavigationControllerTest, Forward) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  test_rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller.GoBack();
  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller.GoForward();

  // We should now have a pending navigation to go forward.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), 1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());

  test_rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // The forward navigation completed successfully.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

// Tests what happens when a forward navigation produces a new page.
TEST_F(NavigationControllerTest, Forward_GeneratesNewPage) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");
  const GURL url3("http://foo3");

  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  test_rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller.GoBack();
  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller.GoForward();
  EXPECT_EQ(0U, notifications.size());

  // Should now have a pending navigation to go forward.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), 1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());

  test_rvh()->SendNavigate(2, url3);
  EXPECT_TRUE(notifications.Check2AndReset(
      content::NOTIFICATION_NAV_LIST_PRUNED,
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

// Two consequent navigation for the same URL entered in should be considered
// as SAME_PAGE navigation even when we are redirected to some other page.
TEST_F(NavigationControllerTest, Redirect) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");  // Redirection target

  // First request
  controller.LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());

  EXPECT_EQ(0U, notifications.size());
  test_rvh()->SendNavigate(0, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Second request
  controller.LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());

  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_EQ(url1, controller.GetActiveEntry()->GetURL());

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
  EXPECT_TRUE(controller.RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  EXPECT_TRUE(details.type == content::NAVIGATION_TYPE_SAME_PAGE);
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_EQ(url2, controller.GetActiveEntry()->GetURL());

  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

// Similar to Redirect above, but the first URL is requested by POST,
// the second URL is requested by GET. NavigationEntry::has_post_data_
// must be cleared. http://crbug.com/21245
TEST_F(NavigationControllerTest, PostThenRedirect) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");  // Redirection target

  // First request as POST
  controller.LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  controller.GetActiveEntry()->SetHasPostData(true);

  EXPECT_EQ(0U, notifications.size());
  test_rvh()->SendNavigate(0, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Second request
  controller.LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());

  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_EQ(url1, controller.GetActiveEntry()->GetURL());

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
  EXPECT_TRUE(controller.RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  EXPECT_TRUE(details.type == content::NAVIGATION_TYPE_SAME_PAGE);
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_EQ(url2, controller.GetActiveEntry()->GetURL());
  EXPECT_FALSE(controller.GetActiveEntry()->GetHasPostData());

  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

// A redirect right off the bat should be a NEW_PAGE.
TEST_F(NavigationControllerTest, ImmediateRedirect) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");  // Redirection target

  // First request
  controller.LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());

  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_EQ(url1, controller.GetActiveEntry()->GetURL());

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
  EXPECT_TRUE(controller.RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  EXPECT_TRUE(details.type == content::NAVIGATION_TYPE_NEW_PAGE);
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_EQ(url2, controller.GetActiveEntry()->GetURL());

  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

// Tests navigation via link click within a subframe. A new navigation entry
// should be created.
TEST_F(NavigationControllerTest, NewSubframe) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  const GURL url1("http://foo1");
  test_rvh()->SendNavigate(0, url1);
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
  EXPECT_TRUE(controller.RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(url1, details.previous_url);
  EXPECT_FALSE(details.is_in_page);
  EXPECT_FALSE(details.is_main_frame);

  // The new entry should be appended.
  EXPECT_EQ(2, controller.GetEntryCount());

  // New entry should refer to the new page, but the old URL (entries only
  // reflect the toplevel URL).
  EXPECT_EQ(url1, details.entry->GetURL());
  EXPECT_EQ(params.page_id, details.entry->GetPageID());
}

// Some pages create a popup, then write an iframe into it. This causes a
// subframe navigation without having any committed entry. Such navigations
// just get thrown on the ground, but we shouldn't crash.
TEST_F(NavigationControllerTest, SubframeOnEmptyPage) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

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
  EXPECT_FALSE(controller.RendererDidNavigate(params, &details));
  EXPECT_EQ(0U, notifications.size());
}

// Auto subframes are ones the page loads automatically like ads. They should
// not create new navigation entries.
TEST_F(NavigationControllerTest, AutoSubframe) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  const GURL url1("http://foo1");
  test_rvh()->SendNavigate(0, url1);
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
  EXPECT_FALSE(controller.RendererDidNavigate(params, &details));
  EXPECT_EQ(0U, notifications.size());

  // There should still be only one entry.
  EXPECT_EQ(1, controller.GetEntryCount());
}

// Tests navigation and then going back to a subframe navigation.
TEST_F(NavigationControllerTest, BackSubframe) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  // Main page.
  const GURL url1("http://foo1");
  test_rvh()->SendNavigate(0, url1);
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
  EXPECT_TRUE(controller.RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(2, controller.GetEntryCount());

  // Second manual subframe navigation should also make a new entry.
  const GURL url3("http://foo3");
  params.page_id = 2;
  params.url = url3;
  EXPECT_TRUE(controller.RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());

  // Go back one.
  controller.GoBack();
  params.url = url2;
  params.page_id = 1;
  EXPECT_TRUE(controller.RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());

  // Go back one more.
  controller.GoBack();
  params.url = url1;
  params.page_id = 0;
  EXPECT_TRUE(controller.RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
}

TEST_F(NavigationControllerTest, LinkClick) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  test_rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Should not have produced a new session history entry.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

TEST_F(NavigationControllerTest, InPage) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  // Main page.
  const GURL url1("http://foo");
  test_rvh()->SendNavigate(0, url1);
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
  EXPECT_TRUE(controller.RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_TRUE(details.is_in_page);
  EXPECT_FALSE(details.did_replace_entry);
  EXPECT_EQ(2, controller.GetEntryCount());

  // Go back one.
  ViewHostMsg_FrameNavigate_Params back_params(params);
  controller.GoBack();
  back_params.url = url1;
  back_params.page_id = 0;
  EXPECT_TRUE(controller.RendererDidNavigate(back_params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  // is_in_page is false in that case but should be true.
  // See comment in AreURLsInPageNavigation() in navigation_controller.cc
  // EXPECT_TRUE(details.is_in_page);
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(back_params.url, controller.GetActiveEntry()->GetURL());

  // Go forward
  ViewHostMsg_FrameNavigate_Params forward_params(params);
  controller.GoForward();
  forward_params.url = url2;
  forward_params.page_id = 1;
  EXPECT_TRUE(controller.RendererDidNavigate(forward_params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_TRUE(details.is_in_page);
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(forward_params.url,
            controller.GetActiveEntry()->GetURL());

  // Now go back and forward again. This is to work around a bug where we would
  // compare the incoming URL with the last committed entry rather than the
  // one identified by an existing page ID. This would result in the second URL
  // losing the reference fragment when you navigate away from it and then back.
  controller.GoBack();
  EXPECT_TRUE(controller.RendererDidNavigate(back_params, &details));
  controller.GoForward();
  EXPECT_TRUE(controller.RendererDidNavigate(forward_params, &details));
  EXPECT_EQ(forward_params.url,
            controller.GetActiveEntry()->GetURL());

  // Finally, navigate to an unrelated URL to make sure in_page is not sticky.
  const GURL url3("http://bar");
  params.page_id = 2;
  params.url = url3;
  notifications.Reset();
  EXPECT_TRUE(controller.RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_FALSE(details.is_in_page);
}

TEST_F(NavigationControllerTest, InPage_Replace) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  // Main page.
  const GURL url1("http://foo");
  test_rvh()->SendNavigate(0, url1);
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
  EXPECT_TRUE(controller.RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_TRUE(details.is_in_page);
  EXPECT_TRUE(details.did_replace_entry);
  EXPECT_EQ(1, controller.GetEntryCount());
}

// Tests for http://crbug.com/40395
// Simulates this:
//   <script>
//     window.location.replace("#a");
//     window.location='http://foo3/';
//   </script>
TEST_F(NavigationControllerTest, ClientRedirectAfterInPageNavigation) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  // Load an initial page.
  {
    const GURL url("http://foo/");
    test_rvh()->SendNavigate(0, url);
    EXPECT_TRUE(notifications.Check1AndReset(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED));
  }

  // Navigate to a new page.
  {
    const GURL url("http://foo2/");
    test_rvh()->SendNavigate(1, url);
    controller.DocumentLoadedInFrame();
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
    EXPECT_TRUE(controller.RendererDidNavigate(params, &details));
    EXPECT_TRUE(notifications.Check1AndReset(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED));
    EXPECT_TRUE(details.is_in_page);
    EXPECT_TRUE(details.did_replace_entry);
    EXPECT_EQ(2, controller.GetEntryCount());
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
    EXPECT_TRUE(controller.RendererDidNavigate(params, &details));
    EXPECT_TRUE(notifications.Check1AndReset(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED));
    EXPECT_FALSE(details.is_in_page);
    EXPECT_EQ(3, controller.GetEntryCount());
  }

  // Verify that BACK brings us back to http://foo2/.
  {
    const GURL url("http://foo2/");
    controller.GoBack();
    test_rvh()->SendNavigate(1, url);
    EXPECT_TRUE(notifications.Check1AndReset(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED));
    EXPECT_EQ(url, controller.GetActiveEntry()->GetURL());
  }
}

// NotificationObserver implementation used in verifying we've received the
// content::NOTIFICATION_NAV_LIST_PRUNED method.
class PrunedListener : public content::NotificationObserver {
 public:
  explicit PrunedListener(NavigationControllerImpl* controller)
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
  NavigationControllerImpl& controller = controller_impl();
  size_t original_count = NavigationControllerImpl::max_entry_count();
  const int kMaxEntryCount = 5;

  NavigationControllerImpl::set_max_entry_count_for_testing(kMaxEntryCount);

  int url_index;
  // Load up to the max count, all entries should be there.
  for (url_index = 0; url_index < kMaxEntryCount; url_index++) {
    GURL url(StringPrintf("http://www.a.com/%d", url_index));
    controller.LoadURL(
        url, content::Referrer(), content::PAGE_TRANSITION_TYPED,
        std::string());
    test_rvh()->SendNavigate(url_index, url);
  }

  EXPECT_EQ(controller.GetEntryCount(), kMaxEntryCount);

  // Created a PrunedListener to observe prune notifications.
  PrunedListener listener(&controller);

  // Navigate some more.
  GURL url(StringPrintf("http://www.a.com/%d", url_index));
  controller.LoadURL(
      url, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(url_index, url);
  url_index++;

  // We should have got a pruned navigation.
  EXPECT_EQ(1, listener.notification_count_);
  EXPECT_TRUE(listener.details_.from_front);
  EXPECT_EQ(1, listener.details_.count);

  // We expect http://www.a.com/0 to be gone.
  EXPECT_EQ(controller.GetEntryCount(), kMaxEntryCount);
  EXPECT_EQ(controller.GetEntryAtIndex(0)->GetURL(),
            GURL("http:////www.a.com/1"));

  // More navigations.
  for (int i = 0; i < 3; i++) {
    url = GURL(StringPrintf("http:////www.a.com/%d", url_index));
    controller.LoadURL(
        url, content::Referrer(), content::PAGE_TRANSITION_TYPED,
        std::string());
    test_rvh()->SendNavigate(url_index, url);
    url_index++;
  }
  EXPECT_EQ(controller.GetEntryCount(), kMaxEntryCount);
  EXPECT_EQ(controller.GetEntryAtIndex(0)->GetURL(),
            GURL("http:////www.a.com/4"));

  NavigationControllerImpl::set_max_entry_count_for_testing(original_count);
}

// Tests that we can do a restore and navigate to the restored entries and
// everything is updated properly. This can be tricky since there is no
// SiteInstance for the entries created initially.
TEST_F(NavigationControllerTest, RestoreNavigate) {
  // Create a NavigationController with a restored set of tabs.
  GURL url("http://foo");
  std::vector<NavigationEntry*> entries;
  NavigationEntry* entry = NavigationControllerImpl::CreateNavigationEntry(
      url, content::Referrer(), content::PAGE_TRANSITION_RELOAD, false,
      std::string(), browser_context());
  entry->SetPageID(0);
  entry->SetTitle(ASCIIToUTF16("Title"));
  entry->SetContentState("state");
  entries.push_back(entry);
  WebContentsImpl our_contents(
      browser_context(), NULL, MSG_ROUTING_NONE, NULL, NULL, NULL);
  NavigationControllerImpl& our_controller = our_contents.GetControllerImpl();
  our_controller.Restore(0, true, &entries);
  ASSERT_EQ(0u, entries.size());

  // Before navigating to the restored entry, it should have a restore_type
  // and no SiteInstance.
  EXPECT_EQ(NavigationEntryImpl::RESTORE_LAST_SESSION,
            NavigationEntryImpl::FromNavigationEntry(
                our_controller.GetEntryAtIndex(0))->restore_type());
  EXPECT_FALSE(NavigationEntryImpl::FromNavigationEntry(
      our_controller.GetEntryAtIndex(0))->site_instance());

  // After navigating, we should have one entry, and it should be "pending".
  // It should now have a SiteInstance and no restore_type.
  our_controller.GoToIndex(0);
  EXPECT_EQ(1, our_controller.GetEntryCount());
  EXPECT_EQ(our_controller.GetEntryAtIndex(0),
            our_controller.GetPendingEntry());
  EXPECT_EQ(0, our_controller.GetEntryAtIndex(0)->GetPageID());
  EXPECT_EQ(NavigationEntryImpl::RESTORE_NONE,
            NavigationEntryImpl::FromNavigationEntry
                (our_controller.GetEntryAtIndex(0))->restore_type());
  EXPECT_TRUE(NavigationEntryImpl::FromNavigationEntry(
      our_controller.GetEntryAtIndex(0))->site_instance());

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
  EXPECT_EQ(1, our_controller.GetEntryCount());
  EXPECT_EQ(0, our_controller.GetLastCommittedEntryIndex());
  EXPECT_FALSE(our_controller.GetPendingEntry());
  EXPECT_EQ(url,
            NavigationEntryImpl::FromNavigationEntry(
                our_controller.GetLastCommittedEntry())->site_instance()->
                    GetSite());
  EXPECT_EQ(NavigationEntryImpl::RESTORE_NONE,
            NavigationEntryImpl::FromNavigationEntry(
                our_controller.GetEntryAtIndex(0))->restore_type());
}

// Tests that we can still navigate to a restored entry after a different
// navigation fails and clears the pending entry.  http://crbug.com/90085
TEST_F(NavigationControllerTest, RestoreNavigateAfterFailure) {
  // Create a NavigationController with a restored set of tabs.
  GURL url("http://foo");
  std::vector<NavigationEntry*> entries;
  NavigationEntry* entry = NavigationControllerImpl::CreateNavigationEntry(
      url, content::Referrer(), content::PAGE_TRANSITION_RELOAD, false,
      std::string(), browser_context());
  entry->SetPageID(0);
  entry->SetTitle(ASCIIToUTF16("Title"));
  entry->SetContentState("state");
  entries.push_back(entry);
  WebContentsImpl our_contents(
      browser_context(), NULL, MSG_ROUTING_NONE, NULL, NULL, NULL);
  NavigationControllerImpl& our_controller = our_contents.GetControllerImpl();
  our_controller.Restore(0, true, &entries);
  ASSERT_EQ(0u, entries.size());

  // Before navigating to the restored entry, it should have a restore_type
  // and no SiteInstance.
  EXPECT_EQ(NavigationEntryImpl::RESTORE_LAST_SESSION,
            NavigationEntryImpl::FromNavigationEntry(
                our_controller.GetEntryAtIndex(0))->restore_type());
  EXPECT_FALSE(NavigationEntryImpl::FromNavigationEntry(
      our_controller.GetEntryAtIndex(0))->site_instance());

  // After navigating, we should have one entry, and it should be "pending".
  // It should now have a SiteInstance and no restore_type.
  our_controller.GoToIndex(0);
  EXPECT_EQ(1, our_controller.GetEntryCount());
  EXPECT_EQ(our_controller.GetEntryAtIndex(0),
            our_controller.GetPendingEntry());
  EXPECT_EQ(0, our_controller.GetEntryAtIndex(0)->GetPageID());
  EXPECT_EQ(NavigationEntryImpl::RESTORE_NONE,
            NavigationEntryImpl::FromNavigationEntry(
                our_controller.GetEntryAtIndex(0))->restore_type());
  EXPECT_TRUE(NavigationEntryImpl::FromNavigationEntry(
      our_controller.GetEntryAtIndex(0))->site_instance());

  // This pending navigation may have caused a different navigation to fail,
  // which causes the pending entry to be cleared.
  TestRenderViewHost* rvh =
      static_cast<TestRenderViewHost*>(our_contents.GetRenderViewHost());
  ViewHostMsg_DidFailProvisionalLoadWithError_Params fail_load_params;
  fail_load_params.frame_id = 1;
  fail_load_params.is_main_frame = true;
  fail_load_params.error_code = net::ERR_ABORTED;
  fail_load_params.error_description = string16();
  fail_load_params.url = url;
  fail_load_params.showing_repost_interstitial = false;
  rvh->OnMessageReceived(
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
  EXPECT_EQ(1, our_controller.GetEntryCount());
  EXPECT_EQ(0, our_controller.GetLastCommittedEntryIndex());
  EXPECT_FALSE(our_controller.GetPendingEntry());
  EXPECT_EQ(url,
            NavigationEntryImpl::FromNavigationEntry(
                our_controller.GetLastCommittedEntry())->site_instance()->
                    GetSite());
  EXPECT_EQ(NavigationEntryImpl::RESTORE_NONE,
            NavigationEntryImpl::FromNavigationEntry(
                our_controller.GetEntryAtIndex(0))->restore_type());
}

// Make sure that the page type and stuff is correct after an interstitial.
TEST_F(NavigationControllerTest, Interstitial) {
  NavigationControllerImpl& controller = controller_impl();
  // First navigate somewhere normal.
  const GURL url1("http://foo");
  controller.LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(0, url1);

  // Now navigate somewhere with an interstitial.
  const GURL url2("http://bar");
  controller.LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  NavigationEntryImpl::FromNavigationEntry(controller.GetPendingEntry())->
      set_page_type(content::PAGE_TYPE_INTERSTITIAL);

  // At this point the interstitial will be displayed and the load will still
  // be pending. If the user continues, the load will commit.
  test_rvh()->SendNavigate(1, url2);

  // The page should be a normal page again.
  EXPECT_EQ(url2, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(content::PAGE_TYPE_NORMAL,
            controller.GetLastCommittedEntry()->GetPageType());
}

TEST_F(NavigationControllerTest, RemoveEntry) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");
  const GURL url4("http://foo/4");
  const GURL url5("http://foo/5");
  const GURL pending_url("http://foo/pending");
  const GURL default_url("http://foo/default");

  controller.LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(0, url1);
  controller.LoadURL(
      url2, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(1, url2);
  controller.LoadURL(
      url3, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(2, url3);
  controller.LoadURL(
      url4, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(3, url4);
  controller.LoadURL(
      url5, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(4, url5);

  // Try to remove the last entry.  Will fail because it is the current entry.
  controller.RemoveEntryAtIndex(controller.GetEntryCount() - 1);
  EXPECT_EQ(5, controller.GetEntryCount());
  EXPECT_EQ(4, controller.GetLastCommittedEntryIndex());

  // Go back and remove the last entry.
  controller.GoBack();
  test_rvh()->SendNavigate(3, url4);
  controller.RemoveEntryAtIndex(controller.GetEntryCount() - 1);
  EXPECT_EQ(4, controller.GetEntryCount());
  EXPECT_EQ(3, controller.GetLastCommittedEntryIndex());
  EXPECT_FALSE(controller.GetPendingEntry());

  // Remove an entry which is not the last committed one.
  controller.RemoveEntryAtIndex(0);
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  EXPECT_FALSE(controller.GetPendingEntry());

  // Remove the 2 remaining entries.
  controller.RemoveEntryAtIndex(1);
  controller.RemoveEntryAtIndex(0);

  // This should leave us with only the last committed entry.
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
}

// Tests the transient entry, making sure it goes away with all navigations.
TEST_F(NavigationControllerTest, TransientEntry) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  const GURL url0("http://foo/0");
  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");
  const GURL url4("http://foo/4");
  const GURL transient_url("http://foo/transient");

  controller.LoadURL(
      url0, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(0, url0);
  controller.LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(1, url1);

  notifications.Reset();

  // Adding a transient with no pending entry.
  NavigationEntryImpl* transient_entry = new NavigationEntryImpl;
  transient_entry->SetURL(transient_url);
  controller.AddTransientEntry(transient_entry);

  // We should not have received any notifications.
  EXPECT_EQ(0U, notifications.size());

  // Check our state.
  EXPECT_EQ(transient_url, controller.GetActiveEntry()->GetURL());
  EXPECT_EQ(controller.GetEntryCount(), 3);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
  EXPECT_EQ(contents()->GetMaxPageID(), 1);

  // Navigate.
  controller.LoadURL(
      url2, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(2, url2);

  // We should have navigated, transient entry should be gone.
  EXPECT_EQ(url2, controller.GetActiveEntry()->GetURL());
  EXPECT_EQ(controller.GetEntryCount(), 3);

  // Add a transient again, then navigate with no pending entry this time.
  transient_entry = new NavigationEntryImpl;
  transient_entry->SetURL(transient_url);
  controller.AddTransientEntry(transient_entry);
  EXPECT_EQ(transient_url, controller.GetActiveEntry()->GetURL());
  test_rvh()->SendNavigate(3, url3);
  // Transient entry should be gone.
  EXPECT_EQ(url3, controller.GetActiveEntry()->GetURL());
  EXPECT_EQ(controller.GetEntryCount(), 4);

  // Initiate a navigation, add a transient then commit navigation.
  controller.LoadURL(
      url4, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  transient_entry = new NavigationEntryImpl;
  transient_entry->SetURL(transient_url);
  controller.AddTransientEntry(transient_entry);
  EXPECT_EQ(transient_url, controller.GetActiveEntry()->GetURL());
  test_rvh()->SendNavigate(4, url4);
  EXPECT_EQ(url4, controller.GetActiveEntry()->GetURL());
  EXPECT_EQ(controller.GetEntryCount(), 5);

  // Add a transient and go back.  This should simply remove the transient.
  transient_entry = new NavigationEntryImpl;
  transient_entry->SetURL(transient_url);
  controller.AddTransientEntry(transient_entry);
  EXPECT_EQ(transient_url, controller.GetActiveEntry()->GetURL());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
  controller.GoBack();
  // Transient entry should be gone.
  EXPECT_EQ(url4, controller.GetActiveEntry()->GetURL());
  EXPECT_EQ(controller.GetEntryCount(), 5);
  test_rvh()->SendNavigate(3, url3);

  // Add a transient and go to an entry before the current one.
  transient_entry = new NavigationEntryImpl;
  transient_entry->SetURL(transient_url);
  controller.AddTransientEntry(transient_entry);
  EXPECT_EQ(transient_url, controller.GetActiveEntry()->GetURL());
  controller.GoToIndex(1);
  // The navigation should have been initiated, transient entry should be gone.
  EXPECT_EQ(url1, controller.GetActiveEntry()->GetURL());
  // Visible entry does not update for history navigations until commit.
  EXPECT_EQ(url3, controller.GetVisibleEntry()->GetURL());
  test_rvh()->SendNavigate(1, url1);
  EXPECT_EQ(url1, controller.GetVisibleEntry()->GetURL());

  // Add a transient and go to an entry after the current one.
  transient_entry = new NavigationEntryImpl;
  transient_entry->SetURL(transient_url);
  controller.AddTransientEntry(transient_entry);
  EXPECT_EQ(transient_url, controller.GetActiveEntry()->GetURL());
  controller.GoToIndex(3);
  // The navigation should have been initiated, transient entry should be gone.
  // Because of the transient entry that is removed, going to index 3 makes us
  // land on url2 (which is visible after the commit).
  EXPECT_EQ(url2, controller.GetActiveEntry()->GetURL());
  EXPECT_EQ(url1, controller.GetVisibleEntry()->GetURL());
  test_rvh()->SendNavigate(2, url2);
  EXPECT_EQ(url2, controller.GetVisibleEntry()->GetURL());

  // Add a transient and go forward.
  transient_entry = new NavigationEntryImpl;
  transient_entry->SetURL(transient_url);
  controller.AddTransientEntry(transient_entry);
  EXPECT_EQ(transient_url, controller.GetActiveEntry()->GetURL());
  EXPECT_TRUE(controller.CanGoForward());
  controller.GoForward();
  // We should have navigated, transient entry should be gone.
  EXPECT_EQ(url3, controller.GetActiveEntry()->GetURL());
  EXPECT_EQ(url2, controller.GetVisibleEntry()->GetURL());
  test_rvh()->SendNavigate(3, url3);
  EXPECT_EQ(url3, controller.GetVisibleEntry()->GetURL());

  // Ensure the URLS are correct.
  EXPECT_EQ(controller.GetEntryCount(), 5);
  EXPECT_EQ(controller.GetEntryAtIndex(0)->GetURL(), url0);
  EXPECT_EQ(controller.GetEntryAtIndex(1)->GetURL(), url1);
  EXPECT_EQ(controller.GetEntryAtIndex(2)->GetURL(), url2);
  EXPECT_EQ(controller.GetEntryAtIndex(3)->GetURL(), url3);
  EXPECT_EQ(controller.GetEntryAtIndex(4)->GetURL(), url4);
}

// Tests that the URLs for renderer-initiated navigations are not displayed to
// the user until the navigation commits, to prevent URL spoof attacks.
// See http://crbug.com/99016.
TEST_F(NavigationControllerTest, DontShowRendererURLUntilCommit) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  const GURL url0("http://foo/0");
  const GURL url1("http://foo/1");

  // For typed navigations (browser-initiated), both active and visible entries
  // should update before commit.
  controller.LoadURL(url0, content::Referrer(),
                     content::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(url0, controller.GetActiveEntry()->GetURL());
  EXPECT_EQ(url0, controller.GetVisibleEntry()->GetURL());
  test_rvh()->SendNavigate(0, url0);

  // For link clicks (renderer-initiated navigations), the active entry should
  // update before commit but the visible should not.
  controller.LoadURLFromRenderer(url1, content::Referrer(),
                                 content::PAGE_TRANSITION_LINK,
                                 std::string());
  EXPECT_EQ(url1, controller.GetActiveEntry()->GetURL());
  EXPECT_EQ(url0, controller.GetVisibleEntry()->GetURL());
  EXPECT_TRUE(
      NavigationEntryImpl::FromNavigationEntry(controller.GetPendingEntry())->
          is_renderer_initiated());

  // After commit, both should be updated, and we should no longer treat the
  // entry as renderer-initiated.
  test_rvh()->SendNavigate(1, url1);
  EXPECT_EQ(url1, controller.GetActiveEntry()->GetURL());
  EXPECT_EQ(url1, controller.GetVisibleEntry()->GetURL());
  EXPECT_FALSE(
      NavigationEntryImpl::FromNavigationEntry(
          controller.GetLastCommittedEntry())->is_renderer_initiated());

  notifications.Reset();
}

// Tests that IsInPageNavigation returns appropriate results.  Prevents
// regression for bug 1126349.
TEST_F(NavigationControllerTest, IsInPageNavigation) {
  NavigationControllerImpl& controller = controller_impl();
  // Navigate to URL with no refs.
  const GURL url("http://www.google.com/home.html");
  test_rvh()->SendNavigate(0, url);

  // Reloading the page is not an in-page navigation.
  EXPECT_FALSE(controller.IsURLInPageNavigation(url));
  const GURL other_url("http://www.google.com/add.html");
  EXPECT_FALSE(controller.IsURLInPageNavigation(other_url));
  const GURL url_with_ref("http://www.google.com/home.html#my_ref");
  EXPECT_TRUE(controller.IsURLInPageNavigation(url_with_ref));

  // Navigate to URL with refs.
  test_rvh()->SendNavigate(1, url_with_ref);

  // Reloading the page is not an in-page navigation.
  EXPECT_FALSE(controller.IsURLInPageNavigation(url_with_ref));
  EXPECT_FALSE(controller.IsURLInPageNavigation(url));
  EXPECT_FALSE(controller.IsURLInPageNavigation(other_url));
  const GURL other_url_with_ref("http://www.google.com/home.html#my_other_ref");
  EXPECT_TRUE(controller.IsURLInPageNavigation(
      other_url_with_ref));
}

// Some pages can have subframes with the same base URL (minus the reference) as
// the main page. Even though this is hard, it can happen, and we don't want
// these subframe navigations to affect the toplevel document. They should
// instead be ignored.  http://crbug.com/5585
TEST_F(NavigationControllerTest, SameSubframe) {
  NavigationControllerImpl& controller = controller_impl();
  // Navigate the main frame.
  const GURL url("http://www.google.com/");
  test_rvh()->SendNavigate(0, url);

  // We should be at the first navigation entry.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);

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
  EXPECT_FALSE(controller.RendererDidNavigate(params, &details));

  // Nothing should have changed.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
}

// Make sure that on cloning a WebContentsImpl and going back needs_reload is
// false.
TEST_F(NavigationControllerTest, CloneAndGoBack) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);

  scoped_ptr<WebContents> clone(controller.GetWebContents()->Clone());

  ASSERT_EQ(2, clone->GetController().GetEntryCount());
  EXPECT_TRUE(clone->GetController().NeedsReload());
  clone->GetController().GoBack();
  // Navigating back should have triggered needs_reload_ to go false.
  EXPECT_FALSE(clone->GetController().NeedsReload());
}

// Make sure that cloning a WebContentsImpl doesn't copy interstitials.
TEST_F(NavigationControllerTest, CloneOmitsInterstitials) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);

  // Add an interstitial entry.  Should be deleted with controller.
  NavigationEntryImpl* interstitial_entry = new NavigationEntryImpl();
  interstitial_entry->set_page_type(content::PAGE_TYPE_INTERSTITIAL);
  controller.AddTransientEntry(interstitial_entry);

  scoped_ptr<WebContents> clone(controller.GetWebContents()->Clone());

  ASSERT_EQ(2, clone->GetController().GetEntryCount());
}

// Tests a subframe navigation while a toplevel navigation is pending.
// http://crbug.com/43967
TEST_F(NavigationControllerTest, SubframeWhilePending) {
  NavigationControllerImpl& controller = controller_impl();
  // Load the first page.
  const GURL url1("http://foo/");
  NavigateAndCommit(url1);

  // Now start a pending load to a totally different page, but don't commit it.
  const GURL url2("http://bar/");
  controller.LoadURL(
      url2, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());

  // Send a subframe update from the first page, as if one had just
  // automatically loaded. Auto subframes don't increment the page ID.
  const GURL url1_sub("http://foo/subframe");
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = controller.GetLastCommittedEntry()->GetPageID();
  params.url = url1_sub;
  params.transition = content::PAGE_TRANSITION_AUTO_SUBFRAME;
  params.should_update_history = false;
  params.gesture = NavigationGestureAuto;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url1_sub));
  content::LoadCommittedDetails details;

  // This should return false meaning that nothing was actually updated.
  EXPECT_FALSE(controller.RendererDidNavigate(params, &details));

  // The notification should have updated the last committed one, and not
  // the pending load.
  EXPECT_EQ(url1, controller.GetLastCommittedEntry()->GetURL());

  // The active entry should be unchanged by the subframe load.
  EXPECT_EQ(url2, controller.GetActiveEntry()->GetURL());
}

// Test CopyStateFrom with 2 urls, the first selected and nothing in the target.
TEST_F(NavigationControllerTest, CopyStateFrom) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);
  controller.GoBack();
  contents()->CommitPendingNavigation();

  scoped_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents()));
  NavigationControllerImpl& other_controller =
      other_contents->GetControllerImpl();
  other_controller.CopyStateFrom(controller);

  // other_controller should now contain 2 urls.
  ASSERT_EQ(2, other_controller.GetEntryCount());
  // We should be looking at the first one.
  ASSERT_EQ(0, other_controller.GetCurrentEntryIndex());

  EXPECT_EQ(url1, other_controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(0, other_controller.GetEntryAtIndex(0)->GetPageID());
  EXPECT_EQ(url2, other_controller.GetEntryAtIndex(1)->GetURL());
  // This is a different site than url1, so the IDs start again at 0.
  EXPECT_EQ(0, other_controller.GetEntryAtIndex(1)->GetPageID());

  // The max page ID map should be copied over and updated with the max page ID
  // from the current tab.
  SiteInstance* instance1 =
      GetSiteInstanceFromEntry(other_controller.GetEntryAtIndex(0));
  EXPECT_EQ(0, other_contents->GetMaxPageIDForSiteInstance(instance1));
}

// Tests CopyStateFromAndPrune with 2 urls in source, 1 in dest.
TEST_F(NavigationControllerTest, CopyStateFromAndPrune) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);

  // First two entries should have the same SiteInstance.
  SiteInstance* instance1 =
      GetSiteInstanceFromEntry(controller.GetEntryAtIndex(0));
  SiteInstance* instance2 =
      GetSiteInstanceFromEntry(controller.GetEntryAtIndex(1));
  EXPECT_EQ(instance1, instance2);
  EXPECT_EQ(0, controller.GetEntryAtIndex(0)->GetPageID());
  EXPECT_EQ(1, controller.GetEntryAtIndex(1)->GetPageID());
  EXPECT_EQ(1, contents()->GetMaxPageIDForSiteInstance(instance1));

  scoped_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents()));
  NavigationControllerImpl& other_controller =
      other_contents->GetControllerImpl();
  other_contents->NavigateAndCommit(url3);
  other_contents->ExpectSetHistoryLengthAndPrune(
      GetSiteInstanceFromEntry(other_controller.GetEntryAtIndex(0)), 2,
      other_controller.GetEntryAtIndex(0)->GetPageID());
  other_controller.CopyStateFromAndPrune(&controller);

  // other_controller should now contain the 3 urls: url1, url2 and url3.

  ASSERT_EQ(3, other_controller.GetEntryCount());

  ASSERT_EQ(2, other_controller.GetCurrentEntryIndex());

  EXPECT_EQ(url1, other_controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url2, other_controller.GetEntryAtIndex(1)->GetURL());
  EXPECT_EQ(url3, other_controller.GetEntryAtIndex(2)->GetURL());
  EXPECT_EQ(0, other_controller.GetEntryAtIndex(0)->GetPageID());
  EXPECT_EQ(1, other_controller.GetEntryAtIndex(1)->GetPageID());
  EXPECT_EQ(0, other_controller.GetEntryAtIndex(2)->GetPageID());

  // A new SiteInstance should be used for the new tab.
  SiteInstance* instance3 =
      GetSiteInstanceFromEntry(other_controller.GetEntryAtIndex(2));
  EXPECT_NE(instance3, instance1);

  // The max page ID map should be copied over and updated with the max page ID
  // from the current tab.
  EXPECT_EQ(1, other_contents->GetMaxPageIDForSiteInstance(instance1));
  EXPECT_EQ(0, other_contents->GetMaxPageIDForSiteInstance(instance3));
}

// Test CopyStateFromAndPrune with 2 urls, the first selected and nothing in
// the target.
TEST_F(NavigationControllerTest, CopyStateFromAndPrune2) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");
  const GURL url3("http://foo3");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);
  controller.GoBack();

  scoped_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents()));
  NavigationControllerImpl& other_controller =
      other_contents->GetControllerImpl();
  other_contents->ExpectSetHistoryLengthAndPrune(NULL, 1, -1);
  other_controller.CopyStateFromAndPrune(&controller);

  // other_controller should now contain the 1 url: url1.

  ASSERT_EQ(1, other_controller.GetEntryCount());

  ASSERT_EQ(0, other_controller.GetCurrentEntryIndex());

  EXPECT_EQ(url1, other_controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(0, other_controller.GetEntryAtIndex(0)->GetPageID());

  // The max page ID map should be copied over and updated with the max page ID
  // from the current tab.
  SiteInstance* instance1 =
      GetSiteInstanceFromEntry(other_controller.GetEntryAtIndex(0));
  EXPECT_EQ(0, other_contents->GetMaxPageIDForSiteInstance(instance1));
}

// Test CopyStateFromAndPrune with 2 urls, the first selected and nothing in
// the target.
TEST_F(NavigationControllerTest, CopyStateFromAndPrune3) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");
  const GURL url3("http://foo3");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);
  controller.GoBack();

  scoped_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents()));
  NavigationControllerImpl& other_controller =
      other_contents->GetControllerImpl();
  other_controller.LoadURL(
      url3, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  other_contents->ExpectSetHistoryLengthAndPrune(NULL, 1, -1);
  other_controller.CopyStateFromAndPrune(&controller);

  // other_controller should now contain 1 entry for url1, and a pending entry
  // for url3.

  ASSERT_EQ(1, other_controller.GetEntryCount());

  EXPECT_EQ(0, other_controller.GetCurrentEntryIndex());

  EXPECT_EQ(url1, other_controller.GetEntryAtIndex(0)->GetURL());

  // And there should be a pending entry for url3.
  ASSERT_TRUE(other_controller.GetPendingEntry());

  EXPECT_EQ(url3, other_controller.GetPendingEntry()->GetURL());

  // The max page ID map should be copied over and updated with the max page ID
  // from the current tab.
  SiteInstance* instance1 =
      GetSiteInstanceFromEntry(other_controller.GetEntryAtIndex(0));
  EXPECT_EQ(0, other_contents->GetMaxPageIDForSiteInstance(instance1));
}

// Tests CopyStateFromAndPrune with 3 urls in source, 1 in dest,
// when the max entry count is 3.  We should prune one entry.
TEST_F(NavigationControllerTest, CopyStateFromAndPruneMaxEntries) {
  NavigationControllerImpl& controller = controller_impl();
  size_t original_count = NavigationControllerImpl::max_entry_count();
  const int kMaxEntryCount = 3;

  NavigationControllerImpl::set_max_entry_count_for_testing(kMaxEntryCount);

  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");
  const GURL url4("http://foo/4");

  // Create a PrunedListener to observe prune notifications.
  PrunedListener listener(&controller);

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);
  NavigateAndCommit(url3);

  scoped_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents()));
  NavigationControllerImpl& other_controller =
      other_contents->GetControllerImpl();
  other_contents->NavigateAndCommit(url4);
  other_contents->ExpectSetHistoryLengthAndPrune(
      GetSiteInstanceFromEntry(other_controller.GetEntryAtIndex(0)), 2,
      other_controller.GetEntryAtIndex(0)->GetPageID());
  other_controller.CopyStateFromAndPrune(&controller);

  // We should have received a pruned notification.
  EXPECT_EQ(1, listener.notification_count_);
  EXPECT_TRUE(listener.details_.from_front);
  EXPECT_EQ(1, listener.details_.count);

  // other_controller should now contain only 3 urls: url2, url3 and url4.

  ASSERT_EQ(3, other_controller.GetEntryCount());

  ASSERT_EQ(2, other_controller.GetCurrentEntryIndex());

  EXPECT_EQ(url2, other_controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url3, other_controller.GetEntryAtIndex(1)->GetURL());
  EXPECT_EQ(url4, other_controller.GetEntryAtIndex(2)->GetURL());
  EXPECT_EQ(1, other_controller.GetEntryAtIndex(0)->GetPageID());
  EXPECT_EQ(2, other_controller.GetEntryAtIndex(1)->GetPageID());
  EXPECT_EQ(0, other_controller.GetEntryAtIndex(2)->GetPageID());

  NavigationControllerImpl::set_max_entry_count_for_testing(original_count);
}

// Tests that navigations initiated from the page (with the history object)
// work as expected without navigation entries.
TEST_F(NavigationControllerTest, HistoryNavigate) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);
  NavigateAndCommit(url3);
  controller.GoBack();
  contents()->CommitPendingNavigation();

  // Simulate the page calling history.back(), it should not create a pending
  // entry.
  contents()->OnGoToEntryAtOffset(-1);
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
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
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
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
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  message = process()->sink().GetFirstMessageMatching(ViewMsg_Navigate::ID);
  EXPECT_TRUE(message == NULL);
}

// Test call to PruneAllButActive for the only entry.
TEST_F(NavigationControllerTest, PruneAllButActiveForSingle) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo1");
  NavigateAndCommit(url1);
  controller.PruneAllButActive();

  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(controller.GetEntryAtIndex(0)->GetURL(), url1);
}

// Test call to PruneAllButActive for last entry.
TEST_F(NavigationControllerTest, PruneAllButActiveForLast) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);
  NavigateAndCommit(url3);
  controller.GoBack();
  controller.GoBack();
  contents()->CommitPendingNavigation();

  controller.PruneAllButActive();

  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(controller.GetEntryAtIndex(0)->GetURL(), url1);
}

// Test call to PruneAllButActive for intermediate entry.
TEST_F(NavigationControllerTest, PruneAllButActiveForIntermediate) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);
  NavigateAndCommit(url3);
  controller.GoBack();
  contents()->CommitPendingNavigation();

  controller.PruneAllButActive();

  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(controller.GetEntryAtIndex(0)->GetURL(), url2);
}

// Test call to PruneAllButActive for intermediate entry.
TEST_F(NavigationControllerTest, PruneAllButActiveForPending) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);
  NavigateAndCommit(url3);
  controller.GoBack();

  controller.PruneAllButActive();

  EXPECT_EQ(0, controller.GetPendingEntryIndex());
}

// Test call to PruneAllButActive for transient entry.
TEST_F(NavigationControllerTest, PruneAllButActiveForTransient) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url0("http://foo/0");
  const GURL url1("http://foo/1");
  const GURL transient_url("http://foo/transient");

  controller.LoadURL(
      url0, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(0, url0);
  controller.LoadURL(
      url1, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(1, url1);

  // Adding a transient with no pending entry.
  NavigationEntryImpl* transient_entry = new NavigationEntryImpl;
  transient_entry->SetURL(transient_url);
  controller.AddTransientEntry(transient_entry);

  controller.PruneAllButActive();

  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(controller.GetTransientEntry()->GetURL(), transient_url);
}

// Test to ensure that when we do a history navigation back to the current
// committed page (e.g., going forward to a slow-loading page, then pressing
// the back button), we just stop the navigation to prevent the throbber from
// running continuously. Otherwise, the RenderViewHost forces the throbber to
// start, but WebKit essentially ignores the navigation and never sends a
// message to stop the throbber.
TEST_F(NavigationControllerTest, StopOnHistoryNavigationToCurrentPage) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url0("http://foo/0");
  const GURL url1("http://foo/1");

  NavigateAndCommit(url0);
  NavigateAndCommit(url1);

  // Go back to the original page, then forward to the slow page, then back
  controller.GoBack();
  contents()->CommitPendingNavigation();

  controller.GoForward();
  EXPECT_EQ(1, controller.GetPendingEntryIndex());

  controller.GoBack();
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
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
                                 controller.session_id(), 0);
    controller.SetWindowID(window_id);

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
      history->SetOnBackendDestroyTask(MessageLoop::QuitClosure());
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
        controller.window_id(), controller.session_id(), false);

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
  NavigationControllerImpl& controller = controller_impl();
  controller.LoadURL(url0, GURL(), content::PAGE_TRANSITION_LINK);
  test_rvh()->SendNavigate(0, url0);

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
  NavigationControllerImpl& controller = controller_impl();
  test_rvh()->SendNavigate(0, url0);
  test_rvh()->SendNavigate(1, url1);
  test_rvh()->SendNavigate(2, url2);

  controller.GoBack();
  test_rvh()->SendNavigate(1, url1);

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
  NavigationControllerImpl& controller = controller_impl();
  test_rvh()->SendNavigate(0, url0);
  test_rvh()->SendNavigate(1, url1);
  test_rvh()->SendNavigate(2, url2);

  controller.GoBack();
  test_rvh()->SendNavigate(1, url1);

  controller.GoBack();
  test_rvh()->SendNavigate(0, url0);

  test_rvh()->SendNavigate(3, url2);

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
