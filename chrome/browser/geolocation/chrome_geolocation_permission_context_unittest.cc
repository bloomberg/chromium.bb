// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/geolocation/chrome_geolocation_permission_context.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/geolocation/arbitrator_dependency_factories_for_test.h"
#include "content/browser/geolocation/location_arbitrator.h"
#include "content/browser/geolocation/location_provider.h"
#include "content/browser/geolocation/mock_location_provider.h"
#include "content/browser/renderer_host/mock_render_process_host.h"
#include "content/browser/tab_contents/navigation_details.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

// ClosedDelegateTracker ------------------------------------------------------

namespace {

// We need to track which infobars were closed.
class ClosedDelegateTracker : public content::NotificationObserver {
 public:
  ClosedDelegateTracker();
  virtual ~ClosedDelegateTracker();

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

  size_t size() const {
    return removed_infobar_delegates_.size();
  }

  bool Contains(InfoBarDelegate* delegate) const;
  void Clear();

 private:
  content::NotificationRegistrar registrar_;
  std::set<InfoBarDelegate*> removed_infobar_delegates_;
};

ClosedDelegateTracker::ClosedDelegateTracker() {
  registrar_.Add(this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
                 content::NotificationService::AllSources());
}

ClosedDelegateTracker::~ClosedDelegateTracker() {
}

void ClosedDelegateTracker::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED);
  removed_infobar_delegates_.insert(
      content::Details<InfoBarRemovedDetails>(details)->first);
}

bool ClosedDelegateTracker::Contains(InfoBarDelegate* delegate) const {
  return removed_infobar_delegates_.count(delegate) != 0;
}

void ClosedDelegateTracker::Clear() {
  removed_infobar_delegates_.clear();
}

}  // namespace


// GeolocationPermissionContextTests ------------------------------------------

// This class sets up GeolocationArbitrator.
class GeolocationPermissionContextTests : public TabContentsWrapperTestHarness {
 public:
  GeolocationPermissionContextTests();

 protected:
  virtual ~GeolocationPermissionContextTests();

  int process_id() {
    return contents()->render_view_host()->process()->GetID();
  }
  int process_id_for_tab(int tab) {
    return extra_tabs_[tab]->tab_contents()->render_view_host()->process()->
        GetID();
  }
  int render_id() { return contents()->render_view_host()->routing_id(); }
  int render_id_for_tab(int tab) {
    return extra_tabs_[tab]->tab_contents()->render_view_host()->routing_id();
  }
  int bridge_id() const { return 42; }  // Not relevant at this level.
  InfoBarTabHelper* infobar_tab_helper() {
    return contents_wrapper()->infobar_tab_helper();
  }

  void RequestGeolocationPermission(int render_process_id,
                                    int render_view_id,
                                    int bridge_id,
                                    const GURL& requesting_frame);
  void PermissionResponse(int render_process_id,
                          int render_view_id,
                          int bridge_id,
                          bool allowed);
  void CheckPermissionMessageSent(int bridge_id, bool allowed);
  void CheckPermissionMessageSentForTab(int tab, int bridge_id, bool allowed);
  void CheckPermissionMessageSentInternal(MockRenderProcessHost* process,
                                          int bridge_id,
                                          bool allowed);
  void AddNewTab(const GURL& url);
  void CheckTabContentsState(const GURL& requesting_frame,
                             ContentSetting expected_content_setting);

  scoped_refptr<ChromeGeolocationPermissionContext>
      geolocation_permission_context_;
  ClosedDelegateTracker closed_delegate_tracker_;
  ScopedVector<TabContentsWrapper> extra_tabs_;

 private:
  // TabContentsWrapperTestHarness:
  virtual void SetUp();
  virtual void TearDown();

  content::TestBrowserThread ui_thread_;
  scoped_refptr<GeolocationArbitratorDependencyFactory> dependency_factory_;

  // A map between renderer child id and a pair represending the bridge id and
  // whether the requested permission was allowed.
  base::hash_map<int, std::pair<int, bool> > responses_;
};

