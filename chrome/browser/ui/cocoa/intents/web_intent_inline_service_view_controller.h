// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_INLINE_SERVICE_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_INLINE_SERVICE_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_view_controller.h"
#include "googleurl/src/gurl.h"

namespace content {
class WebContents;
}
class WebIntentInlineDispositionDelegate;
class WebIntentPickerCocoa;

// The inline service view shows a web view for a given service.
@interface WebIntentInlineServiceViewController : WebIntentViewController {
 @private
  WebIntentPickerCocoa* picker_;  // weak
  scoped_nsobject<NSImageView> serviceIconImageView_;
  scoped_nsobject<NSTextField> serviceNameTextField_;
  scoped_nsobject<NSButton> chooseServiceButton_;
  scoped_nsobject<NSBox> separator_;
  scoped_nsobject<NSView> webContentView_;
  GURL serviceURL_;
  // Order here is important, as webContents_ may send messages to
  // delegate_ when it gets destroyed.
  scoped_ptr<content::WebContents> webContents_;
  scoped_ptr<WebIntentInlineDispositionDelegate> delegate_;
}

- (id)initWithPicker:(WebIntentPickerCocoa*)picker;

- (NSButton*)chooseServiceButton;
- (content::WebContents*)webContents;

// Gets the minimum size of the web view shown inside picker dialog.
- (NSSize)minimumInlineWebViewSizeForFrame:(NSRect)frame;

- (void)setServiceName:(NSString*)serviceName;
- (void)setServiceIcon:(NSImage*)serviceIcon;
- (void)setServiceURL:(const GURL&)url;
- (void)setChooseServiceButtonHidden:(BOOL)isHidden;

@end

#endif  // CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_INLINE_SERVICE_VIEW_CONTROLLER_H_
