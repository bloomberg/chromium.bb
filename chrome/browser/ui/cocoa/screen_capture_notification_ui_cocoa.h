// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/screen_capture_notification_ui.h"

// Controller for the screen capture notification window which allows the user
// to quickly stop screen capturing.
@interface ScreenCaptureNotificationController
    : NSWindowController<NSWindowDelegate> {
 @private
  base::Closure stop_callback_;
  base::scoped_nsobject<NSButton> stopButton_;
  base::scoped_nsobject<NSButton> minimizeButton_;
}

- (id)initWithCallback:(const base::Closure&)stop_callback
                  text:(const base::string16&)text;
- (void)stopSharing:(id)sender;
- (void)minimize:(id)sender;

@end

class ScreenCaptureNotificationUICocoa : public ScreenCaptureNotificationUI {
 public:
  explicit ScreenCaptureNotificationUICocoa(const base::string16& text);
  virtual ~ScreenCaptureNotificationUICocoa();

  // ScreenCaptureNotificationUI interface.
  virtual gfx::NativeViewId OnStarted(const base::Closure& stop_callback)
      OVERRIDE;

 private:
  friend class ScreenCaptureNotificationUICocoaTest;

  const base::string16 text_;
  base::scoped_nsobject<ScreenCaptureNotificationController> windowController_;

  DISALLOW_COPY_AND_ASSIGN(ScreenCaptureNotificationUICocoa);
};
