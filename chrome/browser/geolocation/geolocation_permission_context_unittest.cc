// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context.h"

#include <stddef.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/id_map.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/permissions/permission_context_base.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_request_id.h"
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
#include "extensions/features/features.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/mock_location_settings.h"
#include "chrome/browser/android/search_geolocation/search_geolocation_service.h"
#include "chrome/browser/geolocation/geolocation_permission_context_android.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission_status.mojom.h"
#else
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/ui/website_settings/mock_permission_prompt_factory.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
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
  DCHECK_EQ(chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED, type);
  removed_infobars_.insert(
      content::Details<infobars::InfoBar::RemovedDetails>(details)->first);
}

bool ClosedInfoBarTracker::Contains(infobars::InfoBar* infobar) const {
  return removed_infobars_.count(infobar) != 0;
}

void ClosedInfoBarTracker::Clear() {
  removed_infobars_.clear();
}

#if defined(OS_ANDROID)
// TestSearchEngineDelegate
class TestSearchEngineDelegate
    : public SearchGeolocationService::SearchEngineDelegate {
 public:
  bool IsDSEGoogle() override { return true; }

  url::Origin GetGoogleDSECCTLD() override {
    return url::Origin(GURL(kDSETestUrl));
  }

  void SetDSEChangedCallback(const base::Closure& callback) override {}

  static const char kDSETestUrl[];
};

const char TestSearchEngineDelegate::kDSETestUrl[] = "https://www.dsetest.com";
#endif  // defined(OS_ANDROID)

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
    return InfoBarService::FromWebContents(extra_tabs_[tab].get());
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
#if !defined(OS_ANDROID)
  void SetupRequestManager(content::WebContents* web_contents);
  size_t GetBubblesQueueSize(PermissionRequestManager* manager);
  void AcceptBubble(PermissionRequestManager* manager);
  void DenyBubble(PermissionRequestManager* manager);
  void CloseBubble(PermissionRequestManager* manager);
#endif
  void RequestManagerDocumentLoadCompleted();
  void RequestManagerDocumentLoadCompleted(content::WebContents* web_contents);
  ContentSetting GetGeolocationContentSetting(GURL frame_0, GURL frame_1);
  size_t GetNumberOfPrompts();
  void AcceptPrompt();
  base::string16 GetPromptText();

  // owned by the browser context
  GeolocationPermissionContext* geolocation_permission_context_;
  ClosedInfoBarTracker closed_infobar_tracker_;
  std::vector<std::unique_ptr<content::WebContents>> extra_tabs_;
#if !defined(OS_ANDROID)
  std::vector<std::unique_ptr<MockPermissionPromptFactory>>
      mock_permission_prompt_factories_;
#endif

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
      ->SendNavigate(entry->GetUniqueID(), true, url);

  // Set up required helpers, and make this be as "tabby" as the code requires.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::SetViewType(new_tab, extensions::VIEW_TYPE_TAB_CONTENTS);
#endif

#if defined(OS_ANDROID)
  InfoBarService::CreateForWebContents(new_tab);
#else
  SetupRequestManager(new_tab);
#endif

  extra_tabs_.push_back(base::WrapUnique(new_tab));
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
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::SetViewType(web_contents(), extensions::VIEW_TYPE_TAB_CONTENTS);
#endif
  InfoBarService::CreateForWebContents(web_contents());
  TabSpecificContentSettings::CreateForWebContents(web_contents());
  geolocation_permission_context_ = static_cast<GeolocationPermissionContext*>(
      PermissionManager::Get(profile())->GetPermissionContext(
          CONTENT_SETTINGS_TYPE_GEOLOCATION));
#if defined(OS_ANDROID)
  static_cast<GeolocationPermissionContextAndroid*>(
      geolocation_permission_context_)
      ->SetLocationSettingsForTesting(
          std::unique_ptr<LocationSettings>(new MockLocationSettings()));
  MockLocationSettings::SetLocationStatus(true, true);
#else
  SetupRequestManager(web_contents());
#endif
}

void GeolocationPermissionContextTests::TearDown() {
#if !defined(OS_ANDROID)
  mock_permission_prompt_factories_.clear();
#endif
  extra_tabs_.clear();
  ChromeRenderViewHostTestHarness::TearDown();
}

#if !defined(OS_ANDROID)
void GeolocationPermissionContextTests::SetupRequestManager(
    content::WebContents* web_contents) {
  // Create PermissionRequestManager.
  PermissionRequestManager::CreateForWebContents(web_contents);
  PermissionRequestManager* permission_request_manager =
      PermissionRequestManager::FromWebContents(web_contents);

  // Create a MockPermissionPromptFactory for the PermissionRequestManager.
  mock_permission_prompt_factories_.push_back(
      base::MakeUnique<MockPermissionPromptFactory>(
          permission_request_manager));

  // Prepare the PermissionRequestManager to display a mock bubble.
  permission_request_manager->DisplayPendingRequests();
}

