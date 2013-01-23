// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/time.h"
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
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_view_host_observer.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_notification_tracker.h"
#include "net/base/net_util.h"
#include "skia/ext/platform_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/glue_serialize.h"

using base::Time;

namespace {

// Creates an image with a 1x1 SkBitmap of the specified |color|.
gfx::Image CreateImage(SkColor color) {
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, 1, 1);
  bitmap.allocPixels();
  bitmap.eraseColor(color);
  return gfx::Image::CreateFrom1xBitmap(bitmap);
}

// Returns true if images |a| and |b| have the same pixel data.
bool DoImagesMatch(const gfx::Image& a, const gfx::Image& b) {
  // Assume that if the 1x bitmaps match, the images match.
  SkBitmap a_bitmap = a.AsBitmap();
  SkBitmap b_bitmap = b.AsBitmap();

  if (a_bitmap.width() != b_bitmap.width() ||
      a_bitmap.height() != b_bitmap.height()) {
    return false;
  }
  SkAutoLockPixels a_bitmap_lock(a_bitmap);
  SkAutoLockPixels b_bitmap_lock(b_bitmap);
  return memcmp(a_bitmap.getPixels(),
                b_bitmap.getPixels(),
                a_bitmap.getSize()) == 0;
}

}  // namespace

namespace content {

// TimeSmoother tests ----------------------------------------------------------

// With no duplicates, GetSmoothedTime should be the identity
// function.
TEST(TimeSmoother, Basic) {
  NavigationControllerImpl::TimeSmoother smoother;
  for (int64 i = 1; i < 1000; ++i) {
    base::Time t = base::Time::FromInternalValue(i);
    EXPECT_EQ(t, smoother.GetSmoothedTime(t));
  }
}

// With a single duplicate and timestamps thereafter increasing by one
// microsecond, the smoothed time should always be one behind.
TEST(TimeSmoother, SingleDuplicate) {
  NavigationControllerImpl::TimeSmoother smoother;
  base::Time t = base::Time::FromInternalValue(1);
  EXPECT_EQ(t, smoother.GetSmoothedTime(t));
  for (int64 i = 1; i < 1000; ++i) {
    base::Time expected_t = base::Time::FromInternalValue(i + 1);
    t = base::Time::FromInternalValue(i);
    EXPECT_EQ(expected_t, smoother.GetSmoothedTime(t));
  }
}

// With k duplicates and timestamps thereafter increasing by one
// microsecond, the smoothed time should always be k behind.
TEST(TimeSmoother, ManyDuplicates) {
  const int64 kNumDuplicates = 100;
  NavigationControllerImpl::TimeSmoother smoother;
  base::Time t = base::Time::FromInternalValue(1);
  for (int64 i = 0; i < kNumDuplicates; ++i) {
    base::Time expected_t = base::Time::FromInternalValue(i + 1);
    EXPECT_EQ(expected_t, smoother.GetSmoothedTime(t));
  }
  for (int64 i = 1; i < 1000; ++i) {
    base::Time expected_t =
        base::Time::FromInternalValue(i + kNumDuplicates);
    t = base::Time::FromInternalValue(i);
    EXPECT_EQ(expected_t, smoother.GetSmoothedTime(t));
  }
}

// If the clock jumps far back enough after a run of duplicates, it
// should immediately jump to that value.
TEST(TimeSmoother, ClockBackwardsJump) {
  const int64 kNumDuplicates = 100;
  NavigationControllerImpl::TimeSmoother smoother;
  base::Time t = base::Time::FromInternalValue(1000);
  for (int64 i = 0; i < kNumDuplicates; ++i) {
    base::Time expected_t = base::Time::FromInternalValue(i + 1000);
    EXPECT_EQ(expected_t, smoother.GetSmoothedTime(t));
  }
  t = base::Time::FromInternalValue(500);
  EXPECT_EQ(t, smoother.GetSmoothedTime(t));
}

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
  tracker->ListenFor(NOTIFICATION_NAV_ENTRY_COMMITTED,
                     Source<NavigationController>(controller));
  tracker->ListenFor(NOTIFICATION_NAV_LIST_PRUNED,
                     Source<NavigationController>(controller));
  tracker->ListenFor(NOTIFICATION_NAV_ENTRY_CHANGED,
                     Source<NavigationController>(controller));
}

SiteInstance* GetSiteInstanceFromEntry(NavigationEntry* entry) {
  return NavigationEntryImpl::FromNavigationEntry(entry)->site_instance();
}

class TestWebContentsDelegate : public WebContentsDelegate {
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
  EXPECT_FALSE(controller.GetActiveEntry());
  EXPECT_FALSE(controller.GetVisibleEntry());
  EXPECT_FALSE(controller.GetLastCommittedEntry());
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), -1);
  EXPECT_EQ(controller.GetEntryCount(), 0);
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

TEST_F(NavigationControllerTest, GoToOffset) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  const int kNumUrls = 5;
  std::vector<GURL> urls(kNumUrls);
  for (int i = 0; i < kNumUrls; ++i) {
    urls[i] = GURL(base::StringPrintf("http://www.a.com/%d", i));
  }

  test_rvh()->SendNavigate(0, urls[0]);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(urls[0], controller.GetActiveEntry()->GetVirtualURL());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
  EXPECT_FALSE(controller.CanGoToOffset(1));

  for (int i = 1; i <= 4; ++i) {
    test_rvh()->SendNavigate(i, urls[i]);
    EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));
    EXPECT_EQ(urls[i], controller.GetActiveEntry()->GetVirtualURL());
    EXPECT_TRUE(controller.CanGoToOffset(-i));
    EXPECT_FALSE(controller.CanGoToOffset(-(i + 1)));
    EXPECT_FALSE(controller.CanGoToOffset(1));
  }

  // We have loaded 5 pages, and are currently at the last-loaded page.
  int url_index = 4;

  enum Tests {
    GO_TO_MIDDLE_PAGE = -2,
    GO_FORWARDS = 1,
    GO_BACKWARDS = -1,
    GO_TO_BEGINNING = -2,
    GO_TO_END = 4,
    NUM_TESTS = 5,
  };

  const int test_offsets[NUM_TESTS] = {
    GO_TO_MIDDLE_PAGE,
    GO_FORWARDS,
    GO_BACKWARDS,
    GO_TO_BEGINNING,
    GO_TO_END
  };

  for (int test = 0; test < NUM_TESTS; ++test) {
    int offset = test_offsets[test];
    controller.GoToOffset(offset);
    url_index += offset;
    // Check that the GoToOffset will land on the expected page.
    EXPECT_EQ(urls[url_index], controller.GetPendingEntry()->GetVirtualURL());
    test_rvh()->SendNavigate(url_index, urls[url_index]);
    EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));
    // Check that we can go to any valid offset into the history.
    for (size_t j = 0; j < urls.size(); ++j)
      EXPECT_TRUE(controller.CanGoToOffset(j - url_index));
    // Check that we can't go beyond the beginning or end of the history.
    EXPECT_FALSE(controller.CanGoToOffset(-(url_index + 1)));
    EXPECT_FALSE(controller.CanGoToOffset(urls.size() - url_index));
  }
}

