// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/chrome_overlay_window.h"

#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/crash_report/breakpad_helper.h"
#import "ios/chrome/browser/metrics/size_class_recorder.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/ui_util.h"

@interface ChromeOverlayWindow () {
  base::scoped_nsobject<SizeClassRecorder> _recorder;
}

// Initializes the size class recorder. On iPad iOS 9+, it starts tracking
// horizontal size class changes. Otherwise, it is a no-op.
- (void)initializeRecorderIfNeeded;

// Updates the Breakpad report with the current size class on iOS 8+. Otherwise,
// it's a no-op since size class doesn't exist.
- (void)updateBreakpad;

@end

@implementation ChromeOverlayWindow

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    // When not created via a nib, create the recorder immediately.
    [self initializeRecorderIfNeeded];
    [self updateBreakpad];
  }
  return self;
}

- (void)awakeFromNib {
  [super awakeFromNib];
  // When creating via a nib, wait to be awoken, as the size class is not
  // reliable before.
  [self initializeRecorderIfNeeded];
  [self updateBreakpad];
}

- (void)initializeRecorderIfNeeded {
  DCHECK(!_recorder);
  if (IsIPadIdiom()) {
    _recorder.reset([[SizeClassRecorder alloc]
        initWithHorizontalSizeClass:self.traitCollection.horizontalSizeClass]);
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(pageLoaded:)
               name:kTabModelTabDidFinishLoadingNotification
             object:nil];
  }
}

- (void)updateBreakpad {
  breakpad_helper::SetCurrentHorizontalSizeClass(
      self.traitCollection.horizontalSizeClass);
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

#pragma mark - UITraitEnvironment

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (previousTraitCollection.horizontalSizeClass !=
      self.traitCollection.horizontalSizeClass) {
    [_recorder
        horizontalSizeClassDidChange:self.traitCollection.horizontalSizeClass];
    [self updateBreakpad];
  }
}

#pragma mark - Notification handler

- (void)pageLoaded:(NSNotification*)notification {
  [_recorder pageLoadedWithHorizontalSizeClass:self.traitCollection
                                                   .horizontalSizeClass];
}

#pragma mark - Testing methods

- (void)unsetSizeClassRecorder {
  _recorder.reset();
}

@end
