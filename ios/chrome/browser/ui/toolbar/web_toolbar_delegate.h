// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_WEB_TOOLBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_WEB_TOOLBAR_DELEGATE_H_

#import "ios/chrome/browser/ui/toolbar/clean/toolbar_coordinator_delegate.h"

class ToolbarModelIOS;

namespace web {
class WebState;
}

// Delegate interface, to be implemented by the WebToolbarController's delegate.
@protocol WebToolbarDelegate<ToolbarCoordinatorDelegate>
// Returns the WebState.
- (web::WebState*)currentWebState;
@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_WEB_TOOLBAR_DELEGATE_H_
