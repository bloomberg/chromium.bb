// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_JAVA_SCRIPT_DIALOG_COORDINATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_JAVA_SCRIPT_DIALOG_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/clean/chrome/browser/ui/dialogs/dialog_coordinator.h"

@class JavaScriptDialogRequest;

// An overlay coordinator for the UI elements used to present JavaScript
// dialogs.
@interface JavaScriptDialogCoordinator : DialogCoordinator

// Designated initializer to display a JavaScript dialog for |request|.
- (instancetype)initWithRequest:(JavaScriptDialogRequest*)request
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_JAVA_SCRIPT_DIALOG_COORDINATOR_H_
