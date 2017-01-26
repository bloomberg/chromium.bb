// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_PRIVATE_H_
#define IOS_CHROME_BROWSER_TABS_TAB_PRIVATE_H_

#include "ios/net/request_tracker.h"

namespace web {
class WebStateImpl;
}

// Exposed private methods for testing purpose.
@interface Tab ()

- (OpenInController*)openInController;
- (void)closeThisTab;
- (CRWSessionEntry*)currentSessionEntry;
- (void)setShouldObserveInfoBarManager:(BOOL)shouldObserveInfoBarManager;
- (void)setShouldObserveFaviconChanges:(BOOL)shouldObserveFaviconChanges;

@end

@interface Tab (TestingSupport)

// Replaces the existing web state. This method should be called once
// right after init and before any call to |view|.
// TODO(crbug.com/620465): change to std::unique_ptr<web::WebState> once
// Tab no longer uses private //ios/web APIs.
- (void)replaceWebState:(std::unique_ptr<web::WebStateImpl>)webState;

// Replaces the existing |externalAppLauncher_|.
- (void)replaceExternalAppLauncher:(id)externalAppLauncher;

- (TabModel*)parentTabModel;

- (FormInputAccessoryViewController*)inputAccessoryViewController;

@end

#endif  // IOS_CHROME_BROWSER_TABS_TAB_PRIVATE_H_
