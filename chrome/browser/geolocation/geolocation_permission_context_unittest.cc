// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context.h"

#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_vector.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/permission_request_id.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/geolocation/geolocation_permission_context_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
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


// ClosedInfoBarTracker -------------------------------------------------------

// We need to track which infobars were closed.
class ClosedInfoBarTracker : public content::NotificationObserver {
 public:
  ClosedInfoBarTracker();
  virtual ~ClosedInfoBarTracker();

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  size_t size() const { return removed_infobars_.size(); }

  bool Contains(infobars::InfoBar* infobar) const;
  void Clear();

 private:
  FRIEND_TEST_ALL_PREFIXES(GeolocationPermissionContextTests, TabDestroyed);
  content::NotificationRegistrar registrar_;
  std::set<infobars::InfoBar*> removed_infobars_;
};

ClosedInfoBarTracker::ClosedInfoBarTracker() {
  registrar_.Add(this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
                 content::NotificationService::AllSources());
}

ClosedInfoBarTracker::~ClosedInfoBarTracker() {
}

void ClosedInfoBarTracker::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED);
  removed_infobars_.insert(
      content::Details<infobars::InfoBar::RemovedDetails>(details)->first);
}

bool ClosedInfoBarTracker::Contains(infobars::InfoBar* infobar) const {
  return removed_infobars_.count(infobar) != 0;
}

void ClosedInfoBarTracker::Clear() {
  removed_infobars_.clear();
}


// GeolocationPermissionContextTests ------------------------------------------

class GeolocationPermissionContextTests
    : public ChromeRenderViewHostTestHarness {
 protected:
  // ChromeRenderViewHostTestHarness:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  PermissionRequestID RequestID(int bridge_id);
  PermissionRequestID RequestIDForTab(int tab, int bridge_id);
  InfoBarService* infobar_service() {
    return InfoBarService::FromWebContents(web_contents());
  }
  InfoBarService* infobar_service_for_tab(int tab) {
    return InfoBarService::FromWebContents(extra_tabs_[tab]);
  }

  void RequestGeolocationPermission(content::WebContents* web_contents,
                                    const PermissionRequestID& id,
                                    const GURL& requesting_frame);
  void RequestGeolocationPermission(content::WebContents* web_contents,
                                    const PermissionRequestID& id,
                                    const GURL& requesting_frame,
                                    base::Closure* cancel_callback);
  void PermissionResponse(const PermissionRequestID& id,
                          bool allowed);
  void CheckPermissionMessageSent(int bridge_id, bool allowed);
  void CheckPermissionMessageSentForTab(int tab, int bridge_id, bool allowed);
  void CheckPermissionMessageSentInternal(MockRenderProcessHost* process,
                                          int bridge_id,
                                          bool allowed);
  void AddNewTab(const GURL& url);
  void CheckTabContentsState(const GURL& requesting_frame,
                             ContentSetting expected_content_setting);

  scoped_refptr<GeolocationPermissionContext> geolocation_permission_context_;
  ClosedInfoBarTracker closed_infobar_tracker_;
  ScopedVector<content::WebContents> extra_tabs_;

  // A map between renderer child id and a pair represending the bridge id and
  // whether the requested permission was allowed.
  base::hash_map<int, std::pair<int, bool> > responses_;
};

PermissionRequestID GeolocationPermissionContextTests::RequestID(
    int bridge_id) {
  return PermissionRequestID(
      web_contents()->GetRenderProcessHost()->GetID(),
      web_contents()->GetRenderViewHost()->GetRoutingID(),
      bridge_id,
      GURL());
}

PermissionRequestID GeolocationPermissionContextTests::RequestIDForTab(
    int tab,
    int bridge_id) {
  return PermissionRequestID(
      extra_tabs_[tab]->GetRenderProcessHost()->GetID(),
      extra_tabs_[tab]->GetRenderViewHost()->GetRoutingID(),
      bridge_id,
      GURL());
}

void GeolocationPermissionContextTests::RequestGeolocationPermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_frame) {
  RequestGeolocationPermission(web_contents, id, requesting_frame, NULL);
}

void GeolocationPermissionContextTests::RequestGeolocationPermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    base::Closure* cancel_callback) {
  geolocation_permission_context_->RequestGeolocationPermission(
      web_contents, id.bridge_id(), requesting_frame, false,
      base::Bind(&GeolocationPermissionContextTests::PermissionResponse,
                 base::Unretained(this), id),
      cancel_callback);
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  base::RunLoop().RunUntilIdle();
}

