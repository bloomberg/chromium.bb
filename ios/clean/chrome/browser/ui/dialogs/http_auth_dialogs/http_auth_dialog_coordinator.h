// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_HTTP_AUTH_DIALOGS_HTTP_AUTH_DIALOG_COORDINATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_HTTP_AUTH_DIALOGS_HTTP_AUTH_DIALOG_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/clean/chrome/browser/ui/dialogs/dialog_coordinator.h"

@class HTTPAuthDialogRequest;

// A coordinator for the UI elements used to present HTTP authentication
// dialogs.
@interface HTTPAuthDialogCoordinator : DialogCoordinator

// Designated initializer to display a HTTP authentication dialog for |request|.
- (nullable instancetype)initWithRequest:(nonnull HTTPAuthDialogRequest*)request
    NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_HTTP_AUTH_DIALOGS_HTTP_AUTH_DIALOG_COORDINATOR_H_