GeolocationPermissionContextTests::GeolocationPermissionContextTests()
    : TabContentsWrapperTestHarness(),
      ui_thread_(BrowserThread::UI, MessageLoop::current()),
      dependency_factory_(
          new GeolocationArbitratorDependencyFactoryWithLocationProvider(
              &NewAutoSuccessMockNetworkLocationProvider)) {
}

GeolocationPermissionContextTests::~GeolocationPermissionContextTests() {
}

void GeolocationPermissionContextTests::RequestGeolocationPermission(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame) {
  geolocation_permission_context_->RequestGeolocationPermission(
      render_process_id, render_view_id, bridge_id, requesting_frame,
      base::Bind(&GeolocationPermissionContextTests::PermissionResponse,
                 base::Unretained(this),
                 render_process_id,
                 render_view_id,
                 bridge_id));
}

void GeolocationPermissionContextTests::PermissionResponse(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    bool allowed) {
  responses_[render_process_id] = std::make_pair(bridge_id, allowed);
}

void GeolocationPermissionContextTests::CheckPermissionMessageSent(
    int bridge_id,
    bool allowed) {
  CheckPermissionMessageSentInternal(process(), bridge_id, allowed);
}

void GeolocationPermissionContextTests::CheckPermissionMessageSentForTab(
    int tab,
    int bridge_id,
    bool allowed) {
  CheckPermissionMessageSentInternal(static_cast<MockRenderProcessHost*>(
      extra_tabs_[tab]->tab_contents()->render_view_host()->process()),
      bridge_id, allowed);
}

void GeolocationPermissionContextTests::CheckPermissionMessageSentInternal(
    MockRenderProcessHost* process,
    int bridge_id,
    bool allowed) {
  ASSERT_EQ(responses_.count(process->GetID()), 1U);
  EXPECT_EQ(bridge_id, responses_[process->GetID()].first);
  EXPECT_EQ(allowed, responses_[process->GetID()].second);
  responses_.erase(process->GetID());
}

void GeolocationPermissionContextTests::AddNewTab(const GURL& url) {
  TabContents* new_tab =
      new TabContents(profile(), NULL, MSG_ROUTING_NONE, NULL, NULL);
  new_tab->controller().LoadURL(url, content::Referrer(),
                                content::PAGE_TRANSITION_TYPED, std::string());
  static_cast<TestRenderViewHost*>(new_tab->render_manager_for_testing()->
      current_host())->SendNavigate(extra_tabs_.size() + 1, url);
  extra_tabs_.push_back(new TabContentsWrapper(new_tab));
}

void GeolocationPermissionContextTests::CheckTabContentsState(
    const GURL& requesting_frame,
    ContentSetting expected_content_setting) {
  TabSpecificContentSettings* content_settings =
      contents_wrapper()->content_settings();
  const GeolocationSettingsState::StateMap& state_map =
      content_settings->geolocation_settings_state().state_map();
  EXPECT_EQ(1U, state_map.count(requesting_frame.GetOrigin()));
  EXPECT_EQ(0U, state_map.count(requesting_frame));
  GeolocationSettingsState::StateMap::const_iterator settings =
      state_map.find(requesting_frame.GetOrigin());
  ASSERT_FALSE(settings == state_map.end())
      << "geolocation state not found " << requesting_frame;
  EXPECT_EQ(expected_content_setting, settings->second);
}

void GeolocationPermissionContextTests::SetUp() {
  TabContentsWrapperTestHarness::SetUp();
  GeolocationArbitrator::SetDependencyFactoryForTest(
      dependency_factory_.get());
  geolocation_permission_context_ =
      new ChromeGeolocationPermissionContext(profile());
}

void GeolocationPermissionContextTests::TearDown() {
  GeolocationArbitrator::SetDependencyFactoryForTest(NULL);
  TabContentsWrapperTestHarness::TearDown();
}


// Tests ----------------------------------------------------------------------

