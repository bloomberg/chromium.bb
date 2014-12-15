// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabs/media_indicator_button_cocoa.h"

#include <string>

#include "base/command_line.h"
#include "base/mac/scoped_nsobject.h"
#include "base/message_loop/message_loop.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// A simple target to confirm an action was invoked.
@interface MediaIndicatorButtonTestTarget : NSObject {
 @private
  int count_;
}
@property(readonly, nonatomic) int count;
- (void)incrementCount:(id)sender;
@end

@implementation MediaIndicatorButtonTestTarget
@synthesize count = count_;
- (void)incrementCount:(id)sender {
  ++count_;
}
@end

namespace {

class MediaIndicatorButtonTest : public CocoaTest {
 public:
  MediaIndicatorButtonTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        std::string("--") + switches::kEnableTabAudioMuting);

    // Create the MediaIndicatorButton and add it to a view.
    button_.reset([[MediaIndicatorButton alloc] init]);
    EXPECT_TRUE(button_ != nil);
    [[test_window() contentView] addSubview:button_.get()];

    // Initially the button is disabled and showing no indicator.
    EXPECT_EQ(TAB_MEDIA_STATE_NONE, [button_ showingMediaState]);
    EXPECT_FALSE([button_ isEnabled]);

    // Register target to be notified of clicks.
    base::scoped_nsobject<MediaIndicatorButtonTestTarget> clickTarget(
        [[MediaIndicatorButtonTestTarget alloc] init]);
    EXPECT_EQ(0, [clickTarget count]);
    [button_ setClickTarget:clickTarget withAction:@selector(incrementCount:)];

    // Transition to audio indicator mode, and expect button is enabled.
    [button_ transitionToMediaState:TAB_MEDIA_STATE_AUDIO_PLAYING];
    EXPECT_EQ(TAB_MEDIA_STATE_AUDIO_PLAYING, [button_ showingMediaState]);
    EXPECT_TRUE([button_ isEnabled]);

    // Click, and expect one click notification.
    EXPECT_EQ(0, [clickTarget count]);
    [button_ performClick:button_];
    EXPECT_EQ(1, [clickTarget count]);

    // Transition to audio muting mode, and expect button is still enabled.  A
    // click should result in another click notification.
    [button_ transitionToMediaState:TAB_MEDIA_STATE_AUDIO_MUTING];
    EXPECT_EQ(TAB_MEDIA_STATE_AUDIO_MUTING, [button_ showingMediaState]);
    EXPECT_TRUE([button_ isEnabled]);
    [button_ performClick:button_];
    EXPECT_EQ(2, [clickTarget count]);

    // Transition to capturing mode.  Now, the button is disabled since it
    // should only be drawing the indicator icon (i.e., there is nothing to
    // mute).  A click should NOT result in another click notification.
    [button_ transitionToMediaState:TAB_MEDIA_STATE_CAPTURING];
    EXPECT_EQ(TAB_MEDIA_STATE_CAPTURING, [button_ showingMediaState]);
    EXPECT_FALSE([button_ isEnabled]);
    [button_ performClick:button_];
    EXPECT_EQ(2, [clickTarget count]);
  }

  base::scoped_nsobject<MediaIndicatorButton> button_;
  base::MessageLoopForUI message_loop_;  // Needed for gfx::Animation.
};

TEST_VIEW(MediaIndicatorButtonTest, button_)

}  // namespace