TEST_F(NavigationControllerTest, LoadURL) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  controller.LoadURL(url1, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  // Creating a pending notification should not have issued any of the
  // notifications we're listening for.
  EXPECT_EQ(0U, notifications.size());

  // The load should now be pending.
  EXPECT_EQ(controller.GetEntryCount(), 0);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), -1);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_FALSE(controller.GetLastCommittedEntry());
  ASSERT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(controller.GetPendingEntry(), controller.GetActiveEntry());
  EXPECT_EQ(controller.GetPendingEntry(), controller.GetVisibleEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
  EXPECT_EQ(contents()->GetMaxPageID(), -1);

  // The timestamp should not have been set yet.
  EXPECT_TRUE(controller.GetPendingEntry()->GetTimestamp().is_null());

  // We should have gotten no notifications from the preceeding checks.
  EXPECT_EQ(0U, notifications.size());

  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  // The load should now be committed.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  ASSERT_TRUE(controller.GetActiveEntry());
  EXPECT_EQ(controller.GetActiveEntry(), controller.GetVisibleEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
  EXPECT_EQ(contents()->GetMaxPageID(), 0);

  // The timestamp should have been set.
  EXPECT_FALSE(controller.GetActiveEntry()->GetTimestamp().is_null());

  // Load another...
  controller.LoadURL(url2, Referrer(), PAGE_TRANSITION_TYPED, std::string());

  // The load should now be pending.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  ASSERT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(controller.GetPendingEntry(), controller.GetActiveEntry());
  EXPECT_EQ(controller.GetPendingEntry(), controller.GetVisibleEntry());
  // TODO(darin): maybe this should really be true?
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
  EXPECT_EQ(contents()->GetMaxPageID(), 0);

  EXPECT_TRUE(controller.GetPendingEntry()->GetTimestamp().is_null());

  // Simulate the beforeunload ack for the cross-site transition, and then the
  // commit.
  test_rvh()->SendShouldCloseACK(true);
  static_cast<TestRenderViewHost*>(
      contents()->GetPendingRenderViewHost())->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  // The load should now be committed.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  ASSERT_TRUE(controller.GetActiveEntry());
  EXPECT_EQ(controller.GetActiveEntry(), controller.GetVisibleEntry());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
  EXPECT_EQ(contents()->GetMaxPageID(), 1);

  EXPECT_FALSE(controller.GetActiveEntry()->GetTimestamp().is_null());
}

namespace {

base::Time GetFixedTime(base::Time time) {
  return time;
}

}  // namespace

TEST_F(NavigationControllerTest, LoadURLSameTime) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  // Set the clock to always return a timestamp of 1.
  controller.SetGetTimestampCallbackForTest(
      base::Bind(&GetFixedTime, base::Time::FromInternalValue(1)));

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  controller.LoadURL(url1, Referrer(), PAGE_TRANSITION_TYPED, std::string());

  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Load another...
  controller.LoadURL(url2, Referrer(), PAGE_TRANSITION_TYPED, std::string());

  // Simulate the beforeunload ack for the cross-site transition, and then the
  // commit.
  test_rvh()->SendShouldCloseACK(true);
  test_rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  // The two loads should now be committed.
  ASSERT_EQ(controller.GetEntryCount(), 2);

  // Timestamps should be distinct despite the clock returning the
  // same value.
  EXPECT_EQ(1u,
            controller.GetEntryAtIndex(0)->GetTimestamp().ToInternalValue());
  EXPECT_EQ(2u,
            controller.GetEntryAtIndex(1)->GetTimestamp().ToInternalValue());
}

void CheckNavigationEntryMatchLoadParams(
    NavigationController::LoadURLParams& load_params,
    NavigationEntryImpl* entry) {
  EXPECT_EQ(load_params.url, entry->GetURL());
  EXPECT_EQ(load_params.referrer.url, entry->GetReferrer().url);
  EXPECT_EQ(load_params.referrer.policy, entry->GetReferrer().policy);
  EXPECT_EQ(load_params.transition_type, entry->GetTransitionType());
  EXPECT_EQ(load_params.extra_headers, entry->extra_headers());

  EXPECT_EQ(load_params.is_renderer_initiated, entry->is_renderer_initiated());
  EXPECT_EQ(load_params.base_url_for_data_url, entry->GetBaseURLForDataURL());
  if (!load_params.virtual_url_for_data_url.is_empty()) {
    EXPECT_EQ(load_params.virtual_url_for_data_url, entry->GetVirtualURL());
  }
  if (NavigationController::UA_OVERRIDE_INHERIT !=
      load_params.override_user_agent) {
    bool should_override = (NavigationController::UA_OVERRIDE_TRUE ==
        load_params.override_user_agent);
    EXPECT_EQ(should_override, entry->GetIsOverridingUserAgent());
  }
  EXPECT_EQ(load_params.browser_initiated_post_data,
      entry->GetBrowserInitiatedPostData());
  EXPECT_EQ(load_params.transferred_global_request_id,
      entry->transferred_global_request_id());
}

TEST_F(NavigationControllerTest, LoadURLWithParams) {
  NavigationControllerImpl& controller = controller_impl();

  NavigationController::LoadURLParams load_params(GURL("http://foo"));
  load_params.referrer =
      Referrer(GURL("http://referrer"), WebKit::WebReferrerPolicyDefault);
  load_params.transition_type = PAGE_TRANSITION_GENERATED;
  load_params.extra_headers = "content-type: text/plain";
  load_params.load_type = NavigationController::LOAD_TYPE_DEFAULT;
  load_params.is_renderer_initiated = true;
  load_params.override_user_agent = NavigationController::UA_OVERRIDE_TRUE;
  load_params.transferred_global_request_id = GlobalRequestID(2,3);

  controller.LoadURLWithParams(load_params);
  NavigationEntryImpl* entry =
      NavigationEntryImpl::FromNavigationEntry(
          controller.GetPendingEntry());

  // The timestamp should not have been set yet.
  ASSERT_TRUE(entry);
  EXPECT_TRUE(entry->GetTimestamp().is_null());

  CheckNavigationEntryMatchLoadParams(load_params, entry);
}

TEST_F(NavigationControllerTest, LoadURLWithExtraParams_Data) {
  NavigationControllerImpl& controller = controller_impl();

  NavigationController::LoadURLParams load_params(
      GURL("data:text/html,dataurl"));
  load_params.load_type = NavigationController::LOAD_TYPE_DATA;
  load_params.base_url_for_data_url = GURL("http://foo");
  load_params.virtual_url_for_data_url = GURL("about:blank");
  load_params.override_user_agent = NavigationController::UA_OVERRIDE_FALSE;

  controller.LoadURLWithParams(load_params);
  NavigationEntryImpl* entry =
      NavigationEntryImpl::FromNavigationEntry(
          controller.GetPendingEntry());

  CheckNavigationEntryMatchLoadParams(load_params, entry);
}