TEST_F(GeolocationPermissionContextTests, SinglePermission) {
  GURL requesting_frame("http://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  EXPECT_EQ(0U, infobar_tab_helper()->infobar_count());
  RequestGeolocationPermission(
      process_id(), render_id(), bridge_id(), requesting_frame);
  ASSERT_EQ(1U, infobar_tab_helper()->infobar_count());
  ConfirmInfoBarDelegate* infobar_0 = infobar_tab_helper()->
      GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  infobar_0->Cancel();
  infobar_tab_helper()->RemoveInfoBar(infobar_0);
  EXPECT_EQ(1U, closed_delegate_tracker_.size());
  EXPECT_TRUE(closed_delegate_tracker_.Contains(infobar_0));
  infobar_0->InfoBarClosed();
}

TEST_F(GeolocationPermissionContextTests, QueuedPermission) {
  GURL requesting_frame_0("http://www.example.com/geolocation");
  GURL requesting_frame_1("http://www.example-2.com/geolocation");
  EXPECT_EQ(CONTENT_SETTING_ASK,
      profile()->GetHostContentSettingsMap()->GetContentSetting(
          requesting_frame_0,
          requesting_frame_0,
          CONTENT_SETTINGS_TYPE_GEOLOCATION,
          std::string()));
  EXPECT_EQ(CONTENT_SETTING_ASK,
      profile()->GetHostContentSettingsMap()->GetContentSetting(
          requesting_frame_1,
          requesting_frame_0,
          CONTENT_SETTINGS_TYPE_GEOLOCATION,
          std::string()));


  NavigateAndCommit(requesting_frame_0);
  EXPECT_EQ(0U, infobar_tab_helper()->infobar_count());
  // Request permission for two frames.
  RequestGeolocationPermission(
      process_id(), render_id(), bridge_id(), requesting_frame_0);
  RequestGeolocationPermission(
      process_id(), render_id(), bridge_id() + 1, requesting_frame_1);
  // Ensure only one infobar is created.
  ASSERT_EQ(1U, infobar_tab_helper()->infobar_count());
  ConfirmInfoBarDelegate* infobar_0 = infobar_tab_helper()->
      GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_0);
  string16 text_0 = infobar_0->GetMessageText();

  // Accept the first frame.
  infobar_0->Accept();
  CheckTabContentsState(requesting_frame_0, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(bridge_id(), true);

  infobar_tab_helper()->RemoveInfoBar(infobar_0);
  EXPECT_EQ(1U, closed_delegate_tracker_.size());
  EXPECT_TRUE(closed_delegate_tracker_.Contains(infobar_0));
  closed_delegate_tracker_.Clear();
  infobar_0->InfoBarClosed();
  // Now we should have a new infobar for the second frame.
  ASSERT_EQ(1U, infobar_tab_helper()->infobar_count());

  ConfirmInfoBarDelegate* infobar_1 = infobar_tab_helper()->
      GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_1);
  string16 text_1 = infobar_1->GetMessageText();
  EXPECT_NE(text_0, text_1);

  // Cancel (block) this frame.
  infobar_1->Cancel();
  CheckTabContentsState(requesting_frame_1, CONTENT_SETTING_BLOCK);
  CheckPermissionMessageSent(bridge_id() + 1, false);
  infobar_tab_helper()->RemoveInfoBar(infobar_1);
  EXPECT_EQ(1U, closed_delegate_tracker_.size());
  EXPECT_TRUE(closed_delegate_tracker_.Contains(infobar_1));
  infobar_1->InfoBarClosed();
  EXPECT_EQ(0U, infobar_tab_helper()->infobar_count());
  // Ensure the persisted permissions are ok.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
      profile()->GetHostContentSettingsMap()->GetContentSetting(
          requesting_frame_0,
          requesting_frame_0,
          CONTENT_SETTINGS_TYPE_GEOLOCATION,
          std::string()));

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
      profile()->GetHostContentSettingsMap()->GetContentSetting(
          requesting_frame_1,
          requesting_frame_0,
          CONTENT_SETTINGS_TYPE_GEOLOCATION,
          std::string()));
}

