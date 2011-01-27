// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context.h"

#include "base/scoped_vector.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/geolocation/geolocation_content_settings_map.h"
#include "chrome/browser/geolocation/geolocation_permission_context.h"
#include "chrome/browser/geolocation/location_arbitrator.h"
#include "chrome/browser/geolocation/location_provider.h"
#include "chrome/browser/geolocation/mock_location_provider.h"
#include "chrome/browser/renderer_host/mock_render_process_host.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/test_tab_contents.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// TestTabContents short-circuits TAB_CONTENTS_INFOBAR_REMOVED to call
// InfoBarClosed() directly. We need to observe it and call InfoBarClosed()
// later.
class TestTabContentsWithPendingInfoBar : public TestTabContents {
 public:
   TestTabContentsWithPendingInfoBar(Profile* profile, SiteInstance* instance)
    : TestTabContents(profile, instance),
      removed_infobar_delegate_(NULL) {
  }

  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details) {
    // We need to special case TAB_CONTENTS_INFOBAR_REMOVED to track which
    // delegate was removed and avoid calling InfoBarClosed() supplied in the
    // base class. All other notification types are delegated to the base class.
    switch (type.value) {
      case NotificationType::TAB_CONTENTS_INFOBAR_REMOVED:
        removed_infobar_delegate_ = Details<InfoBarDelegate>(details).ptr();
        break;
      default:
        TestTabContents::Observe(type, source, details);
        break;
    }
  }

  InfoBarDelegate* removed_infobar_delegate_;
};

// This class sets up GeolocationArbitrator and injects
// TestTabContentsWithPendingInfoBar.
class GeolocationPermissionContextTests : public RenderViewHostTestHarness {
 public:
  GeolocationPermissionContextTests()
    : RenderViewHostTestHarness(),
      ui_thread_(BrowserThread::UI, MessageLoop::current()),
      tab_contents_with_pending_infobar_(NULL) {
  }

  virtual ~GeolocationPermissionContextTests() {
  }

  virtual void SetUp() {
    RenderViewHostTestHarness::SetUp();
    GeolocationArbitrator::SetProviderFactoryForTest(
        &NewAutoSuccessMockNetworkLocationProvider);
    SiteInstance* site_instance = contents_->GetSiteInstance();
    tab_contents_with_pending_infobar_ =
        new TestTabContentsWithPendingInfoBar(profile_.get(), site_instance);
    contents_.reset(tab_contents_with_pending_infobar_);
    geolocation_permission_context_ =
        new GeolocationPermissionContext(profile());
  }

  virtual void TearDown() {
    RenderViewHostTestHarness::TearDown();
    GeolocationArbitrator::SetProviderFactoryForTest(NULL);
  }

  int process_id() {
    return contents()->render_view_host()->process()->id();
  }
  int process_id_for_tab(int tab) {
    return extra_tabs_[tab]->render_view_host()->process()->id();
  }

  int render_id() {
    return contents()->render_view_host()->routing_id();
  }
  int render_id_for_tab(int tab) {
    return extra_tabs_[tab]->render_view_host()->routing_id();
  }

  int bridge_id() {
    // Bridge id is not relevant at this level.
    return 42;
  }

  void CheckPermissionMessageSent(int bridge_id, bool allowed) {
    CheckPermissionMessageSentInternal(process(), bridge_id, allowed);
  }
  void CheckPermissionMessageSentForTab(int tab, int bridge_id, bool allowed) {
    CheckPermissionMessageSentInternal(
      static_cast<MockRenderProcessHost*>(
          extra_tabs_[tab]->render_view_host()->process()),
      bridge_id, allowed);
  }

  void CheckPermissionMessageSentInternal(MockRenderProcessHost* process,
                                          int bridge_id,
                                          bool allowed) {
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
    MessageLoop::current()->Run();
    const IPC::Message* message =
        process->sink().GetFirstMessageMatching(
            ViewMsg_Geolocation_PermissionSet::ID);
    ASSERT_TRUE(message);
    ViewMsg_Geolocation_PermissionSet::Param param;
    ViewMsg_Geolocation_PermissionSet::Read(message, &param);
    EXPECT_EQ(bridge_id, param.a);
    EXPECT_EQ(allowed, param.b);
    process->sink().ClearMessages();
  }