size_t GeolocationPermissionContextTests::GetBubblesQueueSize(
    PermissionRequestManager* manager) {
  return manager->requests_.size();
}

void GeolocationPermissionContextTests::AcceptBubble(
    PermissionRequestManager* manager) {
  manager->Accept();
}

void GeolocationPermissionContextTests::DenyBubble(
    PermissionRequestManager* manager) {
  manager->Deny();
}

void GeolocationPermissionContextTests::CloseBubble(
    PermissionRequestManager* manager) {
  manager->Closing();
}
#endif

void GeolocationPermissionContextTests::RequestManagerDocumentLoadCompleted() {
  GeolocationPermissionContextTests::RequestManagerDocumentLoadCompleted(
      web_contents());
}

void GeolocationPermissionContextTests::RequestManagerDocumentLoadCompleted(
    content::WebContents* web_contents) {
#if !defined(OS_ANDROID)
  PermissionRequestManager::FromWebContents(web_contents)->
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
#if !defined(OS_ANDROID)
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents());
  return GetBubblesQueueSize(manager);
#else
  return infobar_service()->infobar_count();
#endif
}

void GeolocationPermissionContextTests::AcceptPrompt() {
#if !defined(OS_ANDROID)
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents());
  AcceptBubble(manager);
#else
  infobars::InfoBar* infobar = infobar_service()->infobar_at(0);
  ConfirmInfoBarDelegate* infobar_delegate =
      infobar->delegate()->AsConfirmInfoBarDelegate();
  infobar_delegate->Accept();
#endif
}

base::string16 GeolocationPermissionContextTests::GetPromptText() {
#if !defined(OS_ANDROID)
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents());
  PermissionRequest* request = manager->requests_.front();
  return base::ASCIIToUTF16(request->GetOrigin().spec()) +
         request->GetMessageTextFragment();
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
  RequestManagerDocumentLoadCompleted();

  EXPECT_EQ(0U, GetNumberOfPrompts());
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame, true);
  ASSERT_EQ(1U, GetNumberOfPrompts());
}

TEST_F(GeolocationPermissionContextTests,
       SinglePermissionBubbleFailsOnInsecureOrigin) {
  GURL requesting_frame("http://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();

  EXPECT_EQ(0U, GetNumberOfPrompts());
  RequestGeolocationPermission(web_contents(), RequestID(0), requesting_frame,
                               true);
  ASSERT_EQ(0U, GetNumberOfPrompts());
}

#if defined(OS_ANDROID)
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
  RequestManagerDocumentLoadCompleted();

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

#if defined(OS_ANDROID)
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
#if !defined(OS_ANDROID)
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents());
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
  RequestManagerDocumentLoadCompleted();

  // Check permission is requested.
  ASSERT_EQ(0U, GetNumberOfPrompts());
#if defined(OS_ANDROID)
  const bool user_gesture = false;
#else
  const bool user_gesture = true;
#endif
  RequestGeolocationPermission(web_contents(), RequestID(0), url_a,
                               user_gesture);
  ASSERT_EQ(1U, GetNumberOfPrompts());

  // Change the hash, we'll still be on the same page.
  NavigateAndCommit(url_b);
  RequestManagerDocumentLoadCompleted();

  // Accept.
  AcceptPrompt();
  CheckTabContentsState(url_a, CONTENT_SETTING_ALLOW);
  CheckTabContentsState(url_b, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);

  // Cleanup.
#if defined(OS_ANDROID)
  infobars::InfoBar* infobar = infobar_service()->infobar_at(0);
  infobar_service()->RemoveInfoBar(infobar);
  EXPECT_EQ(1U, closed_infobar_tracker_.size());
  EXPECT_TRUE(closed_infobar_tracker_.Contains(infobar));
#endif
}

// TODO(felt): The bubble is rejecting file:// permission requests.
// Fix and enable this test. crbug.com/444047
#if defined(OS_ANDROID)
#define MAYBE_PermissionForFileScheme PermissionForFileScheme
#else
#define MAYBE_PermissionForFileScheme DISABLED_PermissionForFileScheme
#endif
TEST_F(GeolocationPermissionContextTests, MAYBE_PermissionForFileScheme) {
  GURL requesting_frame("file://example/geolocation.html");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();

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
  RequestManagerDocumentLoadCompleted();

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
#if !defined(OS_ANDROID)
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents());
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
  RequestManagerDocumentLoadCompleted();

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
  RequestManagerDocumentLoadCompleted();
  RequestManagerDocumentLoadCompleted(extra_tabs_[0].get());
  RequestManagerDocumentLoadCompleted(extra_tabs_[1].get());
#if !defined(OS_ANDROID)
  PermissionRequestManager* manager_a0 =
      PermissionRequestManager::FromWebContents(web_contents());
  PermissionRequestManager* manager_b =
      PermissionRequestManager::FromWebContents(extra_tabs_[0].get());
  PermissionRequestManager* manager_a1 =
      PermissionRequestManager::FromWebContents(extra_tabs_[1].get());
