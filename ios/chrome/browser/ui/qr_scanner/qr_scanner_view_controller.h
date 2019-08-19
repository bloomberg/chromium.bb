// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_QR_SCANNER_QR_SCANNER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_QR_SCANNER_QR_SCANNER_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/scanner/scanner_view_controller.h"

@protocol LoadQueryCommands;

// View controller for the QR Scanner.
@interface QRScannerViewController : ScannerViewController

- (instancetype)initWithPresentationProvider:
                    (id<ScannerPresenting>)presentationProvider
                                 queryLoader:(id<LoadQueryCommands>)queryLoader
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithPresentationProvider:
                    (id<ScannerPresenting>)presentationProvider
                            cameraController:(CameraController*)cameraController
    NS_UNAVAILABLE;

- (instancetype)initWithNibName:(NSString*)name
                         bundle:(NSBundle*)bundle NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_QR_SCANNER_QR_SCANNER_VIEW_CONTROLLER_H_
