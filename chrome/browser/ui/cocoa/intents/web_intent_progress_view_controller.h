// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_PROGRESS_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_PROGRESS_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_view_controller.h"

@class SpinnerProgressIndicator;

// The progress view shows a progress indicator and a label underneath it. The
// label is made by joining the title and the message.
@interface WebIntentProgressViewController : WebIntentViewController {
 @private
  scoped_nsobject<NSString> title_;
  scoped_nsobject<NSString> message_;
  scoped_nsobject<NSTextField> messageTextField_;
  scoped_nsobject<SpinnerProgressIndicator> progressIndicator_;
}

- (SpinnerProgressIndicator*)progressIndicator;
- (void)setTitle:(NSString*)title;
- (void)setMessage:(NSString*)message;
- (void)setPercentDone:(int)percent;

@end

#endif  // CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_PROGRESS_VIEW_CONTROLLER_H_
