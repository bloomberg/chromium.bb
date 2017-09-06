// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SAD_TAB_SAD_TAB_LEGACY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SAD_TAB_SAD_TAB_LEGACY_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/web/sad_tab_tab_helper_delegate.h"

@class CommandDispatcher;

namespace web {
class WebState;
}

// Coordinator that displays a SadTab view.
@interface SadTabLegacyCoordinator : NSObject<SadTabTabHelperDelegate>

// The dispatcher for this Coordinator.
@property(nonatomic, weak) CommandDispatcher* dispatcher;

// The web state this SadTabLegacyCoordinator is handling. This might change
// during the life of the Coordinator and might be null.
@property(nonatomic, assign) web::WebState* webState;

@end

#endif  // IOS_CHROME_BROWSER_UI_SAD_TAB_SAD_TAB_LEGACY_COORDINATOR_H_
