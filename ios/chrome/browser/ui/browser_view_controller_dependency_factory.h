// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_CONTROLLER_DEPENDENCY_FACTORY_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_CONTROLLER_DEPENDENCY_FACTORY_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/ui/tabs/tab_strip_controller.h"

@class AlertCoordinator;
@protocol ApplicationCommands;
@protocol BrowserCommands;
@class KeyCommandsProvider;
@class MessageBubbleView;
@class PKPass;
@class PKAddPassesViewController;
@protocol PreloadProvider;
@class TabModel;
class ToolbarModelDelegateIOS;
class ToolbarModelIOS;
@protocol UrlLoader;
@class WebToolbarController;
@protocol WebToolbarDelegate;

namespace infobars {
class InfoBarManager;
}

namespace ios {
class ChromeBrowserState;
}

// The category for all messages presented by the
// BrowserViewControllerDependencyFactory via |showSnackbarWithMessage:|.
extern NSString* const kBrowserViewControllerSnackbarCategory;

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

- (TabStripController*)
newTabStripControllerWithTabModel:(TabModel*)model
                       dispatcher:
                           (id<ApplicationCommands, BrowserCommands>)dispatcher;

- (ToolbarModelIOS*)newToolbarModelIOSWithDelegate:
    (ToolbarModelDelegateIOS*)delegate;

- (WebToolbarController*)
newWebToolbarControllerWithDelegate:(id<WebToolbarDelegate>)delegate
                          urlLoader:(id<UrlLoader>)urlLoader
                    preloadProvider:(id<PreloadProvider>)preload
                         dispatcher:(id<ApplicationCommands, BrowserCommands>)
                                        dispatcher;

// Returns a new keyboard commands coordinator to handle keyboard commands.
- (KeyCommandsProvider*)newKeyCommandsProvider;

- (void)showSnackbarWithMessage:(NSString*)message;

- (AlertCoordinator*)alertCoordinatorWithTitle:(NSString*)title
                                       message:(NSString*)message
                                viewController:
                                    (UIViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_CONTROLLER_DEPENDENCY_FACTORY_H_
