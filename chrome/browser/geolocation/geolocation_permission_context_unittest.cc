// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context.h"

#include <stddef.h>

#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/id_map.h"
#include "base/memory/scoped_vector.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/geolocation/geolocation_permission_context_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/android/mock_location_settings.h"
#include "chrome/browser/geolocation/geolocation_permission_context_android.h"
#include "components/prefs/pref_service.h"
#else
#include "chrome/browser/ui/website_settings/mock_permission_bubble_view.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "extensions/browser/view_type_utils.h"
#endif

using content::MockRenderProcessHost;


// ClosedInfoBarTracker -------------------------------------------------------

// We need to track which infobars were closed.
class ClosedInfoBarTracker : public content::NotificationObserver {
 public:
  ClosedInfoBarTracker();
  ~ClosedInfoBarTracker() override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

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
  void SetUp() override;
  void TearDown() override;

  PermissionRequestID RequestID(int request_id);
  PermissionRequestID RequestIDForTab(int tab, int request_id);
  InfoBarService* infobar_service() {
    return InfoBarService::FromWebContents(web_contents());
  }
  InfoBarService* infobar_service_for_tab(int tab) {
    return InfoBarService::FromWebContents(extra_tabs_[tab]);
  }

  void RequestGeolocationPermission(content::WebContents* web_contents,
                                    const PermissionRequestID& id,
                                    const GURL& requesting_frame,
                                    bool user_gesture);

  void PermissionResponse(const PermissionRequestID& id,
                          ContentSetting content_setting);
  void CheckPermissionMessageSent(int request_id, bool allowed);
  void CheckPermissionMessageSentForTab(int tab, int request_id, bool allowed);
  void CheckPermissionMessageSentInternal(MockRenderProcessHost* process,
                                          int request_id,
                                          bool allowed);
  void AddNewTab(const GURL& url);
  void CheckTabContentsState(const GURL& requesting_frame,
                             ContentSetting expected_content_setting);
#if !BUILDFLAG(ANDROID_JAVA_UI)
  size_t GetBubblesQueueSize(PermissionBubbleManager* manager);
  void AcceptBubble(PermissionBubbleManager* manager);
  void DenyBubble(PermissionBubbleManager* manager);
  void CloseBubble(PermissionBubbleManager* manager);
#endif
  void BubbleManagerDocumentLoadCompleted();
  void BubbleManagerDocumentLoadCompleted(content::WebContents* web_contents);
  ContentSetting GetGeolocationContentSetting(GURL frame_0, GURL frame_1);
  size_t GetNumberOfPrompts();
  void AcceptPrompt();
  base::string16 GetPromptText();

  // owned by the browser context
  GeolocationPermissionContext* geolocation_permission_context_;
  ClosedInfoBarTracker closed_infobar_tracker_;
  ScopedVector<content::WebContents> extra_tabs_;

  // A map between renderer child id and a pair represending the bridge id and
  // whether the requested permission was allowed.
  base::hash_map<int, std::pair<int, bool> > responses_;
};

PermissionRequestID GeolocationPermissionContextTests::RequestID(
    int request_id) {
  return PermissionRequestID(
      web_contents()->GetRenderProcessHost()->GetID(),
      web_contents()->GetMainFrame()->GetRoutingID(),
      request_id);
}

PermissionRequestID GeolocationPermissionContextTests::RequestIDForTab(
    int tab,
    int request_id) {
  return PermissionRequestID(
      extra_tabs_[tab]->GetRenderProcessHost()->GetID(),
      extra_tabs_[tab]->GetMainFrame()->GetRoutingID(),
      request_id);
}

void GeolocationPermissionContextTests::RequestGeolocationPermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    bool user_gesture) {
  geolocation_permission_context_->RequestPermission(
      web_contents, id, requesting_frame, user_gesture,
      base::Bind(&GeolocationPermissionContextTests::PermissionResponse,
                 base::Unretained(this), id));
  content::RunAllBlockingPoolTasksUntilIdle();
}