void GeolocationPermissionContextTests::PermissionResponse(
    const PermissionRequestID& id,
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
  const ContentSettingsUsagesState::StateMap& state_map =
      content_settings->geolocation_usages_state().state_map();
  EXPECT_EQ(1U, state_map.count(requesting_frame.GetOrigin()));
  EXPECT_EQ(0U, state_map.count(requesting_frame));
  ContentSettingsUsagesState::StateMap::const_iterator settings =
      state_map.find(requesting_frame.GetOrigin());
  ASSERT_FALSE(settings == state_map.end())
      << "geolocation state not found " << requesting_frame;
  EXPECT_EQ(expected_content_setting, settings->second);
}

void GeolocationPermissionContextTests::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  // Set up required helpers, and make this be as "tabby" as the code requires.
  extensions::SetViewType(web_contents(), extensions::VIEW_TYPE_TAB_CONTENTS);
  InfoBarService::CreateForWebContents(web_contents());
  TabSpecificContentSettings::CreateForWebContents(web_contents());
#if defined(OS_ANDROID)
  MockGoogleLocationSettingsHelper::SetLocationStatus(true, true);
#endif
  geolocation_permission_context_ =
      GeolocationPermissionContextFactory::GetForProfile(profile());
}

void GeolocationPermissionContextTests::TearDown() {
  extra_tabs_.clear();
  ChromeRenderViewHostTestHarness::TearDown();
}

// Tests ----------------------------------------------------------------------

TEST_F(GeolocationPermissionContextTests, SinglePermission) {
  GURL requesting_frame("http://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  RequestGeolocationPermission(web_contents(), RequestID(0), requesting_frame);
  ASSERT_EQ(1U, infobar_service()->infobar_count());
  infobars::InfoBar* infobar = infobar_service()->infobar_at(0);
  ConfirmInfoBarDelegate* infobar_delegate =
      infobar->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_delegate);
  infobar_delegate->Cancel();
  infobar_service()->RemoveInfoBar(infobar);
  EXPECT_EQ(1U, closed_infobar_tracker_.size());
  EXPECT_TRUE(closed_infobar_tracker_.Contains(infobar));
}

#if defined(OS_ANDROID)
TEST_F(GeolocationPermissionContextTests, GeolocationEnabledDisabled) {
  GURL requesting_frame("http://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  MockGoogleLocationSettingsHelper::SetLocationStatus(true, true);
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  RequestGeolocationPermission(web_contents(), RequestID(0), requesting_frame);
  EXPECT_EQ(1U, infobar_service()->infobar_count());
  ConfirmInfoBarDelegate* infobar_delegate_0 =
      infobar_service()->infobar_at(0)->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_delegate_0);
  base::string16 text_0 = infobar_delegate_0->GetButtonLabel(
      ConfirmInfoBarDelegate::BUTTON_OK);

  Reload();
  MockGoogleLocationSettingsHelper::SetLocationStatus(true, false);
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  RequestGeolocationPermission(web_contents(), RequestID(0), requesting_frame);
  EXPECT_EQ(1U, infobar_service()->infobar_count());
  ConfirmInfoBarDelegate* infobar_delegate_1 =
      infobar_service()->infobar_at(0)->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_delegate_1);
  base::string16 text_1 = infobar_delegate_1->GetButtonLabel(
      ConfirmInfoBarDelegate::BUTTON_OK);
  EXPECT_NE(text_0, text_1);

  Reload();
  MockGoogleLocationSettingsHelper::SetLocationStatus(false, false);
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  RequestGeolocationPermission(web_contents(), RequestID(0), requesting_frame);
  EXPECT_EQ(0U, infobar_service()->infobar_count());
}

TEST_F(GeolocationPermissionContextTests, MasterEnabledGoogleAppsEnabled) {
  GURL requesting_frame("http://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  MockGoogleLocationSettingsHelper::SetLocationStatus(true, true);
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  RequestGeolocationPermission(web_contents(), RequestID(0), requesting_frame);
  EXPECT_EQ(1U, infobar_service()->infobar_count());
  ConfirmInfoBarDelegate* infobar_delegate =
      infobar_service()->infobar_at(0)->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_delegate);
  infobar_delegate->Accept();
  CheckTabContentsState(requesting_frame, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);
}