#endif

  // Request permission in all three tabs.
  RequestGeolocationPermission(
      web_contents(), RequestID(0), url_a, true);
  RequestGeolocationPermission(
      extra_tabs_[0].get(), RequestIDForTab(0, 0), url_b, true);
  RequestGeolocationPermission(
      extra_tabs_[1].get(), RequestIDForTab(1, 0), url_a, true);
  ASSERT_EQ(1U, GetNumberOfPrompts());  // For A0.
#if !defined(OS_ANDROID)
  ASSERT_EQ(1U, GetBubblesQueueSize(manager_b));
  ASSERT_EQ(1U, GetBubblesQueueSize(manager_a1));
#else
  ASSERT_EQ(1U, infobar_service_for_tab(0)->infobar_count());
  ASSERT_EQ(1U, infobar_service_for_tab(1)->infobar_count());
#endif

  // Accept the permission in tab A0.
#if !defined(OS_ANDROID)
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
#if !defined(OS_ANDROID)
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
#if !defined(OS_ANDROID)
  RequestManagerDocumentLoadCompleted();
  RequestManagerDocumentLoadCompleted(extra_tabs_[0].get());
  PermissionRequestManager* manager_a0 =
      PermissionRequestManager::FromWebContents(web_contents());
  PermissionRequestManager* manager_a1 =
      PermissionRequestManager::FromWebContents(extra_tabs_[0].get());
#endif

  // Request permission in both tabs; the extra tab will have two permission
  // requests from two origins.
  RequestGeolocationPermission(
      web_contents(), RequestID(0), url_a, true);
  RequestGeolocationPermission(
      extra_tabs_[0].get(), RequestIDForTab(0, 0), url_a, true);
  RequestGeolocationPermission(
      extra_tabs_[0].get(), RequestIDForTab(0, 1), url_b, true);
#if !defined(OS_ANDROID)
  ASSERT_EQ(1U, GetBubblesQueueSize(manager_a0));
  ASSERT_EQ(1U, GetBubblesQueueSize(manager_a1));
#else
  ASSERT_EQ(1U, infobar_service()->infobar_count());
  ASSERT_EQ(1U, infobar_service_for_tab(0)->infobar_count());
#endif

  // Accept the first request in tab A1.
#if !defined(OS_ANDROID)
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
#if !defined(OS_ANDROID)
  EXPECT_EQ(1U, GetBubblesQueueSize(manager_a0));
#else
  EXPECT_EQ(0U, infobar_service()->infobar_count());
  CheckPermissionMessageSent(0, true);
#endif

  // The second request should now be visible in tab A1.
#if !defined(OS_ANDROID)
  ASSERT_EQ(1U, GetBubblesQueueSize(manager_a1));
#else
  ASSERT_EQ(1U, infobar_service_for_tab(0)->infobar_count());
#endif

  // Accept the second request and check that it's gone.
#if !defined(OS_ANDROID)
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
  RequestManagerDocumentLoadCompleted();

  // Request permission for two frames.
  RequestGeolocationPermission(
      web_contents(), RequestID(0), requesting_frame_0, false);
  RequestGeolocationPermission(
      web_contents(), RequestID(1), requesting_frame_1, false);

  // Ensure only one prompt is created.
  ASSERT_EQ(1U, GetNumberOfPrompts());

  // Delete the tab contents.
#if defined(OS_ANDROID)
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

#if defined(OS_ANDROID)
TEST_F(GeolocationPermissionContextTests, SearchGeolocationInIncognito) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kConsistentOmniboxGeolocation);

  GURL requesting_frame(TestSearchEngineDelegate::kDSETestUrl);
  // The DSE Geolocation setting should be used in incognito if it is BLOCK,
  // but not if it is ALLOW.
  SearchGeolocationService* geo_service =
      SearchGeolocationService::Factory::GetForBrowserContext(profile());
  geo_service->SetSearchEngineDelegateForTest(
      base::MakeUnique<TestSearchEngineDelegate>());

  Profile* otr_profile = profile()->GetOffTheRecordProfile();

  // A DSE setting of ALLOW should not flow through to incognito.
  geo_service->SetDSEGeolocationSetting(true);
  ASSERT_EQ(CONTENT_SETTING_ASK,
            PermissionManager::Get(otr_profile)
                ->GetPermissionStatus(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                      requesting_frame, requesting_frame)
                .content_setting);

  // Changing the setting to BLOCK should flow through to incognito.
  geo_service->SetDSEGeolocationSetting(false);
  ASSERT_EQ(CONTENT_SETTING_BLOCK,
            PermissionManager::Get(otr_profile)
                ->GetPermissionStatus(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                      requesting_frame, requesting_frame)
                .content_setting);
}
#endif  // defined(OS_ANDROID)
