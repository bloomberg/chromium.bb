// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/delegation_tracker.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

const char* kOrigin1 = "https://google.com";
const char* kOrigin2 = "https://maps.google.com";
const char* kOrigin3 = "https://example.com";
const char* kUniqueOrigin = "about:blank";

class DelegationTrackerTest : public ChromeRenderViewHostTestHarness {
 protected:
  content::RenderFrameHost* GetMainRFH(const char* origin) {
    content::RenderFrameHost* result = web_contents()->GetMainFrame();
    content::RenderFrameHostTester::For(result)
        ->InitializeRenderFrameIfNeeded();
    content::RenderFrameHostTester::For(result)->SimulateNavigationCommit(
        GURL(origin));
    return result;
  }

  content::RenderFrameHost* AddChildRFH(content::RenderFrameHost* parent,
                                        const char* origin) {
    content::RenderFrameHost* result =
        content::RenderFrameHostTester::For(parent)->AppendChild("");
    content::RenderFrameHostTester::For(result)
        ->InitializeRenderFrameIfNeeded();
    content::RenderFrameHostTester::For(result)->SimulateNavigationCommit(
        GURL(origin));
    return result;
  }
};

TEST_F(DelegationTrackerTest, SingleFrame) {
  DelegationTracker tracker;
  content::RenderFrameHost* parent = GetMainRFH(kOrigin1);

  EXPECT_TRUE(tracker.IsGranted(parent, CONTENT_SETTINGS_TYPE_GEOLOCATION));
}