  void AddNewTab(const GURL& url) {
    TestTabContentsWithPendingInfoBar* new_tab =
        new TestTabContentsWithPendingInfoBar(profile(), NULL);
    new_tab->controller().LoadURL(url, GURL(), PageTransition::TYPED);
    static_cast<TestRenderViewHost*>(new_tab->render_manager()->
       current_host())->SendNavigate(extra_tabs_.size() + 1, url);
    extra_tabs_.push_back(new_tab);
  }

  void CheckTabContentsState(const GURL& requesting_frame,
                             ContentSetting expected_content_setting) {
    TabSpecificContentSettings* content_settings =
        contents()->GetTabSpecificContentSettings();
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

 protected:
  BrowserThread ui_thread_;
  TestTabContentsWithPendingInfoBar* tab_contents_with_pending_infobar_;
  scoped_refptr<GeolocationPermissionContext> geolocation_permission_context_;
  ScopedVector<TestTabContentsWithPendingInfoBar> extra_tabs_;
};

TEST_F(GeolocationPermissionContextTests, SinglePermission) {
  GURL requesting_frame("http://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  EXPECT_EQ(0, contents()->infobar_delegate_count());
  geolocation_permission_context_->RequestGeolocationPermission(
      process_id(), render_id(), bridge_id(), requesting_frame);
  EXPECT_EQ(1, contents()->infobar_delegate_count());
}

TEST_F(GeolocationPermissionContextTests, QueuedPermission) {
  GURL requesting_frame_0("http://www.example.com/geolocation");
  GURL requesting_frame_1("http://www.example-2.com/geolocation");
  EXPECT_EQ(CONTENT_SETTING_ASK,
      profile()->GetGeolocationContentSettingsMap()->GetContentSetting(
          requesting_frame_0, requesting_frame_0));
  EXPECT_EQ(CONTENT_SETTING_ASK,
      profile()->GetGeolocationContentSettingsMap()->GetContentSetting(
          requesting_frame_1, requesting_frame_0));

  NavigateAndCommit(requesting_frame_0);
  EXPECT_EQ(0, contents()->infobar_delegate_count());
  // Request permission for two frames.
  geolocation_permission_context_->RequestGeolocationPermission(
      process_id(), render_id(), bridge_id(), requesting_frame_0);
  geolocation_permission_context_->RequestGeolocationPermission(
      process_id(), render_id(), bridge_id() + 1, requesting_frame_1);
  // Ensure only one infobar is created.
  EXPECT_EQ(1, contents()->infobar_delegate_count());
  ConfirmInfoBarDelegate* infobar_0 =
      contents()->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_0);
  string16 text_0 = infobar_0->GetMessageText();

  // Accept the first frame.
  infobar_0->Accept();
  CheckTabContentsState(requesting_frame_0, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(bridge_id(), true);

  contents()->RemoveInfoBar(infobar_0);
  EXPECT_EQ(infobar_0,
            tab_contents_with_pending_infobar_->removed_infobar_delegate_);
  infobar_0->InfoBarClosed();
  // Now we should have a new infobar for the second frame.
  EXPECT_EQ(1, contents()->infobar_delegate_count());

  ConfirmInfoBarDelegate* infobar_1 =
      contents()->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_1);
  string16 text_1 = infobar_1->GetMessageText();
  EXPECT_NE(text_0, text_1);

  // Cancel (block) this frame.
  infobar_1->Cancel();
  CheckTabContentsState(requesting_frame_1, CONTENT_SETTING_BLOCK);
  CheckPermissionMessageSent(bridge_id() + 1, false);
  contents()->RemoveInfoBar(infobar_1);
  EXPECT_EQ(infobar_1,
            tab_contents_with_pending_infobar_->removed_infobar_delegate_);
  infobar_1->InfoBarClosed();
  EXPECT_EQ(0, contents()->infobar_delegate_count());
  // Ensure the persisted permissions are ok.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
      profile()->GetGeolocationContentSettingsMap()->GetContentSetting(
          requesting_frame_0, requesting_frame_0));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
      profile()->GetGeolocationContentSettingsMap()->GetContentSetting(
          requesting_frame_1, requesting_frame_0));
}