void GeolocationPermissionContextTests::PermissionResponse(
    const PermissionRequestID& id,
    ContentSetting content_setting) {
  responses_[id.render_process_id()] =
      std::make_pair(id.request_id(), content_setting == CONTENT_SETTING_ALLOW);
}

void GeolocationPermissionContextTests::CheckPermissionMessageSent(
    int request_id,
    bool allowed) {
  CheckPermissionMessageSentInternal(process(), request_id, allowed);
}

void GeolocationPermissionContextTests::CheckPermissionMessageSentForTab(
    int tab,
    int request_id,
    bool allowed) {
  CheckPermissionMessageSentInternal(static_cast<MockRenderProcessHost*>(
      extra_tabs_[tab]->GetRenderProcessHost()),
      request_id, allowed);
}

void GeolocationPermissionContextTests::CheckPermissionMessageSentInternal(
    MockRenderProcessHost* process,
    int request_id,
    bool allowed) {
  ASSERT_EQ(responses_.count(process->GetID()), 1U);
  EXPECT_EQ(request_id, responses_[process->GetID()].first);
  EXPECT_EQ(allowed, responses_[process->GetID()].second);
  responses_.erase(process->GetID());
}

void GeolocationPermissionContextTests::AddNewTab(const GURL& url) {
  content::WebContents* new_tab = CreateTestWebContents();
  new_tab->GetController().LoadURL(
      url, content::Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  content::NavigationEntry* entry = new_tab->GetController().GetPendingEntry();
  content::RenderFrameHostTester::For(new_tab->GetMainFrame())
      ->SendNavigate(extra_tabs_.size() + 1, entry->GetUniqueID(), true, url);

  // Set up required helpers, and make this be as "tabby" as the code requires.
#if defined(ENABLE_EXTENSIONS)
  extensions::SetViewType(new_tab, extensions::VIEW_TYPE_TAB_CONTENTS);
#endif
#if BUILDFLAG(ANDROID_JAVA_UI)
  InfoBarService::CreateForWebContents(new_tab);
#else
  PermissionBubbleManager::CreateForWebContents(new_tab);
  PermissionBubbleManager* permission_bubble_manager =
      PermissionBubbleManager::FromWebContents(new_tab);
  MockPermissionBubbleView::SetFactory(permission_bubble_manager, false);
  permission_bubble_manager->DisplayPendingRequests();
#endif

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
#if defined(ENABLE_EXTENSIONS)
  extensions::SetViewType(web_contents(), extensions::VIEW_TYPE_TAB_CONTENTS);
#endif
  InfoBarService::CreateForWebContents(web_contents());
  TabSpecificContentSettings::CreateForWebContents(web_contents());
  geolocation_permission_context_ =
      GeolocationPermissionContextFactory::GetForProfile(profile());
#if BUILDFLAG(ANDROID_JAVA_UI)
  static_cast<GeolocationPermissionContextAndroid*>(
      geolocation_permission_context_)
      ->SetLocationSettingsForTesting(
          scoped_ptr<LocationSettings>(new MockLocationSettings()));
  MockLocationSettings::SetLocationStatus(true, true);
#else
  PermissionBubbleManager::CreateForWebContents(web_contents());
  PermissionBubbleManager* permission_bubble_manager =
      PermissionBubbleManager::FromWebContents(web_contents());
  MockPermissionBubbleView::SetFactory(permission_bubble_manager, false);
  permission_bubble_manager->DisplayPendingRequests();
#endif
}

void GeolocationPermissionContextTests::TearDown() {
  extra_tabs_.clear();
  ChromeRenderViewHostTestHarness::TearDown();
}

#if !BUILDFLAG(ANDROID_JAVA_UI)
size_t GeolocationPermissionContextTests::GetBubblesQueueSize(
    PermissionBubbleManager* manager) {
  return manager->requests_.size();
}

void GeolocationPermissionContextTests::AcceptBubble(
    PermissionBubbleManager* manager) {
  manager->Accept();
}

void GeolocationPermissionContextTests::DenyBubble(
    PermissionBubbleManager* manager) {
  manager->Deny();
}

void GeolocationPermissionContextTests::CloseBubble(
    PermissionBubbleManager* manager) {
  manager->Closing();
}
#endif

void GeolocationPermissionContextTests::BubbleManagerDocumentLoadCompleted() {
  GeolocationPermissionContextTests::BubbleManagerDocumentLoadCompleted(
      web_contents());
}

void GeolocationPermissionContextTests::BubbleManagerDocumentLoadCompleted(
    content::WebContents* web_contents) {
#if !BUILDFLAG(ANDROID_JAVA_UI)
  PermissionBubbleManager::FromWebContents(web_contents)->
      DocumentOnLoadCompletedInMainFrame();
#endif
}

ContentSetting GeolocationPermissionContextTests::GetGeolocationContentSetting(
    GURL frame_0, GURL frame_1) {
  return HostContentSettingsMapFactory::GetForProfile(profile())
      ->GetContentSetting(frame_0,
                          frame_1,
                          CONTENT_SETTINGS_TYPE_GEOLOCATION,
                          std::string());
}

size_t GeolocationPermissionContextTests::GetNumberOfPrompts() {
#if !BUILDFLAG(ANDROID_JAVA_UI)
  PermissionBubbleManager* manager =
      PermissionBubbleManager::FromWebContents(web_contents());
  return GetBubblesQueueSize(manager);
#else
  return infobar_service()->infobar_count();
#endif
}

void GeolocationPermissionContextTests::AcceptPrompt() {
#if !BUILDFLAG(ANDROID_JAVA_UI)
  PermissionBubbleManager* manager =
      PermissionBubbleManager::FromWebContents(web_contents());
  AcceptBubble(manager);
#else
  infobars::InfoBar* infobar = infobar_service()->infobar_at(0);
  ConfirmInfoBarDelegate* infobar_delegate =
      infobar->delegate()->AsConfirmInfoBarDelegate();
  infobar_delegate->Accept();
#endif
}

base::string16 GeolocationPermissionContextTests::GetPromptText() {
#if !BUILDFLAG(ANDROID_JAVA_UI)
  PermissionBubbleManager* manager =
      PermissionBubbleManager::FromWebContents(web_contents());
  return manager->requests_.front()->GetMessageText();
#else
  infobars::InfoBar* infobar = infobar_service()->infobar_at(0);
  ConfirmInfoBarDelegate* infobar_delegate =
      infobar->delegate()->AsConfirmInfoBarDelegate();
  return infobar_delegate->GetMessageText();
#endif
}

// Tests ----------------------------------------------------------------------

TEST_F(GeolocationPermissionContextTests, SinglePermissionBubble) {
  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  BubbleManagerDocumentLoadCompleted();

  EXPECT_EQ(0U, GetNumberOfPrompts());
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame, true);
  ASSERT_EQ(1U, GetNumberOfPrompts());
}

