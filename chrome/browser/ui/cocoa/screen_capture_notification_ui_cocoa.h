// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include <string>

#include "base/callback.h"
#include "base/string16.h"
#include "chrome/browser/ui/screen_capture_notification_ui.h"

// Controller for the screen capture notification window which allows the user
// to quickly stop screen capturing.
@interface ScreenCaptureNotificationController : NSWindowController {
 @private
  base::Closure stop_callback_;
  string16 title_;
  IBOutlet NSTextField* statusField_;
  IBOutlet NSButton* stopButton_;
}

- (id)initWithCallback:(const base::Closure&)stop_callback
                 title:(const string16&)title;
- (IBAction)stopSharing:(id)sender;
@end

// A floating window with a custom border. The custom border and background
// content is defined by DisconnectView. Declared here so that it can be
// instantiated via a xib.
@interface ScreenCaptureNotificationWindow : NSWindow
@end

// The custom background/border for the ScreenCaptureNotificationWindow.
// Declared here so that it can be instantiated via a xib.
@interface ScreenCaptureNotificationView : NSView
@end
