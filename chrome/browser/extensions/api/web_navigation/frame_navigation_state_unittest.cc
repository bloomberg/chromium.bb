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
  const FrameNavigationState::FrameID frame_id1(23, fake_rvh);
  const FrameNavigationState::FrameID frame_id2(42, fake_rvh);
  const GURL url1("http://www.google.com/");
  const GURL url2("http://mail.google.com/");

  // Create a main frame.
  EXPECT_FALSE(navigation_state.CanSendEvents(frame_id1));
  EXPECT_FALSE(navigation_state.IsValidFrame(frame_id1));
  navigation_state.TrackFrame(frame_id1, url1, true, false);
  navigation_state.SetNavigationCommitted(frame_id1);
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id1));
  EXPECT_TRUE(navigation_state.IsValidFrame(frame_id1));

  // Add a sub frame.
  EXPECT_FALSE(navigation_state.CanSendEvents(frame_id2));
  EXPECT_FALSE(navigation_state.IsValidFrame(frame_id2));
  navigation_state.TrackFrame(frame_id2, url2, false, false);
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
  const FrameNavigationState::FrameID frame_id(42, fake_rvh);
  const GURL url("http://www.google.com/");

  navigation_state.TrackFrame(frame_id, url, true, false);
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id));
  EXPECT_FALSE(navigation_state.GetErrorOccurredInFrame(frame_id));

  // After an error occurred, no further events should be sent.
  navigation_state.SetErrorOccurredInFrame(frame_id);
  EXPECT_FALSE(navigation_state.CanSendEvents(frame_id));
  EXPECT_TRUE(navigation_state.GetErrorOccurredInFrame(frame_id));

  // Navigations to a network error page should be ignored.
  navigation_state.TrackFrame(frame_id, GURL(), true, true);
  EXPECT_FALSE(navigation_state.CanSendEvents(frame_id));
  EXPECT_TRUE(navigation_state.GetErrorOccurredInFrame(frame_id));

  // However, when the frame navigates again, it should send events again.
  navigation_state.TrackFrame(frame_id, url, true, false);
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id));
  EXPECT_FALSE(navigation_state.GetErrorOccurredInFrame(frame_id));
}

// Tests that for a sub frame, no events are send after an error occurred, but
// before a new navigation happened in this frame.
TEST(FrameNavigationStateTest, ErrorStateFrame) {
  FrameNavigationState navigation_state;
  const FrameNavigationState::FrameID frame_id1(23, fake_rvh);
  const FrameNavigationState::FrameID frame_id2(42, fake_rvh);
  const GURL url("http://www.google.com/");

  navigation_state.TrackFrame(frame_id1, url, true, false);
  navigation_state.TrackFrame(frame_id2, url, false, false);
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id1));
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id2));

  // After an error occurred, no further events should be sent.
  navigation_state.SetErrorOccurredInFrame(frame_id2);
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id1));
  EXPECT_FALSE(navigation_state.CanSendEvents(frame_id2));

  // Navigations to a network error page should be ignored.
  navigation_state.TrackFrame(frame_id2, GURL(), false, true);
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id1));
  EXPECT_FALSE(navigation_state.CanSendEvents(frame_id2));

  // However, when the frame navigates again, it should send events again.
  navigation_state.TrackFrame(frame_id2, url, false, false);
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id1));
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id2));
}

// Tests that no events are send for a not web-safe scheme.
TEST(FrameNavigationStateTest, WebSafeScheme) {
  FrameNavigationState navigation_state;
  const FrameNavigationState::FrameID frame_id(23, fake_rvh);
  const GURL url("unsafe://www.google.com/");

  navigation_state.TrackFrame(frame_id, url, true, false);
  EXPECT_FALSE(navigation_state.CanSendEvents(frame_id));
}

}  // namespace extensions
