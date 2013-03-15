// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabs/tab_audio_indicator_view_mac.h"

#include "base/message_loop.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"

class TabAudioIndicatorViewMacTest : public CocoaTest {
 protected:
  TabAudioIndicatorViewMacTest() {
    scoped_nsobject<TabAudioIndicatorViewMac> view(
        [[TabAudioIndicatorViewMac alloc]
            initWithFrame:NSMakeRect(0, 0, 16, 16)]);
    view_ = view.get();
    [[test_window() contentView] addSubview:view_];

    scoped_nsobject<NSImage> image(
        [[NSImage alloc] initWithSize:NSMakeSize(16, 16)]);
    [image lockFocus];
    NSRectFill(NSMakeRect(0, 0, 16, 16));
    [image unlockFocus];

    [view_ setBackgroundImage:image];
    [view_ setIsPlayingAudio:YES];
  }

  TabAudioIndicatorViewMac* view_;
  MessageLoopForUI message_loop_;  // Needed for ui::LinearAnimation.

 private:
  DISALLOW_COPY_AND_ASSIGN(TabAudioIndicatorViewMacTest);
};

TEST_VIEW(TabAudioIndicatorViewMacTest, view_)
