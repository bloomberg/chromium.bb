// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/system_notification_fullscreen_disabler.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/scoped_fullscreen_disabler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SystemNotificationFullscreenDisabler () {
  // The disabler created when VoiceOver is enabled.
  std::unique_ptr<ScopedFullscreenDisabler> _voiceOverDisabler;
  // The disabler created when the keyboard is visible.
  std::unique_ptr<ScopedFullscreenDisabler> _keyboardDisabler;
}
// The FullscreenController being enabled/disabled for VoiceOver.
@property(nonatomic, readonly, nonnull) FullscreenController* controller;
// Creates or destroys |_voiceOverDisabler| depending on whether VoiceOver is
// enabled.
- (void)voiceOverStatusChanged;
// Called when the keyboard is shown/hidden to reset |_keyboardDisabler|.
- (void)keyboardWillShow;
- (void)keyboardDidHide;
@end

@implementation SystemNotificationFullscreenDisabler
@synthesize controller = _controller;

- (instancetype)initWithController:(FullscreenController*)controller {
  if (self = [super init]) {
    _controller = controller;
    DCHECK(_controller);
    // Register for VoiceOVer status change notifications.  The notification
    // name has been updated in iOS 11.
    if (@available(iOS 11, *)) {
      [[NSNotificationCenter defaultCenter]
          addObserver:self
             selector:@selector(voiceOverStatusChanged)
                 name:UIAccessibilityVoiceOverStatusDidChangeNotification
               object:nil];
    } else {
      [[NSNotificationCenter defaultCenter]
          addObserver:self
             selector:@selector(voiceOverStatusChanged)
                 name:UIAccessibilityVoiceOverStatusChanged
               object:nil];
    }
    // Create a disabler if VoiceOver is enabled.
    if (UIAccessibilityIsVoiceOverRunning()) {
      _voiceOverDisabler =
          base::MakeUnique<ScopedFullscreenDisabler>(_controller);
    }
    // Regsiter for keyboard visibility notifications.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(keyboardWillShow)
               name:UIKeyboardWillShowNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(keyboardDidHide)
               name:UIKeyboardDidHideNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  // |-disconnect| should be called before deallocation.
  DCHECK(!_controller);
}

#pragma mark Public

- (void)disconnect {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  _controller = nullptr;
}

#pragma mark Private

- (void)voiceOverStatusChanged {
  _voiceOverDisabler =
      UIAccessibilityIsVoiceOverRunning()
          ? base::MakeUnique<ScopedFullscreenDisabler>(self.controller)
          : nullptr;
}

- (void)keyboardWillShow {
  _keyboardDisabler =
      base::MakeUnique<ScopedFullscreenDisabler>(self.controller);
}

- (void)keyboardDidHide {
  _keyboardDisabler = nullptr;
}

@end
