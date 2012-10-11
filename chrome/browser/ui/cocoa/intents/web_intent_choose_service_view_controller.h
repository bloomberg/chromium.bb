// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_CHOOSE_SERVICE_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_CHOOSE_SERVICE_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_view_controller.h"

// The choose service view shows a list of installed and suggested services.
@interface WebIntentChooseServiceViewController
    : NSViewController
     <WebIntentViewController> {
 @private
  scoped_nsobject<NSTextField> titleTextField_;
  scoped_nsobject<NSTextField> messageTextField_;
  scoped_nsobject<NSBox> separator_;
  scoped_nsobject<NSButton> showMoreServicesButton_;
  scoped_nsobject<NSImageView> moreServicesIconView_;
  // An array of WebIntentServiceRowViewController. One for each installed and
  // suggested service to show.
  scoped_nsobject<NSArray> rows_;
}

- (NSButton*)showMoreServicesButton;

- (void)setTitle:(NSString*)title;
- (void)setMessage:(NSString*)message;
- (void)setRows:(NSArray*)rows;
- (NSArray*)rows;

@end

#endif  // CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_CHOOSE_SERVICE_VIEW_CONTROLLER_H_