TEST_F(NavigationControllerTest, LoadURLWithExtraParams_HttpPost) {
  NavigationControllerImpl& controller = controller_impl();

  NavigationController::LoadURLParams load_params(GURL("https://posturl"));
  load_params.transition_type = PAGE_TRANSITION_TYPED;
  load_params.load_type =
      NavigationController::LOAD_TYPE_BROWSER_INITIATED_HTTP_POST;
  load_params.override_user_agent = NavigationController::UA_OVERRIDE_TRUE;


  const unsigned char* raw_data =
      reinterpret_cast<const unsigned char*>("d\n\0a2");
  const int length = 5;
  std::vector<unsigned char> post_data_vector(raw_data, raw_data+length);
  scoped_refptr<base::RefCountedBytes> data =
      base::RefCountedBytes::TakeVector(&post_data_vector);
  load_params.browser_initiated_post_data = data.get();

  controller.LoadURLWithParams(load_params);
  NavigationEntryImpl* entry =
      NavigationEntryImpl::FromNavigationEntry(
          controller.GetPendingEntry());

  CheckNavigationEntryMatchLoadParams(load_params, entry);
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

  controller.LoadURL(url1, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(0U, notifications.size());
  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  ASSERT_TRUE(controller.GetActiveEntry());
  const base::Time timestamp = controller.GetActiveEntry()->GetTimestamp();
  EXPECT_FALSE(timestamp.is_null());

  controller.LoadURL(url1, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(0U, notifications.size());
  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  // We should not have produced a new session history entry.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  ASSERT_TRUE(controller.GetActiveEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());

  // The timestamp should have been updated.
  //
  // TODO(akalin): Change this EXPECT_GE (and other similar ones) to
  // EXPECT_GT once we guarantee that timestamps are unique.
  EXPECT_GE(controller.GetActiveEntry()->GetTimestamp(), timestamp);
}

// Tests loading a URL but discarding it before the load commits.
TEST_F(NavigationControllerTest, LoadURL_Discarded) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  controller.LoadURL(url1, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(0U, notifications.size());
  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  ASSERT_TRUE(controller.GetActiveEntry());
  const base::Time timestamp = controller.GetActiveEntry()->GetTimestamp();
  EXPECT_FALSE(timestamp.is_null());

  controller.LoadURL(url2, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  controller.DiscardNonCommittedEntries();
  EXPECT_EQ(0U, notifications.size());

  // Should not have produced a new session history entry.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  ASSERT_TRUE(controller.GetActiveEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());

  // Timestamp should not have changed.
  EXPECT_EQ(timestamp, controller.GetActiveEntry()->GetTimestamp());
}

// Tests navigations that come in unrequested. This happens when the user
// navigates from the web page, and here we test that there is no pending entry.
TEST_F(NavigationControllerTest, LoadURL_NoPending) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  // First make an existing committed entry.
  const GURL kExistingURL1("http://eh");
  controller.LoadURL(
      kExistingURL1, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(0, kExistingURL1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Do a new navigation without making a pending one.
  const GURL kNewURL("http://see");
  test_rvh()->SendNavigate(99, kNewURL);

  // There should no longer be any pending entry, and the third navigation we
  // just made should be committed.
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));
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
  controller.LoadURL(
      kExistingURL1, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(0, kExistingURL1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Make a pending entry to somewhere new.
  const GURL kExistingURL2("http://bee");
  controller.LoadURL(
      kExistingURL2, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(0U, notifications.size());

  // After the beforeunload but before it commits, do a new navigation.
  test_rvh()->SendShouldCloseACK(true);
  const GURL kNewURL("http://see");
  static_cast<TestRenderViewHost*>(
      contents()->GetPendingRenderViewHost())->SendNavigate(3, kNewURL);

  // There should no longer be any pending entry, and the third navigation we
  // just made should be committed.
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));
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
  controller.LoadURL(
      kExistingURL1, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(0, kExistingURL1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  const GURL kExistingURL2("http://foo/bee");
  controller.LoadURL(
      kExistingURL2, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(1, kExistingURL2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Now make a pending back/forward navigation. The zeroth entry should be
  // pending.
  controller.GoBack();
  EXPECT_EQ(0U, notifications.size());
  EXPECT_EQ(0, controller.GetPendingEntryIndex());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // Before that commits, do a new navigation.
  const GURL kNewURL("http://foo/see");
  LoadCommittedDetails details;
  test_rvh()->SendNavigate(3, kNewURL);

  // There should no longer be any pending entry, and the third navigation we
  // just made should be committed.
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));
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
  controller.LoadURL(
      kExistingURL1, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(0, kExistingURL1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  const GURL kExistingURL2("http://foo/bee");
  controller.LoadURL(
      kExistingURL2, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(1, kExistingURL2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Now make a pending new navigation.
  const GURL kNewURL("http://foo/see");
  controller.LoadURL(
      kNewURL, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(0U, notifications.size());
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // Before that commits, a back navigation from the renderer commits.
  test_rvh()->SendNavigate(0, kExistingURL1);

  // There should no longer be any pending entry, and the back navigation we
  // just made should be committed.
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));
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
      kNewURL, Referrer(), PAGE_TRANSITION_TYPED, std::string());
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
// We do not want to clear the pending entry, so that the user doesn't
// lose a typed URL.  (See http://crbug.com/9682.)
TEST_F(NavigationControllerTest, LoadURL_AbortDoesntCancelPending) {
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
      kNewURL, Referrer(), PAGE_TRANSITION_TYPED, std::string());
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

  // This should not clear the pending entry or notify of a navigation state
  // change, so that we keep displaying kNewURL (until the user clears it).
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(-1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(1, delegate->navigation_state_change_count());

  contents()->SetDelegate(NULL);
}

// Tests that the pending URL is not visible during a renderer-initiated
// redirect and abort.  See http://crbug.com/83031.
TEST_F(NavigationControllerTest, LoadURL_RedirectAbortDoesntShowPendingURL) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  // Set a WebContentsDelegate to listen for state changes.
  scoped_ptr<TestWebContentsDelegate> delegate(new TestWebContentsDelegate());
  EXPECT_FALSE(contents()->GetDelegate());
  contents()->SetDelegate(delegate.get());

  // Without any navigations, the renderer starts at about:blank.
  const GURL kExistingURL("about:blank");

  // Now make a pending new navigation, initiated by the renderer.
  const GURL kNewURL("http://eh");
  NavigationController::LoadURLParams load_url_params(kNewURL);
  load_url_params.transition_type = PAGE_TRANSITION_TYPED;
  load_url_params.is_renderer_initiated = true;
  controller.LoadURLWithParams(load_url_params);
  EXPECT_EQ(0U, notifications.size());
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(-1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(1, delegate->navigation_state_change_count());

  // There should be no visible entry (resulting in about:blank in the
  // omnibox), because it was renderer-initiated and there's no last committed
  // entry.
  EXPECT_FALSE(controller.GetVisibleEntry());

  // Now the navigation redirects.
  const GURL kRedirectURL("http://bee");
  test_rvh()->OnMessageReceived(
      ViewHostMsg_DidRedirectProvisionalLoad(0,  // routing_id
                                             -1,  // pending page_id
                                             kNewURL,  // old url
                                             kRedirectURL));  // new url

  // We don't want to change the NavigationEntry's url, in case it cancels.
  // Prevents regression of http://crbug.com/77786.
  EXPECT_EQ(kNewURL, controller.GetPendingEntry()->GetURL());

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

  // This should not clear the pending entry or notify of a navigation state
  // change.
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(-1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(1, delegate->navigation_state_change_count());

  // There should be no visible entry (resulting in about:blank in the
  // omnibox), ensuring no spoof is possible.
  EXPECT_FALSE(controller.GetVisibleEntry());

  contents()->SetDelegate(NULL);
}

TEST_F(NavigationControllerTest, Reload) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  const GURL url1("http://foo1");

  controller.LoadURL(url1, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(0U, notifications.size());
  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));
  ASSERT_TRUE(controller.GetActiveEntry());
  controller.GetActiveEntry()->SetTitle(ASCIIToUTF16("Title"));
  controller.Reload(true);
  EXPECT_EQ(0U, notifications.size());

  const base::Time timestamp = controller.GetActiveEntry()->GetTimestamp();
  EXPECT_FALSE(timestamp.is_null());

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
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Now the reload is committed.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());

  // The timestamp should have been updated.
  ASSERT_TRUE(controller.GetActiveEntry());
  EXPECT_GE(controller.GetActiveEntry()->GetTimestamp(), timestamp);
}

// Tests what happens when a reload navigation produces a new page.
TEST_F(NavigationControllerTest, Reload_GeneratesNewPage) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  controller.LoadURL(url1, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller.Reload(true);
  EXPECT_EQ(0U, notifications.size());

  test_rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Now the reload is committed.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

class TestNavigationObserver : public RenderViewHostObserver {
 public:
  TestNavigationObserver(RenderViewHost* render_view_host)
      : RenderViewHostObserver(render_view_host) {
  }

  const GURL& navigated_url() const {
    return navigated_url_;
  }

 protected:
  virtual void Navigate(const GURL& url) OVERRIDE {
    navigated_url_ = url;
  }

 private:
  GURL navigated_url_;
};

#if !defined(OS_ANDROID)  // http://crbug.com/157428
TEST_F(NavigationControllerTest, ReloadOriginalRequestURL) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);
  TestNavigationObserver observer(test_rvh());

  const GURL original_url("http://foo1");
  const GURL final_url("http://foo2");

  // Load up the original URL, but get redirected.
  controller.LoadURL(
      original_url, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(0U, notifications.size());
  test_rvh()->SendNavigateWithOriginalRequestURL(0, final_url, original_url);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  // The NavigationEntry should save both the original URL and the final
  // redirected URL.
  EXPECT_EQ(original_url, controller.GetActiveEntry()->GetOriginalRequestURL());
  EXPECT_EQ(final_url, controller.GetActiveEntry()->GetURL());

  // Reload using the original URL.
  controller.GetActiveEntry()->SetTitle(ASCIIToUTF16("Title"));
  controller.ReloadOriginalRequestURL(false);
  EXPECT_EQ(0U, notifications.size());

  // The reload is pending.  The request should point to the original URL.
  EXPECT_EQ(original_url, observer.navigated_url());
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

  // Send that the navigation has proceeded; say it got redirected again.
  test_rvh()->SendNavigate(0, final_url);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Now the reload is committed.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

#endif  // !defined(OS_ANDROID)

// Tests what happens when we navigate back successfully
TEST_F(NavigationControllerTest, Back) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  const GURL url1("http://foo1");
  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  const GURL url2("http://foo2");
  test_rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller.GoBack();
  EXPECT_EQ(0U, notifications.size());

  // We should now have a pending navigation to go back.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller.GetPendingEntryIndex(), 0);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoToOffset(-1));
  EXPECT_TRUE(controller.CanGoForward());
  EXPECT_TRUE(controller.CanGoToOffset(1));
  EXPECT_FALSE(controller.CanGoToOffset(2));  // Cannot go foward 2 steps.

  // Timestamp for entry 1 should be on or after that of entry 0.
  EXPECT_FALSE(controller.GetEntryAtIndex(0)->GetTimestamp().is_null());
  EXPECT_GE(controller.GetEntryAtIndex(1)->GetTimestamp(),
            controller.GetEntryAtIndex(0)->GetTimestamp());

  test_rvh()->SendNavigate(0, url2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  // The back navigation completed successfully.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoToOffset(-1));
  EXPECT_TRUE(controller.CanGoForward());
  EXPECT_TRUE(controller.CanGoToOffset(1));
  EXPECT_FALSE(controller.CanGoToOffset(2));  // Cannot go foward 2 steps.

  // Timestamp for entry 0 should be on or after that of entry 1
  // (since we went back to it).
  EXPECT_GE(controller.GetEntryAtIndex(0)->GetTimestamp(),
            controller.GetEntryAtIndex(1)->GetTimestamp());
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
      url1, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller.LoadURL(url2, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(
      NOTIFICATION_NAV_ENTRY_COMMITTED));

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
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

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
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  // controller.LoadURL(kUrl2, PAGE_TRANSITION_TYPED);
  test_rvh()->SendNavigate(1, kUrl2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Now start a new pending navigation and go back before it commits.
  controller.LoadURL(kUrl3, Referrer(), PAGE_TRANSITION_TYPED, std::string());
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
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  test_rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller.GoBack();
  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller.GoForward();

  // We should now have a pending navigation to go forward.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), 1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_TRUE(controller.CanGoToOffset(-1));
  EXPECT_FALSE(controller.CanGoToOffset(-2));  // Cannot go back 2 steps.
  EXPECT_FALSE(controller.CanGoForward());
  EXPECT_FALSE(controller.CanGoToOffset(1));

  // Timestamp for entry 0 should be on or after that of entry 1
  // (since we went back to it).
  EXPECT_FALSE(controller.GetEntryAtIndex(0)->GetTimestamp().is_null());
  EXPECT_GE(controller.GetEntryAtIndex(0)->GetTimestamp(),
            controller.GetEntryAtIndex(1)->GetTimestamp());

  test_rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  // The forward navigation completed successfully.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_TRUE(controller.CanGoToOffset(-1));
  EXPECT_FALSE(controller.CanGoToOffset(-2));  // Cannot go back 2 steps.
  EXPECT_FALSE(controller.CanGoForward());
  EXPECT_FALSE(controller.CanGoToOffset(1));

  // Timestamp for entry 1 should be on or after that of entry 0
  // (since we went forward to it).
  EXPECT_GE(controller.GetEntryAtIndex(1)->GetTimestamp(),
            controller.GetEntryAtIndex(0)->GetTimestamp());
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
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));
  test_rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  controller.GoBack();
  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

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
      NOTIFICATION_NAV_LIST_PRUNED,
      NOTIFICATION_NAV_ENTRY_COMMITTED));

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
  controller.LoadURL(url1, Referrer(), PAGE_TRANSITION_TYPED, std::string());

  EXPECT_EQ(0U, notifications.size());
  test_rvh()->SendNavigate(0, url2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Second request
  controller.LoadURL(url1, Referrer(), PAGE_TRANSITION_TYPED, std::string());

  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_EQ(url1, controller.GetActiveEntry()->GetURL());

  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 0;
  params.url = url2;
  params.transition = PAGE_TRANSITION_SERVER_REDIRECT;
  params.redirects.push_back(GURL("http://foo1"));
  params.redirects.push_back(GURL("http://foo2"));
  params.should_update_history = false;
  params.gesture = NavigationGestureAuto;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url2));

  LoadCommittedDetails details;

  EXPECT_EQ(0U, notifications.size());
  EXPECT_TRUE(controller.RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  EXPECT_TRUE(details.type == NAVIGATION_TYPE_SAME_PAGE);
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
  controller.LoadURL(url1, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  controller.GetActiveEntry()->SetHasPostData(true);

  EXPECT_EQ(0U, notifications.size());
  test_rvh()->SendNavigate(0, url2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Second request
  controller.LoadURL(url1, Referrer(), PAGE_TRANSITION_TYPED, std::string());

  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_EQ(url1, controller.GetActiveEntry()->GetURL());

  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 0;
  params.url = url2;
  params.transition = PAGE_TRANSITION_SERVER_REDIRECT;
  params.redirects.push_back(GURL("http://foo1"));
  params.redirects.push_back(GURL("http://foo2"));
  params.should_update_history = false;
  params.gesture = NavigationGestureAuto;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url2));

  LoadCommittedDetails details;

  EXPECT_EQ(0U, notifications.size());
  EXPECT_TRUE(controller.RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  EXPECT_TRUE(details.type == NAVIGATION_TYPE_SAME_PAGE);
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
  controller.LoadURL(url1, Referrer(), PAGE_TRANSITION_TYPED, std::string());

  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_EQ(url1, controller.GetActiveEntry()->GetURL());

  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 0;
  params.url = url2;
  params.transition = PAGE_TRANSITION_SERVER_REDIRECT;
  params.redirects.push_back(GURL("http://foo1"));
  params.redirects.push_back(GURL("http://foo2"));
  params.should_update_history = false;
  params.gesture = NavigationGestureAuto;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url2));

  LoadCommittedDetails details;

  EXPECT_EQ(0U, notifications.size());
  EXPECT_TRUE(controller.RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  EXPECT_TRUE(details.type == NAVIGATION_TYPE_NEW_PAGE);
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
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  const GURL url2("http://foo2");
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 1;
  params.url = url2;
  params.transition = PAGE_TRANSITION_MANUAL_SUBFRAME;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url2));

  LoadCommittedDetails details;
  EXPECT_TRUE(controller.RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));
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
  params.transition = PAGE_TRANSITION_AUTO_SUBFRAME;
  params.should_update_history = false;
  params.gesture = NavigationGestureAuto;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url));

  LoadCommittedDetails details;
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
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  const GURL url2("http://foo2");
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 0;
  params.url = url2;
  params.transition = PAGE_TRANSITION_AUTO_SUBFRAME;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url2));

  // Navigating should do nothing.
  LoadCommittedDetails details;
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
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  // First manual subframe navigation.
  const GURL url2("http://foo2");
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 1;
  params.url = url2;
  params.transition = PAGE_TRANSITION_MANUAL_SUBFRAME;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url2));

  // This should generate a new entry.
  LoadCommittedDetails details;
  EXPECT_TRUE(controller.RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(2, controller.GetEntryCount());

  // Second manual subframe navigation should also make a new entry.
  const GURL url3("http://foo3");
  params.page_id = 2;
  params.url = url3;
  EXPECT_TRUE(controller.RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());

  // Go back one.
  controller.GoBack();
  params.url = url2;
  params.page_id = 1;
  EXPECT_TRUE(controller.RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());

  // Go back one more.
  controller.GoBack();
  params.url = url1;
  params.page_id = 0;
  EXPECT_TRUE(controller.RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));
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
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  test_rvh()->SendNavigate(1, url2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

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
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Ensure main page navigation to same url respects the was_within_same_page
  // hint provided in the params.
  ViewHostMsg_FrameNavigate_Params self_params;
  self_params.page_id = 0;
  self_params.url = url1;
  self_params.transition = PAGE_TRANSITION_LINK;
  self_params.should_update_history = false;
  self_params.gesture = NavigationGestureUser;
  self_params.is_post = false;
  self_params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url1));
  self_params.was_within_same_page = true;

  LoadCommittedDetails details;
  EXPECT_TRUE(controller.RendererDidNavigate(self_params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_TRUE(details.is_in_page);
  EXPECT_TRUE(details.did_replace_entry);
  EXPECT_EQ(1, controller.GetEntryCount());

  // Fragment navigation to a new page_id.
  const GURL url2("http://foo#a");
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 1;
  params.url = url2;
  params.transition = PAGE_TRANSITION_LINK;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url2));

  // This should generate a new entry.
  EXPECT_TRUE(controller.RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_TRUE(details.is_in_page);
  EXPECT_FALSE(details.did_replace_entry);
  EXPECT_EQ(2, controller.GetEntryCount());

  // Go back one.
  ViewHostMsg_FrameNavigate_Params back_params(params);
  controller.GoBack();
  back_params.url = url1;
  back_params.page_id = 0;
  EXPECT_TRUE(controller.RendererDidNavigate(back_params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));
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
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));
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
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));
  EXPECT_FALSE(details.is_in_page);
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());
}