TEST_F(DelegationTrackerTest, SingleAncestorSameOrigin) {
  DelegationTracker tracker;
  content::RenderFrameHost* parent = GetMainRFH(kOrigin1);
  content::RenderFrameHost* child = AddChildRFH(parent, kOrigin1);

  EXPECT_TRUE(tracker.IsGranted(parent, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_TRUE(tracker.IsGranted(child, CONTENT_SETTINGS_TYPE_GEOLOCATION));
}

TEST_F(DelegationTrackerTest, SingleAncestorNoDelegation) {
  DelegationTracker tracker;
  content::RenderFrameHost* parent = GetMainRFH(kOrigin1);
  content::RenderFrameHost* child = AddChildRFH(parent, kOrigin2);

  EXPECT_TRUE(tracker.IsGranted(parent, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_FALSE(tracker.IsGranted(child, CONTENT_SETTINGS_TYPE_GEOLOCATION));
}

TEST_F(DelegationTrackerTest, SingleAncestorPermissionDelegated) {
  DelegationTracker tracker;
  content::RenderFrameHost* parent = GetMainRFH(kOrigin1);
  content::RenderFrameHost* child = AddChildRFH(parent, kOrigin2);

  tracker.SetDelegatedPermissions(child, {CONTENT_SETTINGS_TYPE_GEOLOCATION});

  EXPECT_TRUE(tracker.IsGranted(parent, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_TRUE(tracker.IsGranted(child, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_FALSE(tracker.IsGranted(child, CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
}

TEST_F(DelegationTrackerTest, SingleAncestorMultiplePermissionsDelegated) {
  DelegationTracker tracker;
  content::RenderFrameHost* parent = GetMainRFH(kOrigin1);
  content::RenderFrameHost* child = AddChildRFH(parent, kOrigin2);

  tracker.SetDelegatedPermissions(child, {CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                          CONTENT_SETTINGS_TYPE_NOTIFICATIONS});

  EXPECT_TRUE(tracker.IsGranted(child, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_TRUE(tracker.IsGranted(child, CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
}

TEST_F(DelegationTrackerTest, SingleAncestorMultipleChildren) {
  DelegationTracker tracker;
  content::RenderFrameHost* parent = GetMainRFH(kOrigin1);
  content::RenderFrameHost* child1 = AddChildRFH(parent, kOrigin2);
  content::RenderFrameHost* child2 = AddChildRFH(parent, kOrigin2);

  tracker.SetDelegatedPermissions(child1, {CONTENT_SETTINGS_TYPE_GEOLOCATION});

  EXPECT_TRUE(tracker.IsGranted(child1, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_FALSE(tracker.IsGranted(child2, CONTENT_SETTINGS_TYPE_GEOLOCATION));
}

TEST_F(DelegationTrackerTest, MultipleAncestorsNotDelegated) {
  DelegationTracker tracker;
  content::RenderFrameHost* grandparent = GetMainRFH(kOrigin1);
  content::RenderFrameHost* parent = AddChildRFH(grandparent, kOrigin2);
  content::RenderFrameHost* child1 = AddChildRFH(parent, kOrigin3);
  content::RenderFrameHost* child2 = AddChildRFH(parent, kOrigin3);

  tracker.SetDelegatedPermissions(child1, {CONTENT_SETTINGS_TYPE_GEOLOCATION});

  EXPECT_FALSE(tracker.IsGranted(parent, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_FALSE(tracker.IsGranted(child1, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_FALSE(tracker.IsGranted(child2, CONTENT_SETTINGS_TYPE_GEOLOCATION));
}

TEST_F(DelegationTrackerTest, MultipleAncestorsDelegated) {
  DelegationTracker tracker;
  content::RenderFrameHost* grandparent = GetMainRFH(kOrigin1);
  content::RenderFrameHost* parent = AddChildRFH(grandparent, kOrigin2);
  content::RenderFrameHost* child1 = AddChildRFH(parent, kOrigin3);
  content::RenderFrameHost* child2 = AddChildRFH(parent, kOrigin3);

  tracker.SetDelegatedPermissions(parent, {CONTENT_SETTINGS_TYPE_GEOLOCATION});
  tracker.SetDelegatedPermissions(child1, {CONTENT_SETTINGS_TYPE_GEOLOCATION});

  EXPECT_TRUE(tracker.IsGranted(parent, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_TRUE(tracker.IsGranted(child1, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_FALSE(tracker.IsGranted(child2, CONTENT_SETTINGS_TYPE_GEOLOCATION));
}

TEST_F(DelegationTrackerTest, MultipleAncestorsSameOrigin) {
  DelegationTracker tracker;
  content::RenderFrameHost* grandparent = GetMainRFH(kOrigin1);
  content::RenderFrameHost* parent = AddChildRFH(grandparent, kOrigin1);
  content::RenderFrameHost* child1 = AddChildRFH(parent, kOrigin1);
  content::RenderFrameHost* child2 = AddChildRFH(parent, kOrigin1);

  tracker.SetDelegatedPermissions(parent, {CONTENT_SETTINGS_TYPE_GEOLOCATION});
  tracker.SetDelegatedPermissions(child1, {CONTENT_SETTINGS_TYPE_GEOLOCATION});

  EXPECT_TRUE(tracker.IsGranted(parent, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_TRUE(tracker.IsGranted(child1, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_TRUE(tracker.IsGranted(child2, CONTENT_SETTINGS_TYPE_GEOLOCATION));
}

TEST_F(DelegationTrackerTest, MultipleAncestorsComplexSinglePermission) {
  DelegationTracker tracker;
  content::RenderFrameHost* great_grandparent = GetMainRFH(kOrigin1);
  content::RenderFrameHost* grandparent =
      AddChildRFH(great_grandparent, kOrigin2);
  content::RenderFrameHost* parent1 = AddChildRFH(grandparent, kOrigin2);
  content::RenderFrameHost* parent2 = AddChildRFH(grandparent, kOrigin3);
  content::RenderFrameHost* child = AddChildRFH(parent1, kOrigin3);

  tracker.SetDelegatedPermissions(grandparent,
                                  {CONTENT_SETTINGS_TYPE_GEOLOCATION});
  tracker.SetDelegatedPermissions(child, {CONTENT_SETTINGS_TYPE_GEOLOCATION});

  EXPECT_TRUE(
      tracker.IsGranted(grandparent, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_TRUE(tracker.IsGranted(parent1, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_FALSE(tracker.IsGranted(parent2, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_TRUE(tracker.IsGranted(child, CONTENT_SETTINGS_TYPE_GEOLOCATION));
}

TEST_F(DelegationTrackerTest, MultipleAncestorsComplexMultiplePermissions) {
  DelegationTracker tracker;
  content::RenderFrameHost* great_grandparent = GetMainRFH(kOrigin1);
  content::RenderFrameHost* grandparent =
      AddChildRFH(great_grandparent, kOrigin2);
  content::RenderFrameHost* parent1 = AddChildRFH(grandparent, kOrigin2);
  content::RenderFrameHost* parent2 = AddChildRFH(grandparent, kOrigin3);
  content::RenderFrameHost* child = AddChildRFH(parent1, kOrigin3);

  tracker.SetDelegatedPermissions(
      grandparent,
      {CONTENT_SETTINGS_TYPE_GEOLOCATION, CONTENT_SETTINGS_TYPE_NOTIFICATIONS});
  tracker.SetDelegatedPermissions(child, {CONTENT_SETTINGS_TYPE_GEOLOCATION});

  EXPECT_TRUE(
      tracker.IsGranted(grandparent, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_TRUE(
      tracker.IsGranted(grandparent, CONTENT_SETTINGS_TYPE_NOTIFICATIONS));

  EXPECT_TRUE(tracker.IsGranted(parent1, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_TRUE(tracker.IsGranted(parent1, CONTENT_SETTINGS_TYPE_NOTIFICATIONS));

  EXPECT_FALSE(tracker.IsGranted(parent2, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_FALSE(tracker.IsGranted(parent2, CONTENT_SETTINGS_TYPE_NOTIFICATIONS));

  EXPECT_TRUE(tracker.IsGranted(child, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_FALSE(tracker.IsGranted(child, CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
}

TEST_F(DelegationTrackerTest, RenderFrameHostChanged) {
  DelegationTracker tracker;
  content::RenderFrameHost* grandparent = GetMainRFH(kOrigin1);
  content::RenderFrameHost* parent = AddChildRFH(grandparent, kOrigin2);
  content::RenderFrameHost* child = AddChildRFH(parent, kOrigin3);

  tracker.SetDelegatedPermissions(parent, {CONTENT_SETTINGS_TYPE_GEOLOCATION});
  tracker.SetDelegatedPermissions(child, {CONTENT_SETTINGS_TYPE_GEOLOCATION});

  EXPECT_TRUE(tracker.IsGranted(parent, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_TRUE(tracker.IsGranted(child, CONTENT_SETTINGS_TYPE_GEOLOCATION));

  content::RenderFrameHostTester::For(parent)->SimulateNavigationCommit(
      GURL(kUniqueOrigin));

  EXPECT_FALSE(tracker.IsGranted(parent, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_FALSE(tracker.IsGranted(child, CONTENT_SETTINGS_TYPE_GEOLOCATION));
}

TEST_F(DelegationTrackerTest, UniqueOrigins) {
  DelegationTracker tracker;
  content::RenderFrameHost* grandparent = GetMainRFH(kUniqueOrigin);
  content::RenderFrameHost* parent = AddChildRFH(grandparent, kOrigin2);
  content::RenderFrameHost* child = AddChildRFH(parent, kOrigin3);

  tracker.SetDelegatedPermissions(parent, {CONTENT_SETTINGS_TYPE_GEOLOCATION});
  tracker.SetDelegatedPermissions(child, {CONTENT_SETTINGS_TYPE_GEOLOCATION});

  // Unique origins should never be able to delegate permission.
  EXPECT_FALSE(tracker.IsGranted(parent, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_FALSE(tracker.IsGranted(child, CONTENT_SETTINGS_TYPE_GEOLOCATION));
}
