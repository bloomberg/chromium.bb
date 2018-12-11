// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

namespace web {
class WebState;
}

@class TabModel;
@protocol ApplicationCommands;
@protocol InfobarPositioner;
@protocol SyncPresenter;

// Coordinator that owns and manages an InfobarContainer.
@interface InfobarContainerCoordinator : ChromeCoordinator

// TODO(crbug.com/892376): Stop passing TabModel and use WebStateList instead.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
                                  tabModel:(TabModel*)tabModel
    NS_DESIGNATED_INITIALIZER;
;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
    NS_UNAVAILABLE;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
    NS_UNAVAILABLE;

// The InfobarContainer View.
- (UIView*)view;

// Updates the InfobarContainer according to the positioner information.
- (void)updateInfobarContainer;

// YES if an Infobar is being presented for |webState|.
- (BOOL)isInfobarPresentingForWebState:(web::WebState*)webState;

// The dispatcher for this Coordinator.
@property(nonatomic, weak) id<ApplicationCommands> dispatcher;

// The delegate used to position the InfobarContainer in the view.
@property(nonatomic, weak) id<InfobarPositioner> positioner;

// The SyncPresenter delegate for this Coordinator.
@property(nonatomic, weak) id<SyncPresenter> syncPresenter;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_COORDINATOR_H_
