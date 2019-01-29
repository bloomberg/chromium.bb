// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/infobars/infobar_container_consumer.h"

// TODO(crbug.com/1372916): PLACEHOLDER Work in Progress protocol for the new
// InfobarUI.
// Manages the InfobarContainer presentation.
@protocol InfobarContainerPresenter

// Presents the InfobarContainerViewController.
- (void)presentInfobarContainer;

@end

// TODO(crbug.com/1372916): PLACEHOLDER Work in Progress class for the new
// InfobarUI. ViewController that contains all Infobars.
@interface InfobarContainerViewController
    : UIViewController <InfobarContainerConsumer>

// Delegate to present this ViewController.
@property(nonatomic, strong) id<InfobarContainerPresenter> presenter;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_VIEW_CONTROLLER_H_
