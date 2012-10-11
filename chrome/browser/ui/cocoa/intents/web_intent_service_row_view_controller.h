// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_SERVICE_ROW_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_SERVICE_ROW_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>
#include <string>

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_view_controller.h"

// The service row view shows a single suggested or installed service. It
// contains a button to select the service.
@interface WebIntentServiceRowViewController
    : NSViewController
     <WebIntentViewController> {
 @private
  scoped_nsobject<NSTextField> titleTextField_;
  scoped_nsobject<NSButton> titleLinkButton_;
  scoped_nsobject<NSButton> selectButton_;
  scoped_nsobject<NSImageView> iconView_;
  scoped_nsobject<NSView> ratingsView_;
}

// Initializes a suggested service row. The title is a clickable link.
- (id)initSuggestedServiceRowWithTitle:(NSString*)title
                                  icon:(NSImage*)icon
                                rating:(double)rating;

// Initializes a installed service row with just a title and a select button.
- (id)initInstalledServiceRowWithTitle:(NSString*)title
                                  icon:(NSImage*)icon;

- (NSButton*)titleLinkButton;
- (NSButton*)selectButton;

@end

#endif  // CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_SERVICE_ROW_VIEW_CONTROLLER_H_
