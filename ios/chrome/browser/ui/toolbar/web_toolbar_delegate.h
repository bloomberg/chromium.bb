// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_WEB_TOOLBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_WEB_TOOLBAR_DELEGATE_H_

#import <Foundation/Foundation.h>

class ToolbarModelIOS;

namespace web {
class WebState;
}

// Delegate interface, to be implemented by the WebToolbarController's delegate.
@protocol WebToolbarDelegate<NSObject>
@required
// Called when the location bar gains keyboard focus.
- (IBAction)locationBarDidBecomeFirstResponder:(id)sender;
// Called when the location bar loses keyboard focus.
- (IBAction)locationBarDidResignFirstResponder:(id)sender;
// Called when the location bar receives a key press.
- (IBAction)locationBarBeganEdit:(id)sender;
// Returns the WebState.
- (web::WebState*)currentWebState;
- (ToolbarModelIOS*)toolbarModelIOS;
@optional
// Called before the toolbar screenshot gets updated.
- (void)willUpdateToolbarSnapshot;
@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_WEB_TOOLBAR_DELEGATE_H_
