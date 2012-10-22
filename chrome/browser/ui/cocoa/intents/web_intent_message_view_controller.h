// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_MESSAGE_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_MESSAGE_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_view_controller.h"

// The message view simply shows a title and message.
@interface WebIntentMessageViewController : WebIntentViewController {
 @private
  scoped_nsobject<NSTextField> titleTextField_;
  scoped_nsobject<NSTextField> messageTextField_;
}

- (void)setTitle:(NSString*)title;
- (void)setMessage:(NSString*)message;

@end

#endif  // CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_MESSAGE_VIEW_CONTROLLER_H_