TEST_F(NavigationControllerTest, InPage_Replace) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  // Main page.
  const GURL url1("http://foo");
  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  // First navigation.
  const GURL url2("http://foo#a");
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 0;  // Same page_id
  params.url = url2;
  params.transition = PAGE_TRANSITION_LINK;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url2));

  // This should NOT generate a new entry, nor prune the list.
  LoadCommittedDetails details;
  EXPECT_TRUE(controller.RendererDidNavigate(params, &details));
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));
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
    EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));
  }

  // Navigate to a new page.
  {
    const GURL url("http://foo2/");
    test_rvh()->SendNavigate(1, url);
    controller.DocumentLoadedInFrame();
    EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));
  }

  // Navigate within the page.
  {
    const GURL url("http://foo2/#a");
    ViewHostMsg_FrameNavigate_Params params;
    params.page_id = 1;  // Same page_id
    params.url = url;
    params.transition = PAGE_TRANSITION_LINK;
    params.redirects.push_back(url);
    params.should_update_history = true;
    params.gesture = NavigationGestureUnknown;
    params.is_post = false;
    params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url));

    // This should NOT generate a new entry, nor prune the list.
    LoadCommittedDetails details;
    EXPECT_TRUE(controller.RendererDidNavigate(params, &details));
    EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));
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
    params.transition = PAGE_TRANSITION_CLIENT_REDIRECT;
    params.redirects.push_back(GURL("http://foo2/#a"));
    params.redirects.push_back(url);
    params.should_update_history = true;
    params.gesture = NavigationGestureUnknown;
    params.is_post = false;
    params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url));

    // This SHOULD generate a new entry.
    LoadCommittedDetails details;
    EXPECT_TRUE(controller.RendererDidNavigate(params, &details));
    EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));
    EXPECT_FALSE(details.is_in_page);
    EXPECT_EQ(3, controller.GetEntryCount());
  }

  // Verify that BACK brings us back to http://foo2/.
  {
    const GURL url("http://foo2/");
    controller.GoBack();
    test_rvh()->SendNavigate(1, url);
    EXPECT_TRUE(notifications.Check1AndReset(
        NOTIFICATION_NAV_ENTRY_COMMITTED));
    EXPECT_EQ(url, controller.GetActiveEntry()->GetURL());
  }
}

