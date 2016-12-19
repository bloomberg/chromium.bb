// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READER_MODE_READER_MODE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_READER_MODE_READER_MODE_CONTROLLER_H_

#import <UIKit/UIKit.h>

class GURL;
class ReaderModeChecker;

namespace web {
class WebState;
}

@protocol ReaderModeControllerDelegate
// Called by the controller to retrieve the superview that the reader mode
// panel should be a subview of. The panel's position is based on the size of
// the superview, which is presumed to fill the device's screen.
- (UIView*)superviewForReaderModePanel;

// Called when the distillation finishes.
- (void)loadReaderModeHTML:(NSString*)html forURL:(const GURL&)url;
@end

// A reader mode controller can trigger distillations on the current visible
// page on a web state.
@interface ReaderModeController : NSObject

- (instancetype)initWithWebState:(web::WebState*)state
                        delegate:(id<ReaderModeControllerDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The object that checks if a URL is suitable for reader mode.
@property(nonatomic, readonly, assign) ReaderModeChecker* checker;

// Starts a distillation, animates a view on screen to show a waiting UI to the
// user while the distillation is ongoing, and calls the delegate once finished.
- (void)switchToReaderMode;
// Removes the waiting view from the superview returned by the delegate.
- (void)exitReaderMode;
// Instructs the controller to detach itself from the web state.
- (void)detachFromWebState;
@end

#endif  // IOS_CHROME_BROWSER_UI_READER_MODE_READER_MODE_CONTROLLER_H_
