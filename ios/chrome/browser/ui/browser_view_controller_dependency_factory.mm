// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view_controller_dependency_factory.h"

#import <PassKit/PassKit.h>

#include "base/ios/ios_util.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/infobars/core/simple_alert_infobar_delegate.h"
#include "components/strings/grit/components_strings.h"
#include "components/toolbar/toolbar_model_impl.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/activity_services/activity_service_controller.h"
#import "ios/chrome/browser/ui/activity_services/share_protocol.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/key_commands_provider.h"
#import "ios/chrome/browser/ui/preload_controller.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_model_delegate_ios.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_model_impl_ios.h"
#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

NSString* const kBrowserViewControllerSnackbarCategory =
    @"BrowserViewControllerSnackbarCategory";

@implementation BrowserViewControllerDependencyFactory

- (id)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  self = [super init];
  if (self) {
    browserState_ = browserState;
  }
  return self;
}

- (id<ShareProtocol>)shareControllerInstance {
  return [ActivityServiceController sharedInstance];
}

- (PKAddPassesViewController*)newPassKitViewControllerForPass:(PKPass*)pass {
  return [[PKAddPassesViewController alloc] initWithPass:pass];
}

- (void)showPassKitErrorInfoBarForManager:
    (infobars::InfoBarManager*)infoBarManager {
  DCHECK(infoBarManager);
  SimpleAlertInfoBarDelegate::Create(
      infoBarManager,
      infobars::InfoBarDelegate::SHOW_PASSKIT_INFOBAR_ERROR_DELEGATE, nullptr,
      l10n_util::GetStringUTF16(IDS_IOS_GENERIC_PASSKIT_ERROR), true);
}

- (PreloadController*)newPreloadController {
  return [[PreloadController alloc] initWithBrowserState:browserState_];
}

- (TabStripController*)newTabStripControllerWithTabModel:(TabModel*)model {
  TabStrip::Style style = TabStrip::kStyleDark;
  if (browserState_ && browserState_->IsOffTheRecord())
    style = TabStrip::kStyleIncognito;
  return [[TabStripController alloc] initWithTabModel:model style:style];
}

- (ToolbarModelIOS*)newToolbarModelIOSWithDelegate:
    (ToolbarModelDelegateIOS*)delegate {
  return new ToolbarModelImplIOS(delegate);
}

- (WebToolbarController*)
newWebToolbarControllerWithDelegate:(id<WebToolbarDelegate>)delegate
                          urlLoader:(id<UrlLoader>)urlLoader
                    preloadProvider:(id<PreloadProvider>)preload {
  return [[WebToolbarController alloc] initWithDelegate:delegate
                                              urlLoader:urlLoader
                                           browserState:browserState_
                                        preloadProvider:preload];
}

- (KeyCommandsProvider*)newKeyCommandsProvider {
  return [[KeyCommandsProvider alloc] init];
}

- (void)showSnackbarWithMessage:(NSString*)text {
  MDCSnackbarMessage* message = [MDCSnackbarMessage messageWithText:text];
  message.accessibilityLabel = text;
  message.duration = 2.0;
  message.category = kBrowserViewControllerSnackbarCategory;
  [MDCSnackbarManager showMessage:message];
}

- (AlertCoordinator*)alertCoordinatorWithTitle:(NSString*)title
                                       message:(NSString*)message
                                viewController:
                                    (UIViewController*)viewController {
  AlertCoordinator* alertCoordinator = [[[AlertCoordinator alloc]
      initWithBaseViewController:viewController
                           title:title
                         message:message] autorelease];
  [alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_OK)
                              action:nil
                               style:UIAlertActionStyleDefault];
  return alertCoordinator;
}

@end