TEST_F(GeolocationPermissionContextTests, MasterEnabledGoogleAppsDisabled) {
  GURL requesting_frame("http://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  MockGoogleLocationSettingsHelper::SetLocationStatus(true, false);
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  RequestGeolocationPermission(web_contents(), RequestID(0), requesting_frame);
  EXPECT_EQ(1U, infobar_service()->infobar_count());
  ConfirmInfoBarDelegate* infobar_delegate =
      infobar_service()->infobar_at(0)->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_delegate);
  infobar_delegate->Accept();
  EXPECT_TRUE(
      MockGoogleLocationSettingsHelper::WasGoogleLocationSettingsCalled());
}
#endif

TEST_F(GeolocationPermissionContextTests, QueuedPermission) {
  GURL requesting_frame_0("http://www.example.com/geolocation");
  GURL requesting_frame_1("http://www.example-2.com/geolocation");
  EXPECT_EQ(CONTENT_SETTING_ASK,
            profile()->GetHostContentSettingsMap()->GetContentSetting(
                requesting_frame_0, requesting_frame_0,
                CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            profile()->GetHostContentSettingsMap()->GetContentSetting(
                requesting_frame_1, requesting_frame_0,
                CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string()));

  NavigateAndCommit(requesting_frame_0);
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  // Request permission for two frames.
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame_0);
  RequestGeolocationPermission(
      web_contents(), RequestID(1), requesting_frame_1);
  // Ensure only one infobar is created.
  ASSERT_EQ(1U, infobar_service()->infobar_count());
  infobars::InfoBar* infobar_0 = infobar_service()->infobar_at(0);
  ConfirmInfoBarDelegate* infobar_delegate_0 =
      infobar_0->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_delegate_0);
  base::string16 text_0 = infobar_delegate_0->GetMessageText();

  // Accept the first frame.
  infobar_delegate_0->Accept();
  CheckTabContentsState(requesting_frame_0, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);

  infobar_service()->RemoveInfoBar(infobar_0);
  EXPECT_EQ(1U, closed_infobar_tracker_.size());
  EXPECT_TRUE(closed_infobar_tracker_.Contains(infobar_0));
  closed_infobar_tracker_.Clear();
  // Now we should have a new infobar for the second frame.
  ASSERT_EQ(1U, infobar_service()->infobar_count());

  infobars::InfoBar* infobar_1 = infobar_service()->infobar_at(0);
  ConfirmInfoBarDelegate* infobar_delegate_1 =
      infobar_1->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_delegate_1);
  base::string16 text_1 = infobar_delegate_1->GetMessageText();
  EXPECT_NE(text_0, text_1);

  // Cancel (block) this frame.
  infobar_delegate_1->Cancel();
  CheckTabContentsState(requesting_frame_1, CONTENT_SETTING_BLOCK);
  CheckPermissionMessageSent(1, false);
  infobar_service()->RemoveInfoBar(infobar_1);
  EXPECT_EQ(1U, closed_infobar_tracker_.size());
  EXPECT_TRUE(closed_infobar_tracker_.Contains(infobar_1));
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  // Ensure the persisted permissions are ok.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            profile()->GetHostContentSettingsMap()->GetContentSetting(
                requesting_frame_0, requesting_frame_0,
                CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string()));

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            profile()->GetHostContentSettingsMap()->GetContentSetting(
                requesting_frame_1, requesting_frame_0,
                CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string()));
}

TEST_F(GeolocationPermissionContextTests, HashIsIgnored) {
  GURL url_a("http://www.example.com/geolocation#a");
  GURL url_b("http://www.example.com/geolocation#b");

  // Navigate to the first url and check permission is requested.
  NavigateAndCommit(url_a);
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  RequestGeolocationPermission(web_contents(), RequestID(0), url_a);
  ASSERT_EQ(1U, infobar_service()->infobar_count());
  infobars::InfoBar* infobar = infobar_service()->infobar_at(0);
  ConfirmInfoBarDelegate* infobar_delegate =
      infobar->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_delegate);

  // Change the hash, we'll still be on the same page.
  NavigateAndCommit(url_b);

  // Accept.
  infobar_delegate->Accept();
  CheckTabContentsState(url_a, CONTENT_SETTING_ALLOW);
  CheckTabContentsState(url_b, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);

  // Cleanup.
  infobar_service()->RemoveInfoBar(infobar);
  EXPECT_EQ(1U, closed_infobar_tracker_.size());
  EXPECT_TRUE(closed_infobar_tracker_.Contains(infobar));
}

