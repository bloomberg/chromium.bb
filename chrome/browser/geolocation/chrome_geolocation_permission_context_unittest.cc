// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_vector.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/geolocation/chrome_geolocation_permission_context.h"
#include "chrome/browser/geolocation/chrome_geolocation_permission_context_factory.h"
#include "chrome/browser/geolocation/geolocation_permission_request_id.h"
#include "chrome/browser/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/view_type_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "base/prefs/pref_service.h"
#include "chrome/browser/android/mock_google_location_settings_helper.h"
#include "chrome/common/pref_names.h"
#endif

using content::MockRenderProcessHost;


// ClosedDelegateTracker ------------------------------------------------------

// We need to track which infobars were closed.
class ClosedDelegateTracker : public content::NotificationObserver {
 public:
  ClosedDelegateTracker();
  virtual ~ClosedDelegateTracker();

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  size_t size() const {
    return removed_infobar_delegates_.size();
  }

  bool Contains(InfoBarDelegate* delegate) const;
  void Clear();

 private:
  FRIEND_TEST_ALL_PREFIXES(GeolocationPermissionContextTests, TabDestroyed);
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


// GeolocationPermissionContextTests ------------------------------------------

// This class sets up GeolocationArbitrator.
class GeolocationPermissionContextTests
    : public ChromeRenderViewHostTestHarness {
 public:
  GeolocationPermissionContextTests();

 protected:
  virtual ~GeolocationPermissionContextTests();

  GeolocationPermissionRequestID RequestID(int bridge_id);
  GeolocationPermissionRequestID RequestIDForTab(int tab, int bridge_id);
  InfoBarService* infobar_service() {
    return InfoBarService::FromWebContents(web_contents());
  }
  InfoBarService* infobar_service_for_tab(int tab) {
    return InfoBarService::FromWebContents(extra_tabs_[tab]);
  }

  void RequestGeolocationPermission(const GeolocationPermissionRequestID& id,
                                    const GURL& requesting_frame);
  void CancelGeolocationPermissionRequest(
      const GeolocationPermissionRequestID& id,
      const GURL& requesting_frame);
  void PermissionResponse(const GeolocationPermissionRequestID& id,
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
  ScopedVector<content::WebContents> extra_tabs_;

 private:
  // ChromeRenderViewHostTestHarness:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;

  // A map between renderer child id and a pair represending the bridge id and
  // whether the requested permission was allowed.
  base::hash_map<int, std::pair<int, bool> > responses_;
};

GeolocationPermissionContextTests::GeolocationPermissionContextTests()
    : ChromeRenderViewHostTestHarness(),
      ui_thread_(content::BrowserThread::UI, MessageLoop::current()),
      db_thread_(content::BrowserThread::DB) {
}

GeolocationPermissionContextTests::~GeolocationPermissionContextTests() {
}

GeolocationPermissionRequestID GeolocationPermissionContextTests::RequestID(
    int bridge_id) {
  return GeolocationPermissionRequestID(
      web_contents()->GetRenderProcessHost()->GetID(),
      web_contents()->GetRenderViewHost()->GetRoutingID(),
      bridge_id);
}

GeolocationPermissionRequestID
    GeolocationPermissionContextTests::RequestIDForTab(int tab, int bridge_id) {
  return GeolocationPermissionRequestID(
      extra_tabs_[tab]->GetRenderProcessHost()->GetID(),
      extra_tabs_[tab]->GetRenderViewHost()->GetRoutingID(),
      bridge_id);
}

void GeolocationPermissionContextTests::RequestGeolocationPermission(
    const GeolocationPermissionRequestID& id,
    const GURL& requesting_frame) {
  geolocation_permission_context_->RequestGeolocationPermission(
      id.render_process_id(), id.render_view_id(), id.bridge_id(),
      requesting_frame,
      base::Bind(&GeolocationPermissionContextTests::PermissionResponse,
                 base::Unretained(this), id));
}

void GeolocationPermissionContextTests::CancelGeolocationPermissionRequest(
    const GeolocationPermissionRequestID& id,
    const GURL& requesting_frame) {
  geolocation_permission_context_->CancelGeolocationPermissionRequest(
      id.render_process_id(), id.render_view_id(), id.bridge_id(),
      requesting_frame);
}

void GeolocationPermissionContextTests::PermissionResponse(
    const GeolocationPermissionRequestID& id,
    bool allowed) {
  responses_[id.render_process_id()] = std::make_pair(id.bridge_id(), allowed);
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
      extra_tabs_[tab]->GetRenderProcessHost()),
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
  content::WebContents* new_tab = content::WebContents::Create(
      content::WebContents::CreateParams(profile()));
  new_tab->GetController().LoadURL(
      url, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  content::RenderViewHostTester::For(new_tab->GetRenderViewHost())->
      SendNavigate(extra_tabs_.size() + 1, url);

  // Set up required helpers, and make this be as "tabby" as the code requires.
  extensions::SetViewType(new_tab, extensions::VIEW_TYPE_TAB_CONTENTS);
  InfoBarService::CreateForWebContents(new_tab);

  extra_tabs_.push_back(new_tab);
}

void GeolocationPermissionContextTests::CheckTabContentsState(
    const GURL& requesting_frame,
    ContentSetting expected_content_setting) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
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
  db_thread_.Start();
  ChromeRenderViewHostTestHarness::SetUp();

  // Set up required helpers, and make this be as "tabby" as the code requires.
  extensions::SetViewType(web_contents(), extensions::VIEW_TYPE_TAB_CONTENTS);
  InfoBarService::CreateForWebContents(web_contents());
  TabSpecificContentSettings::CreateForWebContents(web_contents());
#if defined(OS_ANDROID)
  MockGoogleLocationSettingsHelper::SetLocationStatus(true, true);
#endif
  geolocation_permission_context_ =
      ChromeGeolocationPermissionContextFactory::GetForProfile(profile());
}

void GeolocationPermissionContextTests::TearDown() {
  extra_tabs_.clear();
  ChromeRenderViewHostTestHarness::TearDown();
  // Schedule another task on the DB thread to notify us that it's safe to
  // carry on with the test.
  base::WaitableEvent done(false, false);
  content::BrowserThread::PostTask(
      content::BrowserThread::DB, FROM_HERE,
      base::Bind(&base::WaitableEvent::Signal, base::Unretained(&done)));
  done.Wait();
  db_thread_.Stop();
}

// Tests ----------------------------------------------------------------------

TEST_F(GeolocationPermissionContextTests, SinglePermission) {
  GURL requesting_frame("http://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  EXPECT_EQ(0U, infobar_service()->GetInfoBarCount());
  RequestGeolocationPermission(RequestID(0), requesting_frame);
  ASSERT_EQ(1U, infobar_service()->GetInfoBarCount());
  ConfirmInfoBarDelegate* infobar_0 =
      infobar_service()->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  infobar_0->Cancel();
  infobar_service()->RemoveInfoBar(infobar_0);
  EXPECT_EQ(1U, closed_delegate_tracker_.size());
  EXPECT_TRUE(closed_delegate_tracker_.Contains(infobar_0));
  delete infobar_0;
}

#if defined(OS_ANDROID)
TEST_F(GeolocationPermissionContextTests, GeolocationEnabledDisabled) {
  GURL requesting_frame("http://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  MockGoogleLocationSettingsHelper::SetLocationStatus(true, true);
  EXPECT_EQ(0U, infobar_service()->GetInfoBarCount());
  RequestGeolocationPermission(RequestID(0), requesting_frame);
  EXPECT_EQ(1U, infobar_service()->GetInfoBarCount());
  ConfirmInfoBarDelegate* infobar_0 =
      infobar_service()->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_0);
  string16 text_0 = infobar_0->GetButtonLabel(
      ConfirmInfoBarDelegate::BUTTON_OK);

  NavigateAndCommit(requesting_frame);
  MockGoogleLocationSettingsHelper::SetLocationStatus(true, false);
  EXPECT_EQ(0U, infobar_service()->GetInfoBarCount());
  RequestGeolocationPermission(RequestID(0), requesting_frame);
  EXPECT_EQ(1U, infobar_service()->GetInfoBarCount());
  ConfirmInfoBarDelegate* infobar_1 =
      infobar_service()->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_1);
  string16 text_1 = infobar_1->GetButtonLabel(
      ConfirmInfoBarDelegate::BUTTON_OK);
  EXPECT_NE(text_0, text_1);

  NavigateAndCommit(requesting_frame);
  MockGoogleLocationSettingsHelper::SetLocationStatus(false, false);
  EXPECT_EQ(0U, infobar_service()->GetInfoBarCount());
  RequestGeolocationPermission(RequestID(0), requesting_frame);
  EXPECT_EQ(0U, infobar_service()->GetInfoBarCount());
}

TEST_F(GeolocationPermissionContextTests, MasterEnabledGoogleAppsEnabled) {
  GURL requesting_frame("http://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  MockGoogleLocationSettingsHelper::SetLocationStatus(true, true);
  EXPECT_EQ(0U, infobar_service()->GetInfoBarCount());
  RequestGeolocationPermission(RequestID(0), requesting_frame);
  EXPECT_EQ(1U, infobar_service()->GetInfoBarCount());
  ConfirmInfoBarDelegate* infobar_0 =
      infobar_service()->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_0);
  infobar_0->Accept();
  CheckTabContentsState(requesting_frame, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);
}

TEST_F(GeolocationPermissionContextTests, MasterEnabledGoogleAppsDisabled) {
  GURL requesting_frame("http://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  MockGoogleLocationSettingsHelper::SetLocationStatus(true, false);
  EXPECT_EQ(0U, infobar_service()->GetInfoBarCount());
  RequestGeolocationPermission(RequestID(0), requesting_frame);
  EXPECT_EQ(1U, infobar_service()->GetInfoBarCount());
  ConfirmInfoBarDelegate* infobar_0 =
      infobar_service()->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_0);
  infobar_0->Accept();
  EXPECT_TRUE(
      MockGoogleLocationSettingsHelper::WasGoogleLocationSettingsCalled());
}
#endif

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
  EXPECT_EQ(0U, infobar_service()->GetInfoBarCount());
  // Request permission for two frames.
  RequestGeolocationPermission(RequestID(0), requesting_frame_0);
  RequestGeolocationPermission(RequestID(1), requesting_frame_1);
  // Ensure only one infobar is created.
  ASSERT_EQ(1U, infobar_service()->GetInfoBarCount());
  ConfirmInfoBarDelegate* infobar_0 =
      infobar_service()->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_0);
  string16 text_0 = infobar_0->GetMessageText();

  // Accept the first frame.
  infobar_0->Accept();
  CheckTabContentsState(requesting_frame_0, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);

  infobar_service()->RemoveInfoBar(infobar_0);
  EXPECT_EQ(1U, closed_delegate_tracker_.size());
  EXPECT_TRUE(closed_delegate_tracker_.Contains(infobar_0));
  closed_delegate_tracker_.Clear();
  delete infobar_0;
  // Now we should have a new infobar for the second frame.
  ASSERT_EQ(1U, infobar_service()->GetInfoBarCount());

  ConfirmInfoBarDelegate* infobar_1 =
      infobar_service()->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_1);
  string16 text_1 = infobar_1->GetMessageText();
  EXPECT_NE(text_0, text_1);

  // Cancel (block) this frame.
  infobar_1->Cancel();
  CheckTabContentsState(requesting_frame_1, CONTENT_SETTING_BLOCK);
  CheckPermissionMessageSent(1, false);
  infobar_service()->RemoveInfoBar(infobar_1);
  EXPECT_EQ(1U, closed_delegate_tracker_.size());
  EXPECT_TRUE(closed_delegate_tracker_.Contains(infobar_1));
  delete infobar_1;
  EXPECT_EQ(0U, infobar_service()->GetInfoBarCount());
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

