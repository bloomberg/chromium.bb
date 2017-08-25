// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_DIALOG_COORDINATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_DIALOG_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/clean/chrome/browser/ui/overlays/overlay_coordinator.h"

// A coordinator that displays dialog UI.  This class is meant to be subclassed,
// and subclasses are expected to implement the interface in the
// DialogCoordinator+Subclassing category.
@interface DialogCoordinator : OverlayCoordinator
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_DIALOG_COORDINATOR_H_