TEST_F(GeolocationPermissionContextTests, CancelGeolocationPermissionRequest) {
  GURL requesting_frame_0("http://www.example.com/geolocation");
  GURL requesting_frame_1("http://www.example-2.com/geolocation");
  EXPECT_EQ(CONTENT_SETTING_ASK,
      profile()->GetHostContentSettingsMap()->GetContentSetting(
          requesting_frame_0,
          requesting_frame_0,
          CONTENT_SETTINGS_TYPE_GEOLOCATION,
          std::string()));

  EXPECT_EQ(CONTENT_SETTING_ASK,
      profile()->GetHostContentSettingsMap()->GetContentSetting(
          requesting_frame_1,
          requesting_frame_0,
          CONTENT_SETTINGS_TYPE_GEOLOCATION,
          std::string()));


  NavigateAndCommit(requesting_frame_0);
  EXPECT_EQ(0U, infobar_tab_helper()->infobar_count());
  // Request permission for two frames.
  RequestGeolocationPermission(
      process_id(), render_id(), bridge_id(), requesting_frame_0);
  RequestGeolocationPermission(
      process_id(), render_id(), bridge_id() + 1, requesting_frame_1);
  ASSERT_EQ(1U, infobar_tab_helper()->infobar_count());

  ConfirmInfoBarDelegate* infobar_0 = infobar_tab_helper()->
      GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_0);
  string16 text_0 = infobar_0->GetMessageText();

  // Simulate the frame going away, ensure the infobar for this frame
  // is removed and the next pending infobar is created.
  geolocation_permission_context_->CancelGeolocationPermissionRequest(
      process_id(), render_id(), bridge_id(), requesting_frame_0);
  EXPECT_EQ(1U, closed_delegate_tracker_.size());
  EXPECT_TRUE(closed_delegate_tracker_.Contains(infobar_0));
  closed_delegate_tracker_.Clear();
  infobar_0->InfoBarClosed();
  ASSERT_EQ(1U, infobar_tab_helper()->infobar_count());

  ConfirmInfoBarDelegate* infobar_1 = infobar_tab_helper()->
      GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_1);
  string16 text_1 = infobar_1->GetMessageText();
  EXPECT_NE(text_0, text_1);

  // Allow this frame.
  infobar_1->Accept();
  CheckTabContentsState(requesting_frame_1, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(bridge_id() + 1, true);
  infobar_tab_helper()->RemoveInfoBar(infobar_1);
  EXPECT_EQ(1U, closed_delegate_tracker_.size());
  EXPECT_TRUE(closed_delegate_tracker_.Contains(infobar_1));
  infobar_1->InfoBarClosed();
  EXPECT_EQ(0U, infobar_tab_helper()->infobar_count());
  // Ensure the persisted permissions are ok.
  EXPECT_EQ(CONTENT_SETTING_ASK,
      profile()->GetHostContentSettingsMap()->GetContentSetting(
          requesting_frame_0,
          requesting_frame_0,
          CONTENT_SETTINGS_TYPE_GEOLOCATION,
          std::string()));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
      profile()->GetHostContentSettingsMap()->GetContentSetting(
          requesting_frame_1,
          requesting_frame_0,
          CONTENT_SETTINGS_TYPE_GEOLOCATION,
          std::string()));
}

TEST_F(GeolocationPermissionContextTests, InvalidURL) {
  GURL invalid_embedder;
  GURL requesting_frame("about:blank");
  NavigateAndCommit(invalid_embedder);
  EXPECT_EQ(0U, infobar_tab_helper()->infobar_count());
  RequestGeolocationPermission(
      process_id(), render_id(), bridge_id(), requesting_frame);
  EXPECT_EQ(0U, infobar_tab_helper()->infobar_count());
  CheckPermissionMessageSent(bridge_id(), false);
}

