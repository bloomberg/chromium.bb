// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_delegate.h"
#import "ios/chrome/browser/ui/infobars/infobar_ui_delegate.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_delegate.h"

@protocol InfobarBadgeUIDelegate;

@class InfobarBannerTransitionDriver;
@class InfobarBannerViewController;
@class InfobarModalTransitionDriver;
@class InfobarModalViewController;

namespace ios {
class ChromeBrowserState;
}
namespace infobars {
class InfoBarDelegate;
}

// Must be subclassed. Defines common behavior for all Infobars.
@interface InfobarCoordinator : ChromeCoordinator <InfobarUIDelegate,
                                                   InfobarBannerDelegate,
                                                   InfobarModalDelegate>

- (instancetype)initWithInfoBarDelegate:
    (infobars::InfoBarDelegate*)infoBarDelegate NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
    NS_UNAVAILABLE;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
    NS_UNAVAILABLE;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Present the InfobarBanner using |self.baseViewController|.
- (void)presentInfobarBannerAnimated:(BOOL)animated
                          completion:(ProceduralBlock)completion;

// Present the InfobarModal using |self.baseViewController|.
- (void)presentInfobarModal;

// Dismisses the InfobarBanner after the user is no longer interacting with it.
// e.g. No in progress touch gestures,etc. The dismissal will be animated.
- (void)dismissInfobarBannerAfterInteraction;

// Dismisses the InfobarBanner immediately, if none is being presented
// |completion| will still run.
- (void)dismissInfobarBannerAnimated:(BOOL)animated
                          completion:(void (^)())completion;

// YES if the Coordinator has been started.
@property(nonatomic, assign) BOOL started;

// BannerViewController owned by this Coordinator. Can be nil.
@property(nonatomic, strong, readonly) UIViewController* bannerViewController;

// ModalViewController owned by this Coordinator. Can be nil.
@property(nonatomic, strong, readonly) UIViewController* modalViewController;

// Handles any followup actions to Infobar UI events.
@property(nonatomic, weak) id<InfobarBadgeUIDelegate> badgeDelegate;

// The ChromeBrowserState owned by the Coordinator.
// TODO(crbug.com/927064): Once we create the coordinators in the UI Hierarchy
// browserState will be set on init.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;

// The ChromeBrowserState owned by the Coordinator.
// TODO(crbug.com/927064): Once we create the coordinators in the UI Hierarchy
// baseViewController will be set on init.
@property(nonatomic, weak) UIViewController* baseViewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_COORDINATOR_H_
