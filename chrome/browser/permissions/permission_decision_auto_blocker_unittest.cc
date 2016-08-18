// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_decision_auto_blocker.h"

#include "base/bind.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/permission_type.h"

namespace {

bool FilterGoogle(const GURL& url) {
  return url == GURL("https://www.google.com");
}

bool FilterAll(const GURL& url) {
  return true;
}

}  // namespace

class PermissionDecisionAutoBlockerUnitTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  int GetDismissalCount(const GURL& url,
                        content::PermissionType permission) {
    return PermissionDecisionAutoBlocker::GetDismissCount(
        url, permission, profile());
  }

  int GetIgnoreCount(const GURL& url, content::PermissionType permission) {
    return PermissionDecisionAutoBlocker::GetIgnoreCount(
        url, permission, profile());
  }

  int RecordDismiss(const GURL& url, content::PermissionType permission) {
    PermissionDecisionAutoBlocker blocker(profile());
    blocker.ShouldChangeDismissalToBlock(url, permission);
    return PermissionDecisionAutoBlocker::GetDismissCount(
        url, permission, profile());
  }

  int RecordIgnore(const GURL& url, content::PermissionType permission) {
    return PermissionDecisionAutoBlocker(profile()).RecordIgnore(url,
                                                                 permission);
  }
};

TEST_F(PermissionDecisionAutoBlockerUnitTest, RemoveCountsByUrl) {
  GURL url1("https://www.google.com");
  GURL url2("https://www.example.com");

  // Record some dismissals.
  EXPECT_EQ(1, RecordDismiss(url1, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(2, RecordDismiss(url1, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(3, RecordDismiss(url1, content::PermissionType::GEOLOCATION));

  EXPECT_EQ(1, RecordDismiss(url2, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(1, RecordDismiss(url1, content::PermissionType::NOTIFICATIONS));

  // Record some ignores.
  EXPECT_EQ(1, RecordIgnore(url1, content::PermissionType::MIDI_SYSEX));
  EXPECT_EQ(1, RecordIgnore(url1, content::PermissionType::DURABLE_STORAGE));
  EXPECT_EQ(1, RecordIgnore(url2, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(2, RecordIgnore(url2, content::PermissionType::GEOLOCATION));

  PermissionDecisionAutoBlocker::RemoveCountsByUrl(profile(),
                                                   base::Bind(&FilterGoogle));

  // Expect that url1's actions are gone, but url2's remain.
  EXPECT_EQ(0, GetDismissalCount(url1, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(0, GetDismissalCount(url1, content::PermissionType::NOTIFICATIONS));
  EXPECT_EQ(0, GetIgnoreCount(url1, content::PermissionType::MIDI_SYSEX));
  EXPECT_EQ(0, GetIgnoreCount(url1, content::PermissionType::DURABLE_STORAGE));

  EXPECT_EQ(1, GetDismissalCount(url2, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(2, GetIgnoreCount(url2, content::PermissionType::GEOLOCATION));

  // Add some more actions.
  EXPECT_EQ(1, RecordDismiss(url1, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(1, RecordDismiss(url1, content::PermissionType::NOTIFICATIONS));
  EXPECT_EQ(2, RecordDismiss(url2, content::PermissionType::GEOLOCATION));

  EXPECT_EQ(1, RecordIgnore(url1, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(1, RecordIgnore(url1, content::PermissionType::NOTIFICATIONS));
  EXPECT_EQ(1, RecordIgnore(url1, content::PermissionType::DURABLE_STORAGE));
  EXPECT_EQ(1, RecordIgnore(url2, content::PermissionType::MIDI_SYSEX));

  // Remove everything and expect that it's all gone.
  PermissionDecisionAutoBlocker::RemoveCountsByUrl(profile(),
                                                   base::Bind(&FilterAll));

  EXPECT_EQ(0, GetDismissalCount(url1, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(0, GetDismissalCount(url1, content::PermissionType::NOTIFICATIONS));
  EXPECT_EQ(0, GetDismissalCount(url2, content::PermissionType::GEOLOCATION));

  EXPECT_EQ(0, GetIgnoreCount(url1, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(0, GetIgnoreCount(url1, content::PermissionType::NOTIFICATIONS));
  EXPECT_EQ(0, GetIgnoreCount(url2, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(0, GetIgnoreCount(url2, content::PermissionType::DURABLE_STORAGE));
  EXPECT_EQ(0, GetIgnoreCount(url2, content::PermissionType::MIDI_SYSEX));
}
