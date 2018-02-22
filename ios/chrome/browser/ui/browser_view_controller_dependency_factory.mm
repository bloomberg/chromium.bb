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
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/browser_view_controller_helper.h"
#import "ios/chrome/browser/ui/key_commands_provider.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_controller_base_feature.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_model_delegate_ios.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BrowserViewControllerDependencyFactory {
  ios::ChromeBrowserState* browserState_;
  WebStateList* webStateList_;
}

- (id)initWithBrowserState:(ios::ChromeBrowserState*)browserState
              webStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    browserState_ = browserState;
    webStateList_ = webStateList;
  }
  return self;
}

- (PKAddPassesViewController*)newPassKitViewControllerForPass:(PKPass*)pass {
  return [[PKAddPassesViewController alloc] initWithPass:pass];
}

- (void)showPassKitErrorInfoBarForManager:
    (infobars::InfoBarManager*)infoBarManager {
  DCHECK(infoBarManager);
  SimpleAlertInfoBarDelegate::Create(
      infoBarManager,
      infobars::InfoBarDelegate::SHOW_PASSKIT_ERROR_INFOBAR_DELEGATE_IOS,
      nullptr, l10n_util::GetStringUTF16(IDS_IOS_GENERIC_PASSKIT_ERROR), true);
}

- (BrowserViewControllerHelper*)newBrowserViewControllerHelper {
  return [[BrowserViewControllerHelper alloc] init];
}

- (KeyCommandsProvider*)newKeyCommandsProvider {
  return [[KeyCommandsProvider alloc] init];
}

- (AlertCoordinator*)alertCoordinatorWithTitle:(NSString*)title
                                       message:(NSString*)message
                                viewController:
                                    (UIViewController*)viewController {
  AlertCoordinator* alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:viewController
                                                     title:title
                                                   message:message];
  [alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_OK)
                              action:nil
                               style:UIAlertActionStyleDefault];
  return alertCoordinator;
}

@end