TEST_F(GeolocationPermissionContextTests, PermissionForFileScheme) {
  GURL requesting_frame("file://example/geolocation.html");
  NavigateAndCommit(requesting_frame);
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  RequestGeolocationPermission(web_contents(), RequestID(0), requesting_frame);
  EXPECT_EQ(1U, infobar_service()->infobar_count());
  infobars::InfoBar* infobar = infobar_service()->infobar_at(0);
  ConfirmInfoBarDelegate* infobar_delegate =
      infobar->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_delegate);
  // Accept the frame.
  infobar_delegate->Accept();
  CheckTabContentsState(requesting_frame, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);
  infobar_service()->RemoveInfoBar(infobar);

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
                requesting_frame_0, requesting_frame_0,
                CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string()));

  EXPECT_EQ(CONTENT_SETTING_ASK,
            profile()->GetHostContentSettingsMap()->GetContentSetting(
                requesting_frame_1, requesting_frame_0,
                CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string()));

  NavigateAndCommit(requesting_frame_0);
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  // Request permission for two frames.
  base::Closure cancel_callback;
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame_0, &cancel_callback);
  RequestGeolocationPermission(
      web_contents(), RequestID(1), requesting_frame_1);
  ASSERT_EQ(1U, infobar_service()->infobar_count());

  infobars::InfoBar* infobar_0 = infobar_service()->infobar_at(0);
  ConfirmInfoBarDelegate* infobar_delegate_0 =
      infobar_0->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_delegate_0);
  base::string16 text_0 = infobar_delegate_0->GetMessageText();

  // Simulate the frame going away, ensure the infobar for this frame
  // is removed and the next pending infobar is created.
  cancel_callback.Run();
  EXPECT_EQ(1U, closed_infobar_tracker_.size());
  EXPECT_TRUE(closed_infobar_tracker_.Contains(infobar_0));
  closed_infobar_tracker_.Clear();
  ASSERT_EQ(1U, infobar_service()->infobar_count());

  infobars::InfoBar* infobar_1 = infobar_service()->infobar_at(0);
  ConfirmInfoBarDelegate* infobar_delegate_1 =
      infobar_1->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_delegate_1);
  base::string16 text_1 = infobar_delegate_1->GetMessageText();
  EXPECT_NE(text_0, text_1);

  // Allow this frame.
  infobar_delegate_1->Accept();
  CheckTabContentsState(requesting_frame_1, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(1, true);
  infobar_service()->RemoveInfoBar(infobar_1);
  EXPECT_EQ(1U, closed_infobar_tracker_.size());
  EXPECT_TRUE(closed_infobar_tracker_.Contains(infobar_1));
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  // Ensure the persisted permissions are ok.
  EXPECT_EQ(CONTENT_SETTING_ASK,
            profile()->GetHostContentSettingsMap()->GetContentSetting(
                requesting_frame_0, requesting_frame_0,
                CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string()));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            profile()->GetHostContentSettingsMap()->GetContentSetting(
                requesting_frame_1, requesting_frame_0,
                CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string()));
}

TEST_F(GeolocationPermissionContextTests, InvalidURL) {
  GURL invalid_embedder("about:blank");
  GURL requesting_frame;
  NavigateAndCommit(invalid_embedder);
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  RequestGeolocationPermission(web_contents(), RequestID(0), requesting_frame);
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  CheckPermissionMessageSent(0, false);
}