TEST_F(GeolocationPermissionContextTests,
       SinglePermissionBubbleFailsOnInsecureOrigin) {
  GURL requesting_frame("http://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  BubbleManagerDocumentLoadCompleted();

  EXPECT_EQ(0U, GetNumberOfPrompts());
  RequestGeolocationPermission(web_contents(), RequestID(0), requesting_frame,
                               true);
  ASSERT_EQ(0U, GetNumberOfPrompts());
}

#if BUILDFLAG(ANDROID_JAVA_UI)
TEST_F(GeolocationPermissionContextTests, SinglePermissionInfobar) {
  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame, true);
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

// Infobar-only tests; Android doesn't support permission bubbles.
TEST_F(GeolocationPermissionContextTests, GeolocationEnabledDisabled) {
  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  MockLocationSettings::SetLocationStatus(true, true);
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame, true);
  EXPECT_EQ(1U, infobar_service()->infobar_count());
  ConfirmInfoBarDelegate* infobar_delegate_0 =
      infobar_service()->infobar_at(0)->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_delegate_0);
  base::string16 text_0 = infobar_delegate_0->GetButtonLabel(
      ConfirmInfoBarDelegate::BUTTON_OK);

  Reload();
  MockLocationSettings::SetLocationStatus(true, false);
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame, true);
  EXPECT_EQ(0U, infobar_service()->infobar_count());
}