// NotificationObserver implementation used in verifying we've received the
// NOTIFICATION_NAV_LIST_PRUNED method.
class PrunedListener : public NotificationObserver {
 public:
  explicit PrunedListener(NavigationControllerImpl* controller)
      : notification_count_(0) {
    registrar_.Add(this, NOTIFICATION_NAV_LIST_PRUNED,
                   Source<NavigationController>(controller));
  }

  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NOTIFICATION_NAV_LIST_PRUNED) {
      notification_count_++;
      details_ = *(Details<PrunedDetails>(details).ptr());
    }
  }

  // Number of times NAV_LIST_PRUNED has been observed.
  int notification_count_;

  // Details from the last NAV_LIST_PRUNED.
  PrunedDetails details_;

 private:
  NotificationRegistrar registrar_;

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
        url, Referrer(), PAGE_TRANSITION_TYPED, std::string());
    test_rvh()->SendNavigate(url_index, url);
  }

  EXPECT_EQ(controller.GetEntryCount(), kMaxEntryCount);

  // Created a PrunedListener to observe prune notifications.
  PrunedListener listener(&controller);

  // Navigate some more.
  GURL url(StringPrintf("http://www.a.com/%d", url_index));
  controller.LoadURL(
      url, Referrer(), PAGE_TRANSITION_TYPED, std::string());
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
        url, Referrer(), PAGE_TRANSITION_TYPED, std::string());
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
      url, Referrer(), PAGE_TRANSITION_RELOAD, false, std::string(),
      browser_context());
  entry->SetPageID(0);
  entry->SetTitle(ASCIIToUTF16("Title"));
  entry->SetContentState("state");
  const base::Time timestamp = base::Time::Now();
  entry->SetTimestamp(timestamp);
  entries.push_back(entry);
  scoped_ptr<WebContentsImpl> our_contents(static_cast<WebContentsImpl*>(
      WebContents::Create(WebContents::CreateParams(browser_context()))));
  NavigationControllerImpl& our_controller = our_contents->GetController();
  our_controller.Restore(
      0,
      NavigationController::RESTORE_LAST_SESSION_EXITED_CLEANLY,
      &entries);
  ASSERT_EQ(0u, entries.size());

  // Before navigating to the restored entry, it should have a restore_type
  // and no SiteInstance.
  ASSERT_EQ(1, our_controller.GetEntryCount());
  EXPECT_EQ(NavigationEntryImpl::RESTORE_LAST_SESSION_EXITED_CLEANLY,
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

  // Timestamp should remain the same before the navigation finishes.
  EXPECT_EQ(timestamp, our_controller.GetEntryAtIndex(0)->GetTimestamp());

  // Say we navigated to that entry.
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = 0;
  params.url = url;
  params.transition = PAGE_TRANSITION_LINK;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url));
  LoadCommittedDetails details;
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
                    GetSiteURL());
  EXPECT_EQ(NavigationEntryImpl::RESTORE_NONE,
            NavigationEntryImpl::FromNavigationEntry(
                our_controller.GetEntryAtIndex(0))->restore_type());

  // Timestamp should have been updated.
  EXPECT_GE(our_controller.GetEntryAtIndex(0)->GetTimestamp(), timestamp);
}