TEST_F(GeolocationPermissionContextTests, PermissionForFileScheme) {
  GURL requesting_frame("file://example/geolocation.html");
  NavigateAndCommit(requesting_frame);
  EXPECT_EQ(0U, infobar_service()->GetInfoBarCount());
  RequestGeolocationPermission(RequestID(0), requesting_frame);
  EXPECT_EQ(1U, infobar_service()->GetInfoBarCount());
  ConfirmInfoBarDelegate* infobar =
      infobar_service()->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar);
  // Accept the frame
  infobar->Accept();
  CheckTabContentsState(requesting_frame, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);
  infobar_service()->RemoveInfoBar(infobar);
  delete infobar;

  // Make sure the setting is not stored.
  EXPECT_EQ(CONTENT_SETTING_ASK,
      profile()->GetHostContentSettingsMap()->GetContentSetting(
          requesting_frame,
          requesting_frame,
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
  EXPECT_EQ(0U, infobar_service()->GetInfoBarCount());
  // Request permission for two frames.
  RequestGeolocationPermission(RequestID(0), requesting_frame_0);
  RequestGeolocationPermission(RequestID(1), requesting_frame_1);
  ASSERT_EQ(1U, infobar_service()->GetInfoBarCount());

  ConfirmInfoBarDelegate* infobar_0 =
      infobar_service()->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_0);
  string16 text_0 = infobar_0->GetMessageText();

  // Simulate the frame going away, ensure the infobar for this frame
  // is removed and the next pending infobar is created.
  CancelGeolocationPermissionRequest(RequestID(0), requesting_frame_0);
  EXPECT_EQ(1U, closed_delegate_tracker_.size());
  EXPECT_TRUE(closed_delegate_tracker_.Contains(infobar_0));
  closed_delegate_tracker_.Clear();
  delete infobar_0;
  ASSERT_EQ(1U, infobar_service()->GetInfoBarCount());

  ConfirmInfoBarDelegate* infobar_1 =
      infobar_service()->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_1);
  string16 text_1 = infobar_1->GetMessageText();
  EXPECT_NE(text_0, text_1);

  // Allow this frame.
  infobar_1->Accept();
  CheckTabContentsState(requesting_frame_1, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(1, true);
  infobar_service()->RemoveInfoBar(infobar_1);
  EXPECT_EQ(1U, closed_delegate_tracker_.size());
  EXPECT_TRUE(closed_delegate_tracker_.Contains(infobar_1));
  delete infobar_1;
  EXPECT_EQ(0U, infobar_service()->GetInfoBarCount());
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
  GURL invalid_embedder("about:blank");
  GURL requesting_frame;
  NavigateAndCommit(invalid_embedder);
  EXPECT_EQ(0U, infobar_service()->GetInfoBarCount());
  RequestGeolocationPermission(RequestID(0), requesting_frame);
  EXPECT_EQ(0U, infobar_service()->GetInfoBarCount());
  CheckPermissionMessageSent(0, false);
}