TEST_F(GeolocationPermissionContextTests, MasterEnabledGoogleAppsEnabled) {
  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  MockLocationSettings::SetLocationStatus(true, true);
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame, true);
  EXPECT_EQ(1U, infobar_service()->infobar_count());
  ConfirmInfoBarDelegate* infobar_delegate =
      infobar_service()->infobar_at(0)->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_delegate);
  infobar_delegate->Accept();
  CheckTabContentsState(requesting_frame, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);
}

TEST_F(GeolocationPermissionContextTests, MasterEnabledGoogleAppsDisabled) {
  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  MockLocationSettings::SetLocationStatus(true, false);
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame, true);
  EXPECT_EQ(0U, infobar_service()->infobar_count());
}
#endif

TEST_F(GeolocationPermissionContextTests, QueuedPermission) {
  GURL requesting_frame_0("https://www.example.com/geolocation");
  GURL requesting_frame_1("https://www.example-2.com/geolocation");
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      GetGeolocationContentSetting(requesting_frame_0, requesting_frame_1));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      GetGeolocationContentSetting(requesting_frame_1, requesting_frame_1));

  NavigateAndCommit(requesting_frame_0);
  BubbleManagerDocumentLoadCompleted();

  // Check that no permission requests have happened yet.
  EXPECT_EQ(0U, GetNumberOfPrompts());

  // Request permission for two frames.
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame_0, true);
  RequestGeolocationPermission(
      web_contents(), RequestID(1), requesting_frame_1, true);
  // Ensure only one infobar is created.
  ASSERT_EQ(1U, GetNumberOfPrompts());
  base::string16 text_0 = GetPromptText();

  // Accept the first frame.
  AcceptPrompt();
  CheckTabContentsState(requesting_frame_0, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);

#if BUILDFLAG(ANDROID_JAVA_UI)
  infobars::InfoBar* infobar_0 = infobar_service()->infobar_at(0);
  infobar_service()->RemoveInfoBar(infobar_0);
  EXPECT_EQ(1U, closed_infobar_tracker_.size());
  EXPECT_TRUE(closed_infobar_tracker_.Contains(infobar_0));
  closed_infobar_tracker_.Clear();
#endif

  // Now we should have a new infobar for the second frame.
  ASSERT_EQ(1U, GetNumberOfPrompts());
  base::string16 text_1 = GetPromptText();

  // Check that the messages differ.
  EXPECT_NE(text_0, text_1);

  // Cancel (block) this frame.
#if !BUILDFLAG(ANDROID_JAVA_UI)
  PermissionBubbleManager* manager =
      PermissionBubbleManager::FromWebContents(web_contents());
  DenyBubble(manager);
#else
  infobars::InfoBar* infobar_1 = infobar_service()->infobar_at(0);
  infobar_1->delegate()->AsConfirmInfoBarDelegate()->Cancel();
#endif
  CheckTabContentsState(requesting_frame_1, CONTENT_SETTING_BLOCK);
  CheckPermissionMessageSent(1, false);

  // Ensure the persisted permissions are ok.
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      GetGeolocationContentSetting(requesting_frame_0, requesting_frame_0));
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      GetGeolocationContentSetting(requesting_frame_1, requesting_frame_0));
}