// Tests that we can still navigate to a restored entry after a different
// navigation fails and clears the pending entry.  http://crbug.com/90085
TEST_F(NavigationControllerTest, RestoreNavigateAfterFailure) {
  // Create a NavigationController with a restored set of tabs.
  GURL url("http://foo");
  std::vector<NavigationEntry*> entries;
  NavigationEntry* entry = NavigationControllerImpl::CreateNavigationEntry(
      url, Referrer(), PAGE_TRANSITION_RELOAD, false, std::string(),
      browser_context());
  entry->SetPageID(0);
  entry->SetTitle(ASCIIToUTF16("Title"));
  entry->SetContentState("state");
  entries.push_back(entry);
  scoped_ptr<WebContentsImpl> our_contents(static_cast<WebContentsImpl*>(
      WebContents::Create(WebContents::CreateParams(browser_context()))));
  NavigationControllerImpl& our_controller = our_contents->GetController();
  our_controller.Restore(
      0, NavigationController::RESTORE_LAST_SESSION_EXITED_CLEANLY, &entries);
  ASSERT_EQ(0u, entries.size());

  // Before navigating to the restored entry, it should have a restore_type
  // and no SiteInstance.
  EXPECT_EQ(NavigationEntryImpl::RESTORE_LAST_SESSION_EXITED_CLEANLY,
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
      static_cast<TestRenderViewHost*>(our_contents->GetRenderViewHost());
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
  params.transition = PAGE_TRANSITION_LINK;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url));
  LoadCommittedDetails details;
  our_controller.RendererDidNavigate(params, &details);

  // There should be no pending entry and one committed one.
  EXPECT_EQ(1, our_controller.GetEntryCount());
  EXPECT_EQ(0, our_controller.GetLastCommittedEntryIndex());
  EXPECT_FALSE(our_controller.GetPendingEntry());
  EXPECT_EQ(url,
            NavigationEntryImpl::FromNavigationEntry(
                our_controller.GetLastCommittedEntry())->site_instance()->
                    GetSiteURL());
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
      url1, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(0, url1);

  // Now navigate somewhere with an interstitial.
  const GURL url2("http://bar");
  controller.LoadURL(
      url1, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  NavigationEntryImpl::FromNavigationEntry(controller.GetPendingEntry())->
      set_page_type(PAGE_TYPE_INTERSTITIAL);

  // At this point the interstitial will be displayed and the load will still
  // be pending. If the user continues, the load will commit.
  test_rvh()->SendNavigate(1, url2);

  // The page should be a normal page again.
  EXPECT_EQ(url2, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(PAGE_TYPE_NORMAL,
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
      url1, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(0, url1);
  controller.LoadURL(
      url2, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(1, url2);
  controller.LoadURL(
      url3, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(2, url3);
  controller.LoadURL(
      url4, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(3, url4);
  controller.LoadURL(
      url5, Referrer(), PAGE_TRANSITION_TYPED, std::string());
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
  const GURL url3_ref("http://foo/3#bar");
  const GURL url4("http://foo/4");
  const GURL transient_url("http://foo/transient");

  controller.LoadURL(
      url0, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(0, url0);
  controller.LoadURL(
      url1, Referrer(), PAGE_TRANSITION_TYPED, std::string());
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
      url2, Referrer(), PAGE_TRANSITION_TYPED, std::string());
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
      url4, Referrer(), PAGE_TRANSITION_TYPED, std::string());
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

  // Add a transient and do an in-page navigation, replacing the current entry.
  transient_entry = new NavigationEntryImpl;
  transient_entry->SetURL(transient_url);
  controller.AddTransientEntry(transient_entry);
  EXPECT_EQ(transient_url, controller.GetActiveEntry()->GetURL());
  test_rvh()->SendNavigate(3, url3_ref);
  // Transient entry should be gone.
  EXPECT_EQ(url3_ref, controller.GetActiveEntry()->GetURL());

  // Ensure the URLs are correct.
  EXPECT_EQ(controller.GetEntryCount(), 5);
  EXPECT_EQ(controller.GetEntryAtIndex(0)->GetURL(), url0);
  EXPECT_EQ(controller.GetEntryAtIndex(1)->GetURL(), url1);
  EXPECT_EQ(controller.GetEntryAtIndex(2)->GetURL(), url2);
  EXPECT_EQ(controller.GetEntryAtIndex(3)->GetURL(), url3_ref);
  EXPECT_EQ(controller.GetEntryAtIndex(4)->GetURL(), url4);
}

// Test that Reload initiates a new navigation to a transient entry's URL.
TEST_F(NavigationControllerTest, ReloadTransient) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url0("http://foo/0");
  const GURL url1("http://foo/1");
  const GURL transient_url("http://foo/transient");

  // Load |url0|, and start a pending navigation to |url1|.
  controller.LoadURL(
      url0, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(0, url0);
  controller.LoadURL(
      url1, Referrer(), PAGE_TRANSITION_TYPED, std::string());

  // A transient entry is added, interrupting the navigation.
  NavigationEntryImpl* transient_entry = new NavigationEntryImpl;
  transient_entry->SetURL(transient_url);
  controller.AddTransientEntry(transient_entry);
  EXPECT_TRUE(controller.GetTransientEntry());
  EXPECT_EQ(transient_url, controller.GetActiveEntry()->GetURL());

  // The page is reloaded, which should remove the pending entry for |url1| and
  // the transient entry for |transient_url|, and start a navigation to
  // |transient_url|.
  controller.Reload(true);
  EXPECT_FALSE(controller.GetTransientEntry());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(transient_url, controller.GetActiveEntry()->GetURL());
  ASSERT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetEntryAtIndex(0)->GetURL(), url0);

  // Load of |transient_url| completes.
  test_rvh()->SendNavigate(1, transient_url);
  ASSERT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetEntryAtIndex(0)->GetURL(), url0);
  EXPECT_EQ(controller.GetEntryAtIndex(1)->GetURL(), transient_url);
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
  controller.LoadURL(url0, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(url0, controller.GetActiveEntry()->GetURL());
  EXPECT_EQ(url0, controller.GetVisibleEntry()->GetURL());
  test_rvh()->SendNavigate(0, url0);

  // For link clicks (renderer-initiated navigations), the active entry should
  // update before commit but the visible should not.
  NavigationController::LoadURLParams load_url_params(url1);
  load_url_params.is_renderer_initiated = true;
  controller.LoadURLWithParams(load_url_params);
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
  params.transition = PAGE_TRANSITION_AUTO_SUBFRAME;
  params.should_update_history = false;
  params.gesture = NavigationGestureAuto;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(subframe));
  LoadCommittedDetails details;
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
  interstitial_entry->set_page_type(PAGE_TYPE_INTERSTITIAL);
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
      url2, Referrer(), PAGE_TRANSITION_TYPED, std::string());

  // Send a subframe update from the first page, as if one had just
  // automatically loaded. Auto subframes don't increment the page ID.
  const GURL url1_sub("http://foo/subframe");
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = controller.GetLastCommittedEntry()->GetPageID();
  params.url = url1_sub;
  params.transition = PAGE_TRANSITION_AUTO_SUBFRAME;
  params.should_update_history = false;
  params.gesture = NavigationGestureAuto;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url1_sub));
  LoadCommittedDetails details;

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
  NavigationControllerImpl& other_controller = other_contents->GetController();
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

  // Ensure the SessionStorageNamespaceMaps are the same size and have
  // the same partitons loaded.
  //
  // TODO(ajwong): We should load a url from a different partition earlier
  // to make sure this map has more than one entry.
  const SessionStorageNamespaceMap& session_storage_namespace_map =
      controller.GetSessionStorageNamespaceMap();
  const SessionStorageNamespaceMap& other_session_storage_namespace_map =
      other_controller.GetSessionStorageNamespaceMap();
  EXPECT_EQ(session_storage_namespace_map.size(),
            other_session_storage_namespace_map.size());
  for (SessionStorageNamespaceMap::const_iterator it =
           session_storage_namespace_map.begin();
       it != session_storage_namespace_map.end();
       ++it) {
    SessionStorageNamespaceMap::const_iterator other =
        other_session_storage_namespace_map.find(it->first);
    EXPECT_TRUE(other != other_session_storage_namespace_map.end());
  }
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
  NavigationControllerImpl& other_controller = other_contents->GetController();
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
  NavigationControllerImpl& other_controller = other_contents->GetController();
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
  NavigationControllerImpl& other_controller = other_contents->GetController();
  other_controller.LoadURL(
      url3, Referrer(), PAGE_TRANSITION_TYPED, std::string());
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
  NavigationControllerImpl& other_controller = other_contents->GetController();
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
      url0, Referrer(), PAGE_TRANSITION_TYPED, std::string());
  test_rvh()->SendNavigate(0, url0);
  controller.LoadURL(
      url1, Referrer(), PAGE_TRANSITION_TYPED, std::string());
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