TEST_F(GeolocationPermissionContextTests, SameOriginMultipleTabs) {
  GURL url_a("http://www.example.com/geolocation");
  GURL url_b("http://www.example-2.com/geolocation");
  NavigateAndCommit(url_a);
  AddNewTab(url_b);
  AddNewTab(url_a);

  EXPECT_EQ(0U, infobar_service()->GetInfoBarCount());
  RequestGeolocationPermission(RequestID(0), url_a);
  ASSERT_EQ(1U, infobar_service()->GetInfoBarCount());

  RequestGeolocationPermission(RequestIDForTab(0, 0), url_b);
  EXPECT_EQ(1U, infobar_service_for_tab(0)->GetInfoBarCount());

  RequestGeolocationPermission(RequestIDForTab(1, 0), url_a);
  ASSERT_EQ(1U, infobar_service_for_tab(1)->GetInfoBarCount());

  ConfirmInfoBarDelegate* removed_infobar = infobar_service_for_tab(1)->
      GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();

  // Accept the first tab.
  ConfirmInfoBarDelegate* infobar_0 =
      infobar_service()->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_0);
  infobar_0->Accept();
  CheckPermissionMessageSent(0, true);
  infobar_service()->RemoveInfoBar(infobar_0);
  EXPECT_EQ(2U, closed_delegate_tracker_.size());
  EXPECT_TRUE(closed_delegate_tracker_.Contains(infobar_0));
  delete infobar_0;
  // Now the infobar for the tab with the same origin should have gone.
  EXPECT_EQ(0U, infobar_service_for_tab(1)->GetInfoBarCount());
  CheckPermissionMessageSentForTab(1, 0, true);
  EXPECT_TRUE(closed_delegate_tracker_.Contains(removed_infobar));
  closed_delegate_tracker_.Clear();
  // Destroy the infobar that has just been removed.
  delete removed_infobar;

  // But the other tab should still have the info bar...
  ASSERT_EQ(1U, infobar_service_for_tab(0)->GetInfoBarCount());
  ConfirmInfoBarDelegate* infobar_1 = infobar_service_for_tab(0)->
      GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  infobar_1->Cancel();
  infobar_service_for_tab(0)->RemoveInfoBar(infobar_1);
  EXPECT_EQ(1U, closed_delegate_tracker_.size());
  EXPECT_TRUE(closed_delegate_tracker_.Contains(infobar_1));
  delete infobar_1;
}