TEST_F(GeolocationPermissionContextTests, HashIsIgnored) {
  GURL url_a("https://www.example.com/geolocation#a");
  GURL url_b("https://www.example.com/geolocation#b");

  // Navigate to the first url.
  NavigateAndCommit(url_a);
  BubbleManagerDocumentLoadCompleted();

  // Check permission is requested.
  ASSERT_EQ(0U, GetNumberOfPrompts());
#if BUILDFLAG(ANDROID_JAVA_UI)
  const bool user_gesture = false;
#else
  const bool user_gesture = true;
#endif
  RequestGeolocationPermission(web_contents(), RequestID(0), url_a,
                               user_gesture);
  ASSERT_EQ(1U, GetNumberOfPrompts());

  // Change the hash, we'll still be on the same page.
  NavigateAndCommit(url_b);
  BubbleManagerDocumentLoadCompleted();

  // Accept.
  AcceptPrompt();
  CheckTabContentsState(url_a, CONTENT_SETTING_ALLOW);
  CheckTabContentsState(url_b, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);

  // Cleanup.
#if BUILDFLAG(ANDROID_JAVA_UI)
  infobars::InfoBar* infobar = infobar_service()->infobar_at(0);
  infobar_service()->RemoveInfoBar(infobar);
  EXPECT_EQ(1U, closed_infobar_tracker_.size());
  EXPECT_TRUE(closed_infobar_tracker_.Contains(infobar));
#endif
}

// TODO(felt): The bubble is rejecting file:// permission requests.
// Fix and enable this test. crbug.com/444047
#if BUILDFLAG(ANDROID_JAVA_UI)
#define MAYBE_PermissionForFileScheme PermissionForFileScheme
#else
#define MAYBE_PermissionForFileScheme DISABLED_PermissionForFileScheme
#endif
TEST_F(GeolocationPermissionContextTests, MAYBE_PermissionForFileScheme) {
  GURL requesting_frame("file://example/geolocation.html");
  NavigateAndCommit(requesting_frame);
  BubbleManagerDocumentLoadCompleted();

  // Check permission is requested.
  ASSERT_EQ(0U, GetNumberOfPrompts());
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame, true);
  EXPECT_EQ(1U, GetNumberOfPrompts());

  // Accept the frame.
  AcceptPrompt();
  CheckTabContentsState(requesting_frame, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);

  // Make sure the setting is not stored.
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      GetGeolocationContentSetting(requesting_frame, requesting_frame));
}

TEST_F(GeolocationPermissionContextTests, CancelGeolocationPermissionRequest) {
  GURL frame_0("https://www.example.com/geolocation");
  GURL frame_1("https://www.example-2.com/geolocation");
  EXPECT_EQ(
      CONTENT_SETTING_ASK, GetGeolocationContentSetting(frame_0, frame_0));
  EXPECT_EQ(
      CONTENT_SETTING_ASK, GetGeolocationContentSetting(frame_1, frame_0));

  NavigateAndCommit(frame_0);
  BubbleManagerDocumentLoadCompleted();

  ASSERT_EQ(0U, GetNumberOfPrompts());

  // Request permission for two frames.
  RequestGeolocationPermission(
      web_contents(), RequestID(0), frame_0, true);
  RequestGeolocationPermission(
      web_contents(), RequestID(1), frame_1, true);

  // Get the first permission request text.
  ASSERT_EQ(1U, GetNumberOfPrompts());
  base::string16 text_0 = GetPromptText();
  ASSERT_FALSE(text_0.empty());

  // Simulate the frame going away; the request should be removed.
#if !BUILDFLAG(ANDROID_JAVA_UI)
  PermissionBubbleManager* manager =
      PermissionBubbleManager::FromWebContents(web_contents());
  CloseBubble(manager);
#else
  geolocation_permission_context_->CancelPermissionRequest(web_contents(),
                                                           RequestID(0));
#endif

  // Check that the next pending request is created correctly.
  base::string16 text_1 = GetPromptText();
  EXPECT_NE(text_0, text_1);

  // Allow this frame and check that it worked.
  AcceptPrompt();
  CheckTabContentsState(frame_1, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(1, true);

  // Ensure the persisted permissions are ok.
  EXPECT_EQ(
      CONTENT_SETTING_ASK, GetGeolocationContentSetting(frame_0, frame_0));
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW, GetGeolocationContentSetting(frame_1, frame_0));
}

