// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_VOICE_OVER_FULLSCREEN_DISABLER_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_VOICE_OVER_FULLSCREEN_DISABLER_H_

#import <UIKit/UIKit.h>

class FullscreenController;

// Helper class that handles disabling fullscreen while VoiceOver is enabled.
@interface VoiceOverFullscreenDisabler : NSObject

- (nullable instancetype)initWithController:
    (nonnull FullscreenController*)controller NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)init NS_UNAVAILABLE;

// Stops observing VoiceOver notifications.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_VOICE_OVER_FULLSCREEN_DISABLER_H_
