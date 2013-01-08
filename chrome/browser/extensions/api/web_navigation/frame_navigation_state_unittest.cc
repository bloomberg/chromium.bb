// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/extensions/api/web_navigation/frame_navigation_state.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

content::RenderViewHost* fake_rvh =
    reinterpret_cast<content::RenderViewHost*>(31337);

// Test that a frame is correctly tracked, and removed once the tab contents
// goes away.
TEST(FrameNavigationStateTest, TrackFrame) {
  FrameNavigationState navigation_state;
  const FrameNavigationState::FrameID frame_id0(-1, fake_rvh);
  const FrameNavigationState::FrameID frame_id1(23, fake_rvh);
  const FrameNavigationState::FrameID frame_id2(42, fake_rvh);
  const GURL url1("http://www.google.com/");
  const GURL url2("http://mail.google.com/");

  // Create a main frame.
  EXPECT_FALSE(navigation_state.CanSendEvents(frame_id1));
  EXPECT_FALSE(navigation_state.IsValidFrame(frame_id1));
  navigation_state.TrackFrame(frame_id1, frame_id0, url1, true, false, false);
  navigation_state.SetNavigationCommitted(frame_id1);
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id1));
  EXPECT_TRUE(navigation_state.IsValidFrame(frame_id1));

  // Add a sub frame.
  EXPECT_FALSE(navigation_state.CanSendEvents(frame_id2));
  EXPECT_FALSE(navigation_state.IsValidFrame(frame_id2));
  navigation_state.TrackFrame(frame_id2, frame_id1, url2, false, false, false);
  navigation_state.SetNavigationCommitted(frame_id2);
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id2));
  EXPECT_TRUE(navigation_state.IsValidFrame(frame_id2));

  // Check frame state.
  EXPECT_TRUE(navigation_state.IsMainFrame(frame_id1));
  EXPECT_EQ(url1, navigation_state.GetUrl(frame_id1));
  EXPECT_FALSE(navigation_state.IsMainFrame(frame_id2));
  EXPECT_EQ(url2, navigation_state.GetUrl(frame_id2));
  EXPECT_EQ(frame_id1, navigation_state.GetMainFrameID());

  // Drop the frames.
  navigation_state.StopTrackingFramesInRVH(fake_rvh,
                                           FrameNavigationState::FrameID());
  EXPECT_FALSE(navigation_state.CanSendEvents(frame_id1));
  EXPECT_FALSE(navigation_state.IsValidFrame(frame_id1));
  EXPECT_FALSE(navigation_state.CanSendEvents(frame_id2));
  EXPECT_FALSE(navigation_state.IsValidFrame(frame_id2));
}

// Test that no events can be sent for a frame after an error occurred, but
// before a new navigation happened in this frame.
TEST(FrameNavigationStateTest, ErrorState) {
  FrameNavigationState navigation_state;
  const FrameNavigationState::FrameID frame_id0(-1, fake_rvh);
  const FrameNavigationState::FrameID frame_id1(42, fake_rvh);
  const GURL url("http://www.google.com/");

  navigation_state.TrackFrame(frame_id1, frame_id0, url, true, false, false);
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id1));
  EXPECT_FALSE(navigation_state.GetErrorOccurredInFrame(frame_id1));

  // After an error occurred, no further events should be sent.
  navigation_state.SetErrorOccurredInFrame(frame_id1);
  EXPECT_FALSE(navigation_state.CanSendEvents(frame_id1));
  EXPECT_TRUE(navigation_state.GetErrorOccurredInFrame(frame_id1));

  // Navigations to a network error page should be ignored.
  navigation_state.TrackFrame(frame_id1, frame_id0, GURL(), true, true, false);
  EXPECT_FALSE(navigation_state.CanSendEvents(frame_id1));
  EXPECT_TRUE(navigation_state.GetErrorOccurredInFrame(frame_id1));

  // However, when the frame navigates again, it should send events again.
  navigation_state.TrackFrame(frame_id1, frame_id0, url, true, false, false);
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id1));
  EXPECT_FALSE(navigation_state.GetErrorOccurredInFrame(frame_id1));
}