TEST_F(GeolocationPermissionContextTests, InvalidURL) {
  // Navigate to the first url.
  GURL invalid_embedder("about:blank");
  GURL requesting_frame;
  NavigateAndCommit(invalid_embedder);
  BubbleManagerDocumentLoadCompleted();

  // Nothing should be displayed.
  EXPECT_EQ(0U, GetNumberOfPrompts());
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame, true);
  EXPECT_EQ(0U, GetNumberOfPrompts());
  CheckPermissionMessageSent(0, false);
}

TEST_F(GeolocationPermissionContextTests, SameOriginMultipleTabs) {
  GURL url_a("https://www.example.com/geolocation");
  GURL url_b("https://www.example-2.com/geolocation");
  NavigateAndCommit(url_a);  // Tab A0
  AddNewTab(url_b);          // Tab B (extra_tabs_[0])
  AddNewTab(url_a);          // Tab A1 (extra_tabs_[1])
  BubbleManagerDocumentLoadCompleted();
  BubbleManagerDocumentLoadCompleted(extra_tabs_[0]);
  BubbleManagerDocumentLoadCompleted(extra_tabs_[1]);
#if !BUILDFLAG(ANDROID_JAVA_UI)
  PermissionBubbleManager* manager_a0 =
      PermissionBubbleManager::FromWebContents(web_contents());
  PermissionBubbleManager* manager_b =
      PermissionBubbleManager::FromWebContents(extra_tabs_[0]);
  PermissionBubbleManager* manager_a1 =
      PermissionBubbleManager::FromWebContents(extra_tabs_[1]);
#endif

  // Request permission in all three tabs.
  RequestGeolocationPermission(
      web_contents(), RequestID(0), url_a, true);
  RequestGeolocationPermission(
      extra_tabs_[0], RequestIDForTab(0, 0), url_b, true);
  RequestGeolocationPermission(
      extra_tabs_[1], RequestIDForTab(1, 0), url_a, true);
  ASSERT_EQ(1U, GetNumberOfPrompts());  // For A0.
#if !BUILDFLAG(ANDROID_JAVA_UI)
  ASSERT_EQ(1U, GetBubblesQueueSize(manager_b));
  ASSERT_EQ(1U, GetBubblesQueueSize(manager_a1));
#else
  ASSERT_EQ(1U, infobar_service_for_tab(0)->infobar_count());
  ASSERT_EQ(1U, infobar_service_for_tab(1)->infobar_count());
#endif

  // Accept the permission in tab A0.
#if !BUILDFLAG(ANDROID_JAVA_UI)
  AcceptBubble(manager_a0);
#else
  infobars::InfoBar* infobar_a0 = infobar_service()->infobar_at(0);
  ConfirmInfoBarDelegate* infobar_delegate_a0 =
      infobar_a0->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_delegate_a0);
  infobar_delegate_a0->Accept();
  infobar_service()->RemoveInfoBar(infobar_a0);
  EXPECT_EQ(2U, closed_infobar_tracker_.size());
  EXPECT_TRUE(closed_infobar_tracker_.Contains(infobar_a0));
#endif
  CheckPermissionMessageSent(0, true);
  // Because they're the same origin, this will cause tab A1's infobar to
  // disappear. It does not cause the bubble to disappear: crbug.com/443013.
  // TODO(felt): Update this test when the bubble's behavior is changed.
  // Either way, tab B should still have a pending permission request.
#if !BUILDFLAG(ANDROID_JAVA_UI)
  ASSERT_EQ(1U, GetBubblesQueueSize(manager_a1));
  ASSERT_EQ(1U, GetBubblesQueueSize(manager_b));
#else
  CheckPermissionMessageSentForTab(1, 0, true);
  ASSERT_EQ(1U, infobar_service_for_tab(0)->infobar_count());
#endif
}