TEST_F(NavigationControllerTest, IsInitialNavigation) {
  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  // Initial state.
  EXPECT_TRUE(controller.IsInitialNavigation());

  // After load, it is false.
  controller.DocumentLoadedInFrame();
  EXPECT_FALSE(controller.IsInitialNavigation());

  const GURL url1("http://foo1");
  test_rvh()->SendNavigate(0, url1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  // After commit, it stays false.
  EXPECT_FALSE(controller.IsInitialNavigation());
}

// Check that the favicon is not reused across a client redirect.
// (crbug.com/28515)
TEST_F(NavigationControllerTest, ClearFaviconOnRedirect) {
  const GURL kPageWithFavicon("http://withfavicon.html");
  const GURL kPageWithoutFavicon("http://withoutfavicon.html");
  const GURL kIconURL("http://withfavicon.ico");
  const gfx::Image kDefaultFavicon = FaviconStatus().image;

  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  test_rvh()->SendNavigate(0, kPageWithFavicon);
  EXPECT_TRUE(notifications.Check1AndReset(
      NOTIFICATION_NAV_ENTRY_COMMITTED));

  NavigationEntry* entry = controller.GetLastCommittedEntry();
  EXPECT_TRUE(entry);
  EXPECT_EQ(kPageWithFavicon, entry->GetURL());

  // Simulate Chromium having set the favicon for |kPageWithFavicon|.
  content::FaviconStatus& favicon_status = entry->GetFavicon();
  favicon_status.image = CreateImage(SK_ColorWHITE);
  favicon_status.url = kIconURL;
  favicon_status.valid = true;
  EXPECT_FALSE(DoImagesMatch(kDefaultFavicon, entry->GetFavicon().image));

  test_rvh()->SendNavigateWithTransition(
      0, // same page ID.
      kPageWithoutFavicon,
      PAGE_TRANSITION_CLIENT_REDIRECT);
  EXPECT_TRUE(notifications.Check1AndReset(
      NOTIFICATION_NAV_ENTRY_COMMITTED));

  entry = controller.GetLastCommittedEntry();
  EXPECT_TRUE(entry);
  EXPECT_EQ(kPageWithoutFavicon, entry->GetURL());

  EXPECT_TRUE(DoImagesMatch(kDefaultFavicon, entry->GetFavicon().image));
}

// Check that the favicon is not cleared for NavigationEntries which were
// previously navigated to.
TEST_F(NavigationControllerTest, BackNavigationDoesNotClearFavicon) {
  const GURL kUrl1("http://www.a.com/1");
  const GURL kUrl2("http://www.a.com/2");
  const GURL kIconURL("http://www.a.com/1/favicon.ico");

  NavigationControllerImpl& controller = controller_impl();
  TestNotificationTracker notifications;
  RegisterForAllNavNotifications(&notifications, &controller);

  test_rvh()->SendNavigate(0, kUrl1);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Simulate Chromium having set the favicon for |kUrl1|.
  gfx::Image favicon_image = CreateImage(SK_ColorWHITE);
  content::NavigationEntry* entry = controller.GetLastCommittedEntry();
  EXPECT_TRUE(entry);
  content::FaviconStatus& favicon_status = entry->GetFavicon();
  favicon_status.image = favicon_image;
  favicon_status.url = kIconURL;
  favicon_status.valid = true;

  // Navigate to another page and go back to the original page.
  test_rvh()->SendNavigate(1, kUrl2);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));
  test_rvh()->SendNavigateWithTransition(
      0,
      kUrl1,
      PAGE_TRANSITION_FORWARD_BACK);
  EXPECT_TRUE(notifications.Check1AndReset(NOTIFICATION_NAV_ENTRY_COMMITTED));

  // Verify that the favicon for the page at |kUrl1| was not cleared.
  entry = controller.GetEntryAtIndex(0);
  EXPECT_TRUE(entry);
  EXPECT_EQ(kUrl1, entry->GetURL());
  EXPECT_TRUE(DoImagesMatch(favicon_image, entry->GetFavicon().image));
}

