// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TABS_TAB_AUDIO_INDICATOR_VIEW_MAC_H_
#define CHROME_BROWSER_UI_COCOA_TABS_TAB_AUDIO_INDICATOR_VIEW_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"

namespace content {
class WebContents;
}
class TabAudioIndicator;
class TabAudioIndicatorDelegateMac;

// A view that draws an audio indicator on top of a favicon.
@interface TabAudioIndicatorViewMac : NSView {
 @private
  scoped_ptr<TabAudioIndicator> tabAudioIndicator_;
  scoped_ptr<TabAudioIndicatorDelegateMac> delegate_;
}

- (void)setIsPlayingAudio:(BOOL)isPlayingAudio;

- (void)setBackgroundImage:(NSImage*)backgroundImage;

- (BOOL)isAnimating;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TABS_TAB_AUDIO_INDICATOR_VIEW_MAC_H_
