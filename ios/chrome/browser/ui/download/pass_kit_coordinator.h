// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DOWNLOAD_PASS_KIT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_DOWNLOAD_PASS_KIT_COORDINATOR_H_

#import <PassKit/PassKit.h>

#import "ios/chrome/browser/chrome_coordinator.h"
#import "ios/chrome/browser/download/pass_kit_tab_helper_delegate.h"

namespace web {
class WebState;
}  // namespace web

// Coordinates presentation of "Add pkpass UI" and "failed to add pkpass UI".
@interface PassKitCoordinator : ChromeCoordinator<PassKitTabHelperDelegate>

// Must be set before calling |start| method. Set to null when stop method is
// called or web state is destroyed.
@property(nonatomic, nonnull) web::WebState* webState;

// If the PKPass is a valid pass, then the coordinator will present the "Add
// pkpass UI". Otherwise, the coordinator will present the "failed to add
// pkpass UI". Is set to null when the stop method is called.
@property(nonatomic, nullable) PKPass* pass;

@end

#endif  // IOS_CHROME_BROWSER_UI_DOWNLOAD_PASS_KIT_COORDINATOR_H_