TEST_F(GeolocationPermissionContextTests, SameOriginMultipleTabs) {
  GURL url_a("http://www.example.com/geolocation");
  GURL url_b("http://www.example-2.com/geolocation");
  NavigateAndCommit(url_a);
  AddNewTab(url_b);
  AddNewTab(url_a);

  EXPECT_EQ(0U, infobar_tab_helper()->infobar_count());
  RequestGeolocationPermission(
      process_id(), render_id(), bridge_id(), url_a);
  ASSERT_EQ(1U, infobar_tab_helper()->infobar_count());

  RequestGeolocationPermission(
      process_id_for_tab(0), render_id_for_tab(0), bridge_id(), url_b);
  EXPECT_EQ(1U, extra_tabs_[0]->infobar_tab_helper()->infobar_count());

  RequestGeolocationPermission(
      process_id_for_tab(1), render_id_for_tab(1), bridge_id(), url_a);
  ASSERT_EQ(1U, extra_tabs_[1]->infobar_tab_helper()->infobar_count());

  ConfirmInfoBarDelegate* removed_infobar = extra_tabs_[1]->
      infobar_tab_helper()->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();

  // Accept the first tab.
  ConfirmInfoBarDelegate* infobar_0 = infobar_tab_helper()->
      GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_0);
  infobar_0->Accept();
  CheckPermissionMessageSent(bridge_id(), true);
  infobar_tab_helper()->RemoveInfoBar(infobar_0);
  EXPECT_EQ(2U, closed_delegate_tracker_.size());
  EXPECT_TRUE(closed_delegate_tracker_.Contains(infobar_0));
  infobar_0->InfoBarClosed();
  // Now the infobar for the tab with the same origin should have gone.
  EXPECT_EQ(0U, extra_tabs_[1]->infobar_tab_helper()->infobar_count());
  CheckPermissionMessageSentForTab(1, bridge_id(), true);
  EXPECT_TRUE(closed_delegate_tracker_.Contains(removed_infobar));
  closed_delegate_tracker_.Clear();
  // Destroy the infobar that has just been removed.
  removed_infobar->InfoBarClosed();

  // But the other tab should still have the info bar...
  ASSERT_EQ(1U, extra_tabs_[0]->infobar_tab_helper()->infobar_count());
  ConfirmInfoBarDelegate* infobar_1 = extra_tabs_[0]->infobar_tab_helper()->
      GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  infobar_1->Cancel();
  extra_tabs_[0]->infobar_tab_helper()->RemoveInfoBar(infobar_1);
  EXPECT_EQ(1U, closed_delegate_tracker_.size());
  EXPECT_TRUE(closed_delegate_tracker_.Contains(infobar_1));
  infobar_1->InfoBarClosed();

  extra_tabs_.reset();
}

TEST_F(GeolocationPermissionContextTests, QueuedOriginMultipleTabs) {
  GURL url_a("http://www.example.com/geolocation");
  GURL url_b("http://www.example-2.com/geolocation");
  NavigateAndCommit(url_a);
  AddNewTab(url_a);

  EXPECT_EQ(0U, infobar_tab_helper()->infobar_count());
  RequestGeolocationPermission(
      process_id(), render_id(), bridge_id(), url_a);
  ASSERT_EQ(1U, infobar_tab_helper()->infobar_count());

  RequestGeolocationPermission(
      process_id_for_tab(0), render_id_for_tab(0), bridge_id(), url_a);
  EXPECT_EQ(1U, extra_tabs_[0]->infobar_tab_helper()->infobar_count());

  RequestGeolocationPermission(
      process_id_for_tab(0), render_id_for_tab(0), bridge_id() + 1, url_b);
  ASSERT_EQ(1U, extra_tabs_[0]->infobar_tab_helper()->infobar_count());

  ConfirmInfoBarDelegate* removed_infobar =
      infobar_tab_helper()->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();

  // Accept the second tab.
  ConfirmInfoBarDelegate* infobar_0 = extra_tabs_[0]->infobar_tab_helper()->
      GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_0);
  infobar_0->Accept();
  CheckPermissionMessageSentForTab(0, bridge_id(), true);
  extra_tabs_[0]->infobar_tab_helper()->RemoveInfoBar(infobar_0);
  EXPECT_EQ(2U, closed_delegate_tracker_.size());
  EXPECT_TRUE(closed_delegate_tracker_.Contains(infobar_0));
  infobar_0->InfoBarClosed();
  // Now the infobar for the tab with the same origin should have gone.
  EXPECT_EQ(0U, infobar_tab_helper()->infobar_count());
  CheckPermissionMessageSent(bridge_id(), true);
  EXPECT_TRUE(closed_delegate_tracker_.Contains(removed_infobar));
  closed_delegate_tracker_.Clear();
  // Destroy the infobar that has just been removed.
  removed_infobar->InfoBarClosed();

  // And we should have the queued infobar displayed now.
  ASSERT_EQ(1U, extra_tabs_[0]->infobar_tab_helper()->infobar_count());

  // Accept the second infobar.
  ConfirmInfoBarDelegate* infobar_1 = extra_tabs_[0]->infobar_tab_helper()->
      GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_1);
  infobar_1->Accept();
  CheckPermissionMessageSentForTab(0, bridge_id() + 1, true);
  extra_tabs_[0]->infobar_tab_helper()->RemoveInfoBar(infobar_1);
  EXPECT_EQ(1U, closed_delegate_tracker_.size());
  EXPECT_TRUE(closed_delegate_tracker_.Contains(infobar_1));
  infobar_1->InfoBarClosed();

  extra_tabs_.reset();
}