TEST_F(GeolocationPermissionContextTests, QueuedOriginMultipleTabs) {
  GURL url_a("https://www.example.com/geolocation");
  GURL url_b("https://www.example-2.com/geolocation");
  NavigateAndCommit(url_a);  // Tab A0.
  AddNewTab(url_a);          // Tab A1.
#if !BUILDFLAG(ANDROID_JAVA_UI)
  BubbleManagerDocumentLoadCompleted();
  BubbleManagerDocumentLoadCompleted(extra_tabs_[0]);
  PermissionBubbleManager* manager_a0 =
      PermissionBubbleManager::FromWebContents(web_contents());
  PermissionBubbleManager* manager_a1 =
      PermissionBubbleManager::FromWebContents(extra_tabs_[0]);
#endif

  // Request permission in both tabs; the extra tab will have two permission
  // requests from two origins.
  RequestGeolocationPermission(
      web_contents(), RequestID(0), url_a, true);
  RequestGeolocationPermission(
      extra_tabs_[0], RequestIDForTab(0, 0), url_a, true);
  RequestGeolocationPermission(
      extra_tabs_[0], RequestIDForTab(0, 1), url_b, true);
#if !BUILDFLAG(ANDROID_JAVA_UI)
  ASSERT_EQ(1U, GetBubblesQueueSize(manager_a0));
  ASSERT_EQ(1U, GetBubblesQueueSize(manager_a1));
#else
  ASSERT_EQ(1U, infobar_service()->infobar_count());
  ASSERT_EQ(1U, infobar_service_for_tab(0)->infobar_count());
#endif

  // Accept the first request in tab A1.
#if !BUILDFLAG(ANDROID_JAVA_UI)
  AcceptBubble(manager_a1);
#else
  infobars::InfoBar* infobar_a1 = infobar_service_for_tab(0)->infobar_at(0);
  ConfirmInfoBarDelegate* infobar_delegate_a1 =
      infobar_a1->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_delegate_a1);
  infobar_delegate_a1->Accept();
  infobar_service_for_tab(0)->RemoveInfoBar(infobar_a1);
  EXPECT_EQ(2U, closed_infobar_tracker_.size());
  EXPECT_TRUE(closed_infobar_tracker_.Contains(infobar_a1));
#endif
  CheckPermissionMessageSentForTab(0, 0, true);

  // Because they're the same origin, this will cause tab A0's infobar to
  // disappear. It does not cause the bubble to disappear: crbug.com/443013.
  // TODO(felt): Update this test when the bubble's behavior is changed.
#if !BUILDFLAG(ANDROID_JAVA_UI)
  EXPECT_EQ(1U, GetBubblesQueueSize(manager_a0));
#else
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  CheckPermissionMessageSent(0, true);
#endif

  // The second request should now be visible in tab A1.
#if !BUILDFLAG(ANDROID_JAVA_UI)
  ASSERT_EQ(1U, GetBubblesQueueSize(manager_a1));
#else
  ASSERT_EQ(1U, infobar_service_for_tab(0)->infobar_count());
#endif

  // Accept the second request and check that it's gone.
#if !BUILDFLAG(ANDROID_JAVA_UI)
  AcceptBubble(manager_a1);
  EXPECT_EQ(0U, GetBubblesQueueSize(manager_a1));
#else
  infobars::InfoBar* infobar_1 = infobar_service_for_tab(0)->infobar_at(0);
  ConfirmInfoBarDelegate* infobar_delegate_1 =
      infobar_1->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_delegate_1);
  infobar_delegate_1->Accept();
#endif
}

TEST_F(GeolocationPermissionContextTests, TabDestroyed) {
  GURL requesting_frame_0("https://www.example.com/geolocation");
  GURL requesting_frame_1("https://www.example-2.com/geolocation");
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      GetGeolocationContentSetting(requesting_frame_0, requesting_frame_0));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      GetGeolocationContentSetting(requesting_frame_1, requesting_frame_0));

  NavigateAndCommit(requesting_frame_0);
  BubbleManagerDocumentLoadCompleted();

  // Request permission for two frames.
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame_0, false);
  RequestGeolocationPermission(
      web_contents(), RequestID(1), requesting_frame_1, false);

  // Ensure only one prompt is created.
  ASSERT_EQ(1U, GetNumberOfPrompts());

  // Delete the tab contents.
