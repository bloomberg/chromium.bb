// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"

#include "base/format_macros.h"
#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ios_internal {

AlertCoordinator* ErrorCoordinator(NSError* error,
                                   ProceduralBlock dismissAction,
                                   UIViewController* viewController) {
  DCHECK(error);

  AlertCoordinator* alertCoordinator =
      ErrorCoordinatorNoItem(error, viewController);

  NSString* okButtonLabel = l10n_util::GetNSString(IDS_OK);
  [alertCoordinator addItemWithTitle:okButtonLabel
                              action:dismissAction
                               style:UIAlertActionStyleDefault];

  [alertCoordinator setCancelAction:dismissAction];

  return alertCoordinator;
}

AlertCoordinator* ErrorCoordinatorNoItem(NSError* error,
                                         UIViewController* viewController) {
  DCHECK(error);

  NSString* title = l10n_util::GetNSString(
      IDS_IOS_SYNC_AUTHENTICATION_ERROR_ALERT_VIEW_TITLE);
  NSString* errorMessage;
  if ([NSURLErrorDomain isEqualToString:error.domain] &&
      error.code == kCFURLErrorCannotConnectToHost) {
    errorMessage =
        l10n_util::GetNSString(IDS_IOS_SYNC_ERROR_INTERNET_DISCONNECTED);
  } else if ([error.userInfo objectForKey:NSLocalizedDescriptionKey]) {
    errorMessage = [NSString stringWithFormat:@"%@ (%@ %" PRIdNS ")",
                                              error.localizedDescription,
                                              error.domain, error.code];
  } else {
    // If the error has no NSLocalizedDescriptionKey in its user info, then
    // |error.localizedDescription| contains the error domain and code.
    errorMessage = error.localizedDescription;
  }
  AlertCoordinator* alertCoordinator = [[[AlertCoordinator alloc]
      initWithBaseViewController:viewController
                           title:title
                         message:errorMessage] autorelease];
  return alertCoordinator;
}

}  // namespace ios_internal
