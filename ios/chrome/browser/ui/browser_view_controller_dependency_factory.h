// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_CONTROLLER_DEPENDENCY_FACTORY_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_CONTROLLER_DEPENDENCY_FACTORY_H_

#import <UIKit/UIKit.h>

@class AlertCoordinator;
@protocol ApplicationCommands;
@protocol BrowserCommands;
@class KeyCommandsProvider;
@class MessageBubbleView;
@class PKPass;
@class PKAddPassesViewController;
@class TabModel;
@protocol Toolbar;
class ToolbarModelDelegateIOS;
class ToolbarModelIOS;
@protocol UrlLoader;
@protocol WebToolbarDelegate;

namespace infobars {
class InfoBarManager;
}

namespace ios {
class ChromeBrowserState;
}

// Creates helper objects needed by BrowserViewController.
@interface BrowserViewControllerDependencyFactory : NSObject

// Creates a new factory backed by |browserState|. This must be the same browser
// state provided to BrowserViewController (and like BVC, this is a weak
// reference).
- (id)initWithBrowserState:(ios::ChromeBrowserState*)browserState;

// Creates a new PassKit view controller to display |pass|.
- (PKAddPassesViewController*)newPassKitViewControllerForPass:(PKPass*)pass;

// Displays a PassKit error infobar on the current tab.
- (void)showPassKitErrorInfoBarForManager:
    (infobars::InfoBarManager*)infoBarManager;

- (ToolbarModelIOS*)newToolbarModelIOSWithDelegate:
    (ToolbarModelDelegateIOS*)delegate;

- (id<Toolbar>)
newToolbarControllerWithDelegate:(id<WebToolbarDelegate>)delegate
                       urlLoader:(id<UrlLoader>)urlLoader
                      dispatcher:
                          (id<ApplicationCommands, BrowserCommands>)dispatcher;

// Returns a new keyboard commands coordinator to handle keyboard commands.
- (KeyCommandsProvider*)newKeyCommandsProvider;

- (AlertCoordinator*)alertCoordinatorWithTitle:(NSString*)title
                                       message:(NSString*)message
                                viewController:
                                    (UIViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_CONTROLLER_DEPENDENCY_FACTORY_H_