TEST_F(GeolocationPermissionContextTests, QueuedOriginMultipleTabs) {
  GURL url_a("http://www.example.com/geolocation");
  GURL url_b("http://www.example-2.com/geolocation");
  NavigateAndCommit(url_a);
  AddNewTab(url_a);

  EXPECT_EQ(0U, infobar_service()->GetInfoBarCount());
  RequestGeolocationPermission(RequestID(0), url_a);
  ASSERT_EQ(1U, infobar_service()->GetInfoBarCount());

  RequestGeolocationPermission(RequestIDForTab(0, 0), url_a);
  EXPECT_EQ(1U, infobar_service_for_tab(0)->GetInfoBarCount());

  RequestGeolocationPermission(RequestIDForTab(0, 1), url_b);
  ASSERT_EQ(1U, infobar_service_for_tab(0)->GetInfoBarCount());

  ConfirmInfoBarDelegate* removed_infobar =
      infobar_service()->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();

  // Accept the second tab.
  ConfirmInfoBarDelegate* infobar_0 = infobar_service_for_tab(0)->
      GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_0);
  infobar_0->Accept();
  CheckPermissionMessageSentForTab(0, 0, true);
  infobar_service_for_tab(0)->RemoveInfoBar(infobar_0);
  EXPECT_EQ(2U, closed_delegate_tracker_.size());
  EXPECT_TRUE(closed_delegate_tracker_.Contains(infobar_0));
  delete infobar_0;
  // Now the infobar for the tab with the same origin should have gone.
  EXPECT_EQ(0U, infobar_service()->GetInfoBarCount());
  CheckPermissionMessageSent(0, true);
  EXPECT_TRUE(closed_delegate_tracker_.Contains(removed_infobar));
  closed_delegate_tracker_.Clear();
  // Destroy the infobar that has just been removed.
  delete removed_infobar;

  // And we should have the queued infobar displayed now.
  ASSERT_EQ(1U, infobar_service_for_tab(0)->GetInfoBarCount());

  // Accept the second infobar.
  ConfirmInfoBarDelegate* infobar_1 = infobar_service_for_tab(0)->
      GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_1);
  infobar_1->Accept();
  CheckPermissionMessageSentForTab(0, 1, true);
  infobar_service_for_tab(0)->RemoveInfoBar(infobar_1);
  EXPECT_EQ(1U, closed_delegate_tracker_.size());
  EXPECT_TRUE(closed_delegate_tracker_.Contains(infobar_1));
  delete infobar_1;
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
  EXPECT_EQ(0U, infobar_service()->GetInfoBarCount());
  // Request permission for two frames.
  RequestGeolocationPermission(RequestID(0), requesting_frame_0);
  RequestGeolocationPermission(RequestID(1), requesting_frame_1);
  // Ensure only one infobar is created.
  ASSERT_EQ(1U, infobar_service()->GetInfoBarCount());
  ConfirmInfoBarDelegate* infobar_0 =
      infobar_service()->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_0);

  // Delete the tab contents.
  DeleteContents();
  delete infobar_0;

  // During contents destruction, the infobar will have been closed, and the
  // pending request should have been cleared without an infobar being created.
  ASSERT_EQ(1U, closed_delegate_tracker_.size());
  ASSERT_TRUE(closed_delegate_tracker_.Contains(infobar_0));
}