#if BUILDFLAG(ANDROID_JAVA_UI)
  infobars::InfoBar* infobar = infobar_service()->infobar_at(0);
  DeleteContents();
  ASSERT_EQ(1U, closed_infobar_tracker_.size());
  ASSERT_TRUE(closed_infobar_tracker_.Contains(infobar));
#endif

  // The content settings should not have changed.
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      GetGeolocationContentSetting(requesting_frame_0, requesting_frame_0));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      GetGeolocationContentSetting(requesting_frame_1, requesting_frame_0));
}

TEST_F(GeolocationPermissionContextTests, LastUsageAudited) {
  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  BubbleManagerDocumentLoadCompleted();

  base::SimpleTestClock* test_clock = new base::SimpleTestClock;
  test_clock->SetNow(base::Time::UnixEpoch() +
                     base::TimeDelta::FromSeconds(10));

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetPrefClockForTesting(scoped_ptr<base::Clock>(test_clock));

  // The permission shouldn't have been used yet.
  EXPECT_EQ(map->GetLastUsage(requesting_frame.GetOrigin(),
                              requesting_frame.GetOrigin(),
                              CONTENT_SETTINGS_TYPE_GEOLOCATION).ToDoubleT(),
            0);
  ASSERT_EQ(0U, GetNumberOfPrompts());
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame, false);
  ASSERT_EQ(1U, GetNumberOfPrompts());

  AcceptPrompt();
  CheckTabContentsState(requesting_frame, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);

  // Permission has been used at the starting time.
  EXPECT_EQ(map->GetLastUsage(requesting_frame.GetOrigin(),
                              requesting_frame.GetOrigin(),
                              CONTENT_SETTINGS_TYPE_GEOLOCATION).ToDoubleT(),
            10);

  test_clock->Advance(base::TimeDelta::FromSeconds(3));
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame, false);

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

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetPrefClockForTesting(scoped_ptr<base::Clock>(test_clock));

  GURL requesting_frame_0("https://www.example.com/geolocation");
  GURL requesting_frame_1("https://www.example-2.com/geolocation");

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
  BubbleManagerDocumentLoadCompleted();

  EXPECT_EQ(0U, GetNumberOfPrompts());

  // Request permission for two frames.
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame_0, false);
  RequestGeolocationPermission(
      web_contents(), RequestID(1), requesting_frame_1, false);

  // Ensure only one infobar is created.
  ASSERT_EQ(1U, GetNumberOfPrompts());

  // Accept the first frame.
  AcceptPrompt();
#if BUILDFLAG(ANDROID_JAVA_UI)
  infobar_service()->RemoveInfoBar(infobar_service()->infobar_at(0));
#endif
  CheckTabContentsState(requesting_frame_0, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);

  // Verify that accepting the first didn't accept because it's embedded
  // in the other.
  EXPECT_EQ(map->GetLastUsage(requesting_frame_0.GetOrigin(),
                              requesting_frame_0.GetOrigin(),
                              CONTENT_SETTINGS_TYPE_GEOLOCATION).ToDoubleT(),
            10);
  EXPECT_EQ(map->GetLastUsage(requesting_frame_1.GetOrigin(),
                              requesting_frame_0.GetOrigin(),
                              CONTENT_SETTINGS_TYPE_GEOLOCATION).ToDoubleT(),
            0);

  ASSERT_EQ(1U, GetNumberOfPrompts());

  test_clock->Advance(base::TimeDelta::FromSeconds(1));

  // Allow the second frame.
  AcceptPrompt();
  CheckTabContentsState(requesting_frame_1, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(1, true);
#if BUILDFLAG(ANDROID_JAVA_UI)
  infobar_service()->RemoveInfoBar(infobar_service()->infobar_at(0));
#endif

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
      web_contents(), RequestID(0), requesting_frame_0, false);

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