TEST_F(GeolocationPermissionContextTests, SameOriginMultipleTabs) {
  GURL url_a("http://www.example.com/geolocation");
  GURL url_b("http://www.example-2.com/geolocation");
  NavigateAndCommit(url_a);
  AddNewTab(url_b);
  AddNewTab(url_a);

  EXPECT_EQ(0U, infobar_service()->infobar_count());
  RequestGeolocationPermission(web_contents(), RequestID(0), url_a);
  ASSERT_EQ(1U, infobar_service()->infobar_count());

  RequestGeolocationPermission(extra_tabs_[0], RequestIDForTab(0, 0), url_b);
  EXPECT_EQ(1U, infobar_service_for_tab(0)->infobar_count());

  RequestGeolocationPermission(extra_tabs_[1], RequestIDForTab(1, 0), url_a);
  ASSERT_EQ(1U, infobar_service_for_tab(1)->infobar_count());

  infobars::InfoBar* removed_infobar =
      infobar_service_for_tab(1)->infobar_at(0);

  // Accept the first tab.
  infobars::InfoBar* infobar_0 = infobar_service()->infobar_at(0);
  ConfirmInfoBarDelegate* infobar_delegate_0 =
      infobar_0->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_delegate_0);
  infobar_delegate_0->Accept();
  CheckPermissionMessageSent(0, true);
  infobar_service()->RemoveInfoBar(infobar_0);
  EXPECT_EQ(2U, closed_infobar_tracker_.size());
  EXPECT_TRUE(closed_infobar_tracker_.Contains(infobar_0));
  // Now the infobar for the tab with the same origin should have gone.
  EXPECT_EQ(0U, infobar_service_for_tab(1)->infobar_count());
  CheckPermissionMessageSentForTab(1, 0, true);
  EXPECT_TRUE(closed_infobar_tracker_.Contains(removed_infobar));
  closed_infobar_tracker_.Clear();

  // But the other tab should still have the info bar...
  ASSERT_EQ(1U, infobar_service_for_tab(0)->infobar_count());
  infobars::InfoBar* infobar_1 = infobar_service_for_tab(0)->infobar_at(0);
  ConfirmInfoBarDelegate* infobar_delegate_1 =
      infobar_1->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_delegate_1);
  infobar_delegate_1->Cancel();
  infobar_service_for_tab(0)->RemoveInfoBar(infobar_1);
  EXPECT_EQ(1U, closed_infobar_tracker_.size());
  EXPECT_TRUE(closed_infobar_tracker_.Contains(infobar_1));
}

TEST_F(GeolocationPermissionContextTests, QueuedOriginMultipleTabs) {
  GURL url_a("http://www.example.com/geolocation");
  GURL url_b("http://www.example-2.com/geolocation");
  NavigateAndCommit(url_a);
  AddNewTab(url_a);

  EXPECT_EQ(0U, infobar_service()->infobar_count());
  RequestGeolocationPermission(web_contents(), RequestID(0), url_a);
  ASSERT_EQ(1U, infobar_service()->infobar_count());

  RequestGeolocationPermission(extra_tabs_[0], RequestIDForTab(0, 0), url_a);
  EXPECT_EQ(1U, infobar_service_for_tab(0)->infobar_count());

  RequestGeolocationPermission(extra_tabs_[0], RequestIDForTab(0, 1), url_b);
  ASSERT_EQ(1U, infobar_service_for_tab(0)->infobar_count());

  infobars::InfoBar* removed_infobar = infobar_service()->infobar_at(0);

  // Accept the second tab.
  infobars::InfoBar* infobar_0 = infobar_service_for_tab(0)->infobar_at(0);
  ConfirmInfoBarDelegate* infobar_delegate_0 =
      infobar_0->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_delegate_0);
  infobar_delegate_0->Accept();
  CheckPermissionMessageSentForTab(0, 0, true);
  infobar_service_for_tab(0)->RemoveInfoBar(infobar_0);
  EXPECT_EQ(2U, closed_infobar_tracker_.size());
  EXPECT_TRUE(closed_infobar_tracker_.Contains(infobar_0));
  // Now the infobar for the tab with the same origin should have gone.
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  CheckPermissionMessageSent(0, true);
  EXPECT_TRUE(closed_infobar_tracker_.Contains(removed_infobar));
  closed_infobar_tracker_.Clear();

  // And we should have the queued infobar displayed now.
  ASSERT_EQ(1U, infobar_service_for_tab(0)->infobar_count());

  // Accept the second infobar.
  infobars::InfoBar* infobar_1 = infobar_service_for_tab(0)->infobar_at(0);
  ConfirmInfoBarDelegate* infobar_delegate_1 =
      infobar_1->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_delegate_1);
  infobar_delegate_1->Accept();
  CheckPermissionMessageSentForTab(0, 1, true);
  infobar_service_for_tab(0)->RemoveInfoBar(infobar_1);
  EXPECT_EQ(1U, closed_infobar_tracker_.size());
  EXPECT_TRUE(closed_infobar_tracker_.Contains(infobar_1));
}

