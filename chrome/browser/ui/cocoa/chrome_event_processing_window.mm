// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/chrome_event_processing_window.h"

#include "base/logging.h"
#import "chrome/browser/ui/cocoa/chrome_command_dispatcher_delegate.h"

@implementation ChromeEventProcessingWindow {
 @private
  base::scoped_nsobject<CommandDispatcher> commandDispatcher_;
  base::scoped_nsobject<ChromeCommandDispatcherDelegate>
      commandDispatcherDelegate_;
}

- (instancetype)initWithContentRect:(NSRect)contentRect
                          styleMask:(NSUInteger)windowStyle
                            backing:(NSBackingStoreType)bufferingType
                              defer:(BOOL)deferCreation {
  if ((self = [super initWithContentRect:contentRect
                               styleMask:windowStyle
                                 backing:bufferingType
                                   defer:deferCreation])) {
    commandDispatcher_.reset([[CommandDispatcher alloc] initWithOwner:self]);
    commandDispatcherDelegate_.reset(
        [[ChromeCommandDispatcherDelegate alloc] init]);
    [commandDispatcher_ setDelegate:commandDispatcherDelegate_];
  }
  return self;
}

- (BOOL)handleExtraKeyboardShortcut:(NSEvent*)event {
  return [commandDispatcherDelegate_ handleExtraKeyboardShortcut:event
                                                          window:self];
}

// CommandDispatchingWindow implementation.

- (BOOL)redispatchKeyEvent:(NSEvent*)event {
  return [commandDispatcher_ redispatchKeyEvent:event];
}

- (BOOL)defaultPerformKeyEquivalent:(NSEvent*)event {
  return [super performKeyEquivalent:event];
}

// NSWindow overrides.

- (BOOL)performKeyEquivalent:(NSEvent*)event {
  return [commandDispatcher_ performKeyEquivalent:event];
}

- (void)sendEvent:(NSEvent*)event {
  if (![commandDispatcher_ preSendEvent:event])
    [super sendEvent:event];
}

@end  // ChromeEventProcessingWindow