// The test crashes on android: http://crbug.com/170449
#if defined(OS_ANDROID)
#define MAYBE_PurgeScreenshot DISABLED_PurgeScreenshot
#else
#define MAYBE_PurgeScreenshot PurgeScreenshot
#endif
// Tests that screenshot are purged correctly.
TEST_F(NavigationControllerTest, MAYBE_PurgeScreenshot) {
  NavigationControllerImpl& controller = controller_impl();

  // Prepare some data to use as screenshot for each navigation.
  skia::PlatformBitmap bitmap;
  ASSERT_TRUE(bitmap.Allocate(1, 1, false));
  NavigationEntryImpl* entry;

  // Navigate enough times to make sure that some screenshots are purged.
  for (int i = 0; i < 12; ++i) {
    const GURL url(base::StringPrintf("http://foo%d/", i));
    NavigateAndCommit(url);
    EXPECT_EQ(i, controller.GetCurrentEntryIndex());

    entry = NavigationEntryImpl::FromNavigationEntry(
        controller.GetActiveEntry());
    controller.OnScreenshotTaken(entry->GetUniqueID(), &bitmap, true);
    EXPECT_TRUE(entry->screenshot());
  }
  NavigateAndCommit(GURL("https://foo/"));
  EXPECT_EQ(13, controller.GetEntryCount());

  for (int i = 0; i < 2; ++i) {
    entry = NavigationEntryImpl::FromNavigationEntry(
        controller.GetEntryAtIndex(i));
    EXPECT_FALSE(entry->screenshot()) << "Screenshot " << i << " not purged";
  }

  for (int i = 2; i < controller.GetEntryCount() - 1; ++i) {
    entry = NavigationEntryImpl::FromNavigationEntry(
        controller.GetEntryAtIndex(i));
    EXPECT_TRUE(entry->screenshot()) << "Screenshot not found for " << i;
  }

  // Navigate to index 5 and then try to assign screenshot to all entries.
  controller.GoToIndex(5);
  contents()->CommitPendingNavigation();
  EXPECT_EQ(5, controller.GetCurrentEntryIndex());
  for (int i = 0; i < controller.GetEntryCount() - 1; ++i) {
    entry = NavigationEntryImpl::FromNavigationEntry(
        controller.GetEntryAtIndex(i));
    controller.OnScreenshotTaken(entry->GetUniqueID(), &bitmap, true);
  }

  for (int i = 10; i <= 12; ++i) {
    entry = NavigationEntryImpl::FromNavigationEntry(
        controller.GetEntryAtIndex(i));
    EXPECT_FALSE(entry->screenshot()) << "Screenshot " << i << " not purged";
    controller.OnScreenshotTaken(entry->GetUniqueID(), &bitmap, true);
  }

  // Navigate to index 7 and assign screenshot to all entries.
  controller.GoToIndex(7);
  contents()->CommitPendingNavigation();
  EXPECT_EQ(7, controller.GetCurrentEntryIndex());
  for (int i = 0; i < controller.GetEntryCount() - 1; ++i) {
    entry = NavigationEntryImpl::FromNavigationEntry(
        controller.GetEntryAtIndex(i));
    controller.OnScreenshotTaken(entry->GetUniqueID(), &bitmap, true);
  }

  for (int i = 0; i < 2; ++i) {
    entry = NavigationEntryImpl::FromNavigationEntry(
        controller.GetEntryAtIndex(i));
    EXPECT_FALSE(entry->screenshot()) << "Screenshot " << i << " not purged";
  }
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
    HistoryService* history = HistoryServiceFactory::GetForProfiles(
        profile(), Profile::IMPLICIT_ACCESS);
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
  controller.LoadURL(url0, GURL(), PAGE_TRANSITION_LINK);
  test_rvh()->SendNavigate(0, url0);

  GetLastSession();

  session_helper_.AssertSingleWindowWithSingleTab(windows_, 1);
  session_helper_.AssertTabEquals(0, 0, 1, *(windows_[0]->tabs[0]));
  TabNavigation nav1(0, url0, GURL(), string16(),
                     webkit_glue::CreateHistoryStateForURL(url0),
                     PAGE_TRANSITION_LINK);
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
                    PAGE_TRANSITION_LINK);
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
                    PAGE_TRANSITION_LINK);
  session_helper_.AssertNavigationEquals(nav,
                                         windows_[0]->tabs[0]->navigations[0]);
  nav.set_url(url2);
  session_helper_.AssertNavigationEquals(nav,
                                         windows_[0]->tabs[0]->navigations[1]);
}
*/

}  // namespace content