TEST_F(GeolocationPermissionContextTests, TabDestroyed) {
  GURL requesting_frame_0("http://www.example.com/geolocation");
  GURL requesting_frame_1("http://www.example-2.com/geolocation");
  EXPECT_EQ(CONTENT_SETTING_ASK,
            profile()->GetHostContentSettingsMap()->GetContentSetting(
                requesting_frame_0, requesting_frame_0,
                CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string()));

  EXPECT_EQ(CONTENT_SETTING_ASK,
            profile()->GetHostContentSettingsMap()->GetContentSetting(
                requesting_frame_1, requesting_frame_0,
                CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string()));

  NavigateAndCommit(requesting_frame_0);
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  // Request permission for two frames.
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame_0);
  RequestGeolocationPermission(
      web_contents(), RequestID(1), requesting_frame_1);
  // Ensure only one infobar is created.
  ASSERT_EQ(1U, infobar_service()->infobar_count());
  infobars::InfoBar* infobar = infobar_service()->infobar_at(0);

  // Delete the tab contents.
  DeleteContents();

  // During contents destruction, the infobar will have been closed, and the
  // pending request should have been cleared without an infobar being created.
  ASSERT_EQ(1U, closed_infobar_tracker_.size());
  ASSERT_TRUE(closed_infobar_tracker_.Contains(infobar));
}

TEST_F(GeolocationPermissionContextTests, InfoBarUsesCommittedEntry) {
  GURL requesting_frame_0("http://www.example.com/geolocation");
  GURL requesting_frame_1("http://www.example-2.com/geolocation");
  NavigateAndCommit(requesting_frame_0);
  NavigateAndCommit(requesting_frame_1);
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  // Go back: navigate to a pending entry before requesting geolocation
  // permission.
  web_contents()->GetController().GoBack();
  // Request permission for the committed frame (not the pending one).
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame_1);
  // Ensure the infobar is created.
  ASSERT_EQ(1U, infobar_service()->infobar_count());
  infobars::InfoBarDelegate* infobar_delegate =
      infobar_service()->infobar_at(0)->delegate();
  ASSERT_TRUE(infobar_delegate);
  // Ensure the infobar wouldn't expire for a navigation to the committed entry.
  content::LoadCommittedDetails details;
  details.entry = web_contents()->GetController().GetLastCommittedEntry();
  EXPECT_FALSE(infobar_delegate->ShouldExpire(
      InfoBarService::NavigationDetailsFromLoadCommittedDetails(details)));
  // Ensure the infobar will expire when we commit the pending navigation.
  details.entry = web_contents()->GetController().GetActiveEntry();
  EXPECT_TRUE(infobar_delegate->ShouldExpire(
      InfoBarService::NavigationDetailsFromLoadCommittedDetails(details)));

  // Delete the tab contents.
  DeleteContents();
}

TEST_F(GeolocationPermissionContextTests, LastUsageAudited) {
  GURL requesting_frame("http://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);

  base::SimpleTestClock* test_clock = new base::SimpleTestClock;
  test_clock->SetNow(base::Time::UnixEpoch() +
                     base::TimeDelta::FromSeconds(10));

  HostContentSettingsMap* map = profile()->GetHostContentSettingsMap();
  map->SetPrefClockForTesting(scoped_ptr<base::Clock>(test_clock));

  // The permission shouldn't have been used yet.
  EXPECT_EQ(map->GetLastUsage(requesting_frame.GetOrigin(),
                              requesting_frame.GetOrigin(),
                              CONTENT_SETTINGS_TYPE_GEOLOCATION).ToDoubleT(),
            0);

  EXPECT_EQ(0U, infobar_service()->infobar_count());
  RequestGeolocationPermission(web_contents(), RequestID(0), requesting_frame);
  ASSERT_EQ(1U, infobar_service()->infobar_count());
  infobars::InfoBar* infobar = infobar_service()->infobar_at(0);
  ConfirmInfoBarDelegate* infobar_delegate =
      infobar->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_delegate);
  infobar_delegate->Accept();
  CheckTabContentsState(requesting_frame, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);

  // Permission has been used at the starting time.
  EXPECT_EQ(map->GetLastUsage(requesting_frame.GetOrigin(),
                              requesting_frame.GetOrigin(),
                              CONTENT_SETTINGS_TYPE_GEOLOCATION).ToDoubleT(),
            10);

  test_clock->Advance(base::TimeDelta::FromSeconds(3));
  RequestGeolocationPermission(web_contents(), RequestID(0), requesting_frame);

  // Permission has been used three seconds later.
  EXPECT_EQ(map->GetLastUsage(requesting_frame.GetOrigin(),
                              requesting_frame.GetOrigin(),
                              CONTENT_SETTINGS_TYPE_GEOLOCATION).ToDoubleT(),
            13);
}