TEST_F(GeolocationPermissionContextTests, TabDestroyed) {
  GURL requesting_frame_0("http://www.example.com/geolocation");
  GURL requesting_frame_1("http://www.example-2.com/geolocation");
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      profile()->GetHostContentSettingsMap()->GetContentSetting(
          requesting_frame_0,
          requesting_frame_0,
          CONTENT_SETTINGS_TYPE_GEOLOCATION,
          std::string()));

  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      profile()->GetHostContentSettingsMap()->GetContentSetting(
          requesting_frame_1,
          requesting_frame_0,
          CONTENT_SETTINGS_TYPE_GEOLOCATION,
          std::string()));

  NavigateAndCommit(requesting_frame_0);
  EXPECT_EQ(0U, infobar_tab_helper()->infobar_count());
  // Request permission for two frames.
  RequestGeolocationPermission(
      process_id(), render_id(), bridge_id(), requesting_frame_0);
  RequestGeolocationPermission(
      process_id(), render_id(), bridge_id() + 1, requesting_frame_1);
  // Ensure only one infobar is created.
  ASSERT_EQ(1U, infobar_tab_helper()->infobar_count());
  ConfirmInfoBarDelegate* infobar_0 = infobar_tab_helper()->
      GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_0);

  // Delete the tab contents.
  DeleteContents();
  infobar_0->InfoBarClosed();
}

TEST_F(GeolocationPermissionContextTests, InfoBarUsesCommittedEntry) {
  GURL requesting_frame_0("http://www.example.com/geolocation");
  GURL requesting_frame_1("http://www.example-2.com/geolocation");
  NavigateAndCommit(requesting_frame_0);
  NavigateAndCommit(requesting_frame_1);
  EXPECT_EQ(0U, infobar_tab_helper()->infobar_count());
  // Go back: navigate to a pending entry before requesting geolocation
  // permission.
  contents()->controller().GoBack();
  // Request permission for the committed frame (not the pending one).
  RequestGeolocationPermission(
      process_id(), render_id(), bridge_id(), requesting_frame_1);
  // Ensure the infobar is created.
  ASSERT_EQ(1U, infobar_tab_helper()->infobar_count());
  InfoBarDelegate* infobar_0 = infobar_tab_helper()->GetInfoBarDelegateAt(0);
  ASSERT_TRUE(infobar_0);
  // Ensure the infobar is not yet expired.
  content::LoadCommittedDetails details;
  details.entry = contents()->controller().GetLastCommittedEntry();
  ASSERT_FALSE(infobar_0->ShouldExpire(details));
  // Commit the "GoBack()" above, and ensure the infobar is now expired.
  contents()->CommitPendingNavigation();
  details.entry = contents()->controller().GetLastCommittedEntry();
  ASSERT_TRUE(infobar_0->ShouldExpire(details));

  // Delete the tab contents.
  DeleteContents();
  infobar_0->InfoBarClosed();
}
