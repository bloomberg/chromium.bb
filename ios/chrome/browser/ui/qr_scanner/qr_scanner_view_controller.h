// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_QR_SCANNER_QR_SCANNER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_QR_SCANNER_QR_SCANNER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/ui/qr_scanner/camera_controller.h"

@protocol QRScannerViewControllerDelegate

// Called when the scanner detects a valid code. Camera recording is stopped
// when a result is scanned and the QRScannerViewController is dismissed. This
// function is called when the dismissal completes. A valid code is any
// non-empty string. If |load| is YES, the scanned code was of a type which can
// only encode digits, and the delegate can load the result immediately, instead
// of prompting the user to confirm the result.
- (void)receiveQRScannerResult:(NSString*)qrScannerResult
               loadImmediately:(BOOL)load;

@end

@interface QRScannerViewController : UIViewController<CameraControllerDelegate>

// The delegate which receives the scanned result after the view controller is
// dismissed.
@property(nonatomic, assign) id<QRScannerViewControllerDelegate> delegate;

- (instancetype)initWithDelegate:(id<QRScannerViewControllerDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)name
                         bundle:(NSBundle*)bundle NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Returns a view controller to be presented based on the camera state. Returns
// |self| if the camera is available or an appropriate UIAlertController if
// there was an error loading the camera.
- (UIViewController*)getViewControllerToPresent;

@end

#endif  // IOS_CHROME_BROWSER_UI_QR_SCANNER_QR_SCANNER_VIEW_CONTROLLER_H_