TEST_F(GeolocationPermissionContextTests, CancelGeolocationPermissionRequest) {
  GURL requesting_frame_0("http://www.example.com/geolocation");
  GURL requesting_frame_1("http://www.example-2.com/geolocation");
  EXPECT_EQ(CONTENT_SETTING_ASK,
      profile()->GetGeolocationContentSettingsMap()->GetContentSetting(
          requesting_frame_0, requesting_frame_0));
  EXPECT_EQ(CONTENT_SETTING_ASK,
      profile()->GetGeolocationContentSettingsMap()->GetContentSetting(
          requesting_frame_1, requesting_frame_0));

  NavigateAndCommit(requesting_frame_0);
  EXPECT_EQ(0, contents()->infobar_delegate_count());
  // Request permission for two frames.
  geolocation_permission_context_->RequestGeolocationPermission(
      process_id(), render_id(), bridge_id(), requesting_frame_0);
  geolocation_permission_context_->RequestGeolocationPermission(
      process_id(), render_id(), bridge_id() + 1, requesting_frame_1);
  EXPECT_EQ(1, contents()->infobar_delegate_count());

  ConfirmInfoBarDelegate* infobar_0 =
      contents()->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_0);
  string16 text_0 = infobar_0->GetMessageText();

  // Simulate the frame going away, ensure the infobar for this frame
  // is removed and the next pending infobar is created.
  geolocation_permission_context_->CancelGeolocationPermissionRequest(
      process_id(), render_id(), bridge_id(), requesting_frame_0);
  EXPECT_EQ(infobar_0,
            tab_contents_with_pending_infobar_->removed_infobar_delegate_);
  infobar_0->InfoBarClosed();
  EXPECT_EQ(1, contents()->infobar_delegate_count());

  ConfirmInfoBarDelegate* infobar_1 =
      contents()->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_1);
  string16 text_1 = infobar_1->GetMessageText();
  EXPECT_NE(text_0, text_1);

  // Allow this frame.
  infobar_1->Accept();
  CheckTabContentsState(requesting_frame_1, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(bridge_id() + 1, true);
  contents()->RemoveInfoBar(infobar_1);
  EXPECT_EQ(infobar_1,
            tab_contents_with_pending_infobar_->removed_infobar_delegate_);
  infobar_1->InfoBarClosed();
  EXPECT_EQ(0, contents()->infobar_delegate_count());
  // Ensure the persisted permissions are ok.
  EXPECT_EQ(CONTENT_SETTING_ASK,
      profile()->GetGeolocationContentSettingsMap()->GetContentSetting(
          requesting_frame_0, requesting_frame_0));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
      profile()->GetGeolocationContentSettingsMap()->GetContentSetting(
          requesting_frame_1, requesting_frame_0));
}

TEST_F(GeolocationPermissionContextTests, InvalidURL) {
  GURL invalid_embedder;
  GURL requesting_frame("about:blank");
  NavigateAndCommit(invalid_embedder);
  EXPECT_EQ(0, contents()->infobar_delegate_count());
  geolocation_permission_context_->RequestGeolocationPermission(
      process_id(), render_id(), bridge_id(), requesting_frame);
  EXPECT_EQ(0, contents()->infobar_delegate_count());
  CheckPermissionMessageSent(bridge_id(), false);
}

TEST_F(GeolocationPermissionContextTests, SameOriginMultipleTabs) {
  GURL url_a("http://www.example.com/geolocation");
  GURL url_b("http://www.example-2.com/geolocation");
  NavigateAndCommit(url_a);
  AddNewTab(url_b);
  AddNewTab(url_a);

  EXPECT_EQ(0, contents()->infobar_delegate_count());
  geolocation_permission_context_->RequestGeolocationPermission(
      process_id(), render_id(), bridge_id(), url_a);
  EXPECT_EQ(1, contents()->infobar_delegate_count());

  geolocation_permission_context_->RequestGeolocationPermission(
      process_id_for_tab(0), render_id_for_tab(0), bridge_id(), url_b);
  EXPECT_EQ(1, extra_tabs_[0]->infobar_delegate_count());

  geolocation_permission_context_->RequestGeolocationPermission(
      process_id_for_tab(1), render_id_for_tab(1), bridge_id(), url_a);
  EXPECT_EQ(1, extra_tabs_[1]->infobar_delegate_count());

  ConfirmInfoBarDelegate* removed_infobar =
      extra_tabs_[1]->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();

  // Accept the first tab.
  ConfirmInfoBarDelegate* infobar_0 =
      contents()->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_0);
  infobar_0->Accept();
  CheckPermissionMessageSent(bridge_id(), true);
  contents()->RemoveInfoBar(infobar_0);
  EXPECT_EQ(infobar_0,
            tab_contents_with_pending_infobar_->removed_infobar_delegate_);
  infobar_0->InfoBarClosed();
  // Now the infobar for the tab with the same origin should have gone.
  EXPECT_EQ(0, extra_tabs_[1]->infobar_delegate_count());
  CheckPermissionMessageSentForTab(1, bridge_id(), true);
  // Destroy the infobar that has just been removed.
  removed_infobar->InfoBarClosed();

  // But the other tab should still have the info bar...
  EXPECT_EQ(1, extra_tabs_[0]->infobar_delegate_count());
  extra_tabs_.reset();
}