TEST_F(GeolocationPermissionContextTests, InfoBarUsesCommittedEntry) {
  GURL requesting_frame_0("http://www.example.com/geolocation");
  GURL requesting_frame_1("http://www.example-2.com/geolocation");
  NavigateAndCommit(requesting_frame_0);
  NavigateAndCommit(requesting_frame_1);
  EXPECT_EQ(0U, infobar_service()->GetInfoBarCount());
  // Go back: navigate to a pending entry before requesting geolocation
  // permission.
  web_contents()->GetController().GoBack();
  // Request permission for the committed frame (not the pending one).
  RequestGeolocationPermission(RequestID(0), requesting_frame_1);
  // Ensure the infobar is created.
  ASSERT_EQ(1U, infobar_service()->GetInfoBarCount());
  InfoBarDelegate* infobar_0 = infobar_service()->GetInfoBarDelegateAt(0);
  ASSERT_TRUE(infobar_0);
  // Ensure the infobar wouldn't expire for a navigation to the committed entry.
  content::LoadCommittedDetails details;
  details.entry = web_contents()->GetController().GetLastCommittedEntry();
  EXPECT_FALSE(infobar_0->ShouldExpire(details));
  // Ensure the infobar will expire when we commit the pending navigation.
  details.entry = web_contents()->GetController().GetActiveEntry();
  EXPECT_TRUE(infobar_0->ShouldExpire(details));

  // Delete the tab contents.
  DeleteContents();
  delete infobar_0;
}
