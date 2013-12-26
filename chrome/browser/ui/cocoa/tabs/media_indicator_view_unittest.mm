// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabs/media_indicator_view.h"

#include "base/mac/scoped_nsobject.h"
#include "base/message_loop/message_loop.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class MediaIndicatorViewTest : public CocoaTest {
 public:
  MediaIndicatorViewTest() {
    view_.reset([[MediaIndicatorView alloc] init]);
    EXPECT_TRUE(view_ != nil);
    EXPECT_EQ(TAB_MEDIA_STATE_NONE, [view_ animatingMediaState]);

    [[test_window() contentView] addSubview:view_.get()];

    [view_ updateIndicator:TAB_MEDIA_STATE_AUDIO_PLAYING];
    EXPECT_EQ(TAB_MEDIA_STATE_AUDIO_PLAYING, [view_ animatingMediaState]);
  }

  base::scoped_nsobject<MediaIndicatorView> view_;
  base::MessageLoopForUI message_loop_;  // Needed for gfx::Animation.
};

TEST_VIEW(MediaIndicatorViewTest, view_)

}  // namespace