TEST_F(GeolocationPermissionContextTests, QueuedOriginMultipleTabs) {
  GURL url_a("http://www.example.com/geolocation");
  GURL url_b("http://www.example-2.com/geolocation");
  NavigateAndCommit(url_a);
  AddNewTab(url_a);

  EXPECT_EQ(0, contents()->infobar_delegate_count());
  geolocation_permission_context_->RequestGeolocationPermission(
      process_id(), render_id(), bridge_id(), url_a);
  EXPECT_EQ(1, contents()->infobar_delegate_count());

  geolocation_permission_context_->RequestGeolocationPermission(
      process_id_for_tab(0), render_id_for_tab(0), bridge_id(), url_a);
  EXPECT_EQ(1, extra_tabs_[0]->infobar_delegate_count());

  geolocation_permission_context_->RequestGeolocationPermission(
      process_id_for_tab(0), render_id_for_tab(0), bridge_id() + 1, url_b);
  EXPECT_EQ(1, extra_tabs_[0]->infobar_delegate_count());

  ConfirmInfoBarDelegate* removed_infobar =
      contents()->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();

  // Accept the second tab.
  ConfirmInfoBarDelegate* infobar_0 =
      extra_tabs_[0]->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_0);
  infobar_0->Accept();
  CheckPermissionMessageSentForTab(0, bridge_id(), true);
  extra_tabs_[0]->RemoveInfoBar(infobar_0);
  EXPECT_EQ(infobar_0,
            extra_tabs_[0]->removed_infobar_delegate_);
  infobar_0->InfoBarClosed();
  // Now the infobar for the tab with the same origin should have gone.
  EXPECT_EQ(0, contents()->infobar_delegate_count());
  CheckPermissionMessageSent(bridge_id(), true);
  // Destroy the infobar that has just been removed.
  removed_infobar->InfoBarClosed();

  // And we should have the queued infobar displayed now.
  EXPECT_EQ(1, extra_tabs_[0]->infobar_delegate_count());

  // Accept the second infobar.
  ConfirmInfoBarDelegate* infobar_1 =
      extra_tabs_[0]->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_1);
  infobar_1->Accept();
  CheckPermissionMessageSentForTab(0, bridge_id() + 1, true);
  extra_tabs_[0]->RemoveInfoBar(infobar_1);
  EXPECT_EQ(infobar_1,
            extra_tabs_[0]->removed_infobar_delegate_);
  infobar_1->InfoBarClosed();

  extra_tabs_.reset();
}

TEST_F(GeolocationPermissionContextTests, TabDestroyed) {
  GURL requesting_frame_0("http://www.example.com/geolocation");
  GURL requesting_frame_1("http://www.example-2.com/geolocation");
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      profile()->GetGeolocationContentSettingsMap()->GetContentSetting(
          requesting_frame_0, requesting_frame_0));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      profile()->GetGeolocationContentSettingsMap()->GetContentSetting(
          requesting_frame_1, requesting_frame_0));

  NavigateAndCommit(requesting_frame_0);
  EXPECT_EQ(0, contents()->infobar_delegate_count());
  // Request permission for two frames.
  geolocation_permission_context_->RequestGeolocationPermission(
      process_id(), render_id(), bridge_id(), requesting_frame_0);
  geolocation_permission_context_->RequestGeolocationPermission(
      process_id(), render_id(), bridge_id() + 1, requesting_frame_1);
  // Ensure only one infobar is created.
  EXPECT_EQ(1, contents()->infobar_delegate_count());
  ConfirmInfoBarDelegate* infobar_0 =
      contents()->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(infobar_0);
  string16 text_0 = infobar_0->GetMessageText();

  // Delete the tab contents.
  DeleteContents();
}

}  // namespace
