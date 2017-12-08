// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/voice_over_fullscreen_disabler.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/scoped_fullscreen_disabler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface VoiceOverFullscreenDisabler () {
  // The disabler created when VoiceOver is enabled.
  std::unique_ptr<ScopedFullscreenDisabler> _disabler;
}
// The FullscreenController being enabled/disabled for VoiceOver.
@property(nonatomic, readonly, nonnull) FullscreenController* controller;
// Creates or destroys |_disabler| depending on whether VoiceOver is enabled.
- (void)voiceOverStatusChanged;
@end

@implementation VoiceOverFullscreenDisabler
@synthesize controller = _controller;

- (instancetype)initWithController:(FullscreenController*)controller {
  if (self = [super init]) {
    _controller = controller;
    DCHECK(_controller);
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
    if (UIAccessibilityIsVoiceOverRunning())
      _disabler = base::MakeUnique<ScopedFullscreenDisabler>(_controller);
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
  _disabler = UIAccessibilityIsVoiceOverRunning()
                  ? base::MakeUnique<ScopedFullscreenDisabler>(_controller)
                  : nullptr;
}

@end