TEST_F(GeolocationPermissionContextTests, LastUsageAuditedMultipleFrames) {
  base::SimpleTestClock* test_clock = new base::SimpleTestClock;
  test_clock->SetNow(base::Time::UnixEpoch() +
                     base::TimeDelta::FromSeconds(10));

  HostContentSettingsMap* map = profile()->GetHostContentSettingsMap();
  map->SetPrefClockForTesting(scoped_ptr<base::Clock>(test_clock));

  GURL requesting_frame_0("http://www.example.com/geolocation");
  GURL requesting_frame_1("http://www.example-2.com/geolocation");

  // The permission shouldn't have been used yet.
  EXPECT_EQ(map->GetLastUsage(requesting_frame_0.GetOrigin(),
                              requesting_frame_0.GetOrigin(),
                              CONTENT_SETTINGS_TYPE_GEOLOCATION).ToDoubleT(),
            0);
  EXPECT_EQ(map->GetLastUsage(requesting_frame_1.GetOrigin(),
                              requesting_frame_0.GetOrigin(),
                              CONTENT_SETTINGS_TYPE_GEOLOCATION).ToDoubleT(),
            0);

  NavigateAndCommit(requesting_frame_0);
  EXPECT_EQ(0U, infobar_service()->infobar_count());

  // Request permission for two frames.
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame_0);
  RequestGeolocationPermission(
      web_contents(), RequestID(1), requesting_frame_1);

  // Ensure only one infobar is created.
  ASSERT_EQ(1U, infobar_service()->infobar_count());
  infobars::InfoBar* infobar_0 = infobar_service()->infobar_at(0);
  ConfirmInfoBarDelegate* infobar_delegate_0 =
      infobar_0->delegate()->AsConfirmInfoBarDelegate();

  // Accept the first frame.
  infobar_delegate_0->Accept();
  CheckTabContentsState(requesting_frame_0, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);
  infobar_service()->RemoveInfoBar(infobar_0);

  // Verify that accepting the first didn't accept because it's embedder
  // in the other.
  EXPECT_EQ(map->GetLastUsage(requesting_frame_0.GetOrigin(),
                              requesting_frame_0.GetOrigin(),
                              CONTENT_SETTINGS_TYPE_GEOLOCATION).ToDoubleT(),
            10);
  EXPECT_EQ(map->GetLastUsage(requesting_frame_1.GetOrigin(),
                              requesting_frame_0.GetOrigin(),
                              CONTENT_SETTINGS_TYPE_GEOLOCATION).ToDoubleT(),
            0);

  ASSERT_EQ(1U, infobar_service()->infobar_count());
  infobars::InfoBar* infobar_1 = infobar_service()->infobar_at(0);
  ConfirmInfoBarDelegate* infobar_delegate_1 =
      infobar_1->delegate()->AsConfirmInfoBarDelegate();

  test_clock->Advance(base::TimeDelta::FromSeconds(1));

  // Allow the second frame.
  infobar_delegate_1->Accept();
  CheckTabContentsState(requesting_frame_1, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(1, true);
  infobar_service()->RemoveInfoBar(infobar_1);

  // Verify that the times are different.
  EXPECT_EQ(map->GetLastUsage(requesting_frame_0.GetOrigin(),
                              requesting_frame_0.GetOrigin(),
                              CONTENT_SETTINGS_TYPE_GEOLOCATION).ToDoubleT(),
            10);
  EXPECT_EQ(map->GetLastUsage(requesting_frame_1.GetOrigin(),
                              requesting_frame_0.GetOrigin(),
                              CONTENT_SETTINGS_TYPE_GEOLOCATION).ToDoubleT(),
            11);

  test_clock->Advance(base::TimeDelta::FromSeconds(2));
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame_0);

  // Verify that requesting permission in one frame doesn't update other where
  // it is the embedder.
  EXPECT_EQ(map->GetLastUsage(requesting_frame_0.GetOrigin(),
                              requesting_frame_0.GetOrigin(),
                              CONTENT_SETTINGS_TYPE_GEOLOCATION).ToDoubleT(),
            13);
  EXPECT_EQ(map->GetLastUsage(requesting_frame_1.GetOrigin(),
                              requesting_frame_0.GetOrigin(),
                              CONTENT_SETTINGS_TYPE_GEOLOCATION).ToDoubleT(),
            11);
}