// Tests that for a sub frame, no events are send after an error occurred, but
// before a new navigation happened in this frame.
TEST(FrameNavigationStateTest, ErrorStateFrame) {
  FrameNavigationState navigation_state;
  const FrameNavigationState::FrameID frame_id0(-1, fake_rvh);
  const FrameNavigationState::FrameID frame_id1(23, fake_rvh);
  const FrameNavigationState::FrameID frame_id2(42, fake_rvh);
  const GURL url("http://www.google.com/");

  navigation_state.TrackFrame(frame_id1, frame_id0, url, true, false, false);
  navigation_state.TrackFrame(frame_id2, frame_id1, url, false, false, false);
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id1));
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id2));

  // After an error occurred, no further events should be sent.
  navigation_state.SetErrorOccurredInFrame(frame_id2);
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id1));
  EXPECT_FALSE(navigation_state.CanSendEvents(frame_id2));

  // Navigations to a network error page should be ignored.
  navigation_state.TrackFrame(frame_id2, frame_id1, GURL(), false, true, false);
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id1));
  EXPECT_FALSE(navigation_state.CanSendEvents(frame_id2));

  // However, when the frame navigates again, it should send events again.
  navigation_state.TrackFrame(frame_id2, frame_id1, url, false, false, false);
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id1));
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id2));
}

// Tests that no events are send for a not web-safe scheme.
TEST(FrameNavigationStateTest, WebSafeScheme) {
  FrameNavigationState navigation_state;
  const FrameNavigationState::FrameID frame_id0(-1, fake_rvh);
  const FrameNavigationState::FrameID frame_id1(23, fake_rvh);
  const GURL url("unsafe://www.google.com/");

  navigation_state.TrackFrame(frame_id1, frame_id0, url, true, false, false);
  EXPECT_FALSE(navigation_state.CanSendEvents(frame_id1));
}

// Test that parent frame IDs are tracked.
TEST(FrameNavigationStateTest, ParentFrameID) {
  FrameNavigationState navigation_state;
  const FrameNavigationState::FrameID frame_id0(-1, fake_rvh);
  const FrameNavigationState::FrameID frame_id1(23, fake_rvh);
  const FrameNavigationState::FrameID frame_id2(42, fake_rvh);
  const GURL url("http://www.google.com/");

  navigation_state.TrackFrame(frame_id1, frame_id0, url, true, false, false);
  navigation_state.TrackFrame(frame_id2, frame_id1, url, false, false, false);
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id1));
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id2));

  EXPECT_TRUE(navigation_state.GetParentFrameID(frame_id1) == frame_id0);
  EXPECT_TRUE(navigation_state.GetParentFrameID(frame_id2) == frame_id1);
}

// Test for <iframe srcdoc=""> frames.
TEST(FrameNavigationStateTest, SrcDoc) {
  FrameNavigationState navigation_state;
  const FrameNavigationState::FrameID frame_id0(-1, fake_rvh);
  const FrameNavigationState::FrameID frame_id1(23, fake_rvh);
  const FrameNavigationState::FrameID frame_id2(42, fake_rvh);
  const GURL url("http://www.google.com/");
  const GURL blank("about:blank");
  const GURL srcdoc("about:srcdoc");

  navigation_state.TrackFrame(frame_id1, frame_id0, url, true, false, false);
  navigation_state.TrackFrame(frame_id2, frame_id1, blank, false, false, true);
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id1));
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id2));

  EXPECT_TRUE(navigation_state.GetUrl(frame_id1) == url);
  EXPECT_TRUE(navigation_state.GetUrl(frame_id2) == srcdoc);

  EXPECT_TRUE(navigation_state.IsValidUrl(srcdoc));
}

}  // namespace extensions
