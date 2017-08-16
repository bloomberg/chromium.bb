// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_WEB_CONTENTS_WEB_COORDINATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_WEB_CONTENTS_WEB_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/browser_coordinator.h"

namespace web {
class WebState;
}

// A coordinator for a UI element that displays the web view associated with
// |webState|.
@interface WebCoordinator : BrowserCoordinator

// The web state this coordinator is displaying.
@property(nonatomic, assign) web::WebState* webState;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_WEB_CONTENTS_WEB_COORDINATOR_H_
