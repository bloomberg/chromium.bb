// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_START_VOICE_SEARCH_COMMAND_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_START_VOICE_SEARCH_COMMAND_H_

#import <UIKit/UIKit.h>

// Command sent to start a voice search, optionally including the view from
// which the voice search present and dismiss animations will occur.
@interface StartVoiceSearchCommand : NSObject

- (instancetype)initWithOriginView:(UIView*)view NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, readonly) UIView* originView;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_START_VOICE_SEARCH_COMMAND_H_
