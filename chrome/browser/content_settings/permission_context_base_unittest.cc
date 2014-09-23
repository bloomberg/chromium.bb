// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/permission_context_base.h"

#include "base/bind.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/permission_queue_controller.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/permission_request_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

class PermissionContextBaseTests : public ChromeRenderViewHostTestHarness {
 protected:
  PermissionContextBaseTests() {}
  virtual ~PermissionContextBaseTests() {}

 private:
  // ChromeRenderViewHostTestHarness:
  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
    InfoBarService::CreateForWebContents(web_contents());
  }

  DISALLOW_COPY_AND_ASSIGN(PermissionContextBaseTests);
};

class TestPermissionContext : public PermissionContextBase {
 public:
  TestPermissionContext(Profile* profile,
                  const ContentSettingsType permission_type)
   : PermissionContextBase(profile, permission_type),
     permission_set_(false),
     permission_granted_(false),
     tab_context_updated_(false) {}

  virtual ~TestPermissionContext() {}

  PermissionQueueController* GetInfoBarController() {
    return GetQueueController();
  }

  bool permission_granted() {
    return permission_granted_;
  }

  bool permission_set() {
    return permission_set_;
  }

  bool tab_context_updated() {
    return tab_context_updated_;
  }

  void TrackPermissionDecision(bool granted) {
    permission_set_ = true;
    permission_granted_ = granted;
  }

 protected:
  virtual void UpdateTabContext(const PermissionRequestID& id,
                                const GURL& requesting_origin,
                                bool allowed) OVERRIDE {
    tab_context_updated_ = true;
  }

 private:
   bool permission_set_;
   bool permission_granted_;
   bool tab_context_updated_;
};

// Simulates clicking Accept. The permission should be granted and
// saved for future use.
TEST_F(PermissionContextBaseTests, TestAskAndGrant) {
  TestPermissionContext permission_context(profile(),
                      CONTENT_SETTINGS_TYPE_PUSH_MESSAGING);
  GURL url("http://www.google.com");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);

  const PermissionRequestID id(
      web_contents()->GetRenderProcessHost()->GetID(),
      web_contents()->GetRenderViewHost()->GetRoutingID(),
      -1, GURL());
  permission_context.RequestPermission(
      web_contents(),
      id, url, true,
      base::Bind(&TestPermissionContext::TrackPermissionDecision,
                 base::Unretained(&permission_context)));

  permission_context.GetInfoBarController()->OnPermissionSet(
      id, url, url, true, true);
  EXPECT_TRUE(permission_context.permission_set());
  EXPECT_TRUE(permission_context.permission_granted());
  EXPECT_TRUE(permission_context.tab_context_updated());

  ContentSetting setting =
      profile()->GetHostContentSettingsMap()->GetContentSetting(
          url.GetOrigin(), url.GetOrigin(),
          CONTENT_SETTINGS_TYPE_PUSH_MESSAGING, std::string());
  EXPECT_EQ(CONTENT_SETTING_ALLOW , setting);
};

// Simulates clicking Dismiss (X in the infobar.
// The permission should be denied but not saved for future use.
TEST_F(PermissionContextBaseTests, TestAskAndDismiss) {
  TestPermissionContext permission_context(profile(),
                      CONTENT_SETTINGS_TYPE_MIDI_SYSEX);
  GURL url("http://www.google.es");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);

  const PermissionRequestID id(
      web_contents()->GetRenderProcessHost()->GetID(),
      web_contents()->GetRenderViewHost()->GetRoutingID(),
      -1, GURL());
  permission_context.RequestPermission(
      web_contents(),
      id, url, true,
      base::Bind(&TestPermissionContext::TrackPermissionDecision,
                 base::Unretained(&permission_context)));

  permission_context.GetInfoBarController()->OnPermissionSet(
      id, url, url, false, false);
  EXPECT_TRUE(permission_context.permission_set());
  EXPECT_FALSE(permission_context.permission_granted());
  EXPECT_TRUE(permission_context.tab_context_updated());

  ContentSetting setting =
      profile()->GetHostContentSettingsMap()->GetContentSetting(
          url.GetOrigin(), url.GetOrigin(),
          CONTENT_SETTINGS_TYPE_MIDI_SYSEX, std::string());
  EXPECT_EQ(CONTENT_SETTING_ASK , setting);
};
