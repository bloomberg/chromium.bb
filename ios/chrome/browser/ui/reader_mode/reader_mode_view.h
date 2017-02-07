// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READER_MODE_READER_MODE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_READER_MODE_READER_MODE_VIEW_H_

#import <UIKit/UIKit.h>

#include <memory>

#include "base/ios/block_types.h"

namespace dom_distiller {
class DistillerViewer;
}

// Simple delegate to be notified on user action.
@protocol ReaderModeViewDelegate
// Called when the user decides to exit reader mode.
- (void)exitReaderMode;
@end

// A waiting view to show to the user while the distillation is taking place.
// This view also owns the dom_distiller::DistillerViewer for the content, this
// allows for the distillation to stop if the view is deallocated before the
// distillation finishes.
@interface ReaderModeView : UIView

- (instancetype)initWithFrame:(CGRect)frame
                     delegate:(id<ReaderModeViewDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// The view's delegate.
@property(weak, nonatomic, readonly) id<ReaderModeViewDelegate> delegate;

// Takes ownership of the viewer as it is the only handle to the distillation:
// When freed it cancels the distillation, if it is not finished yet. The
// ReaderModeView instance owns this object so that the distillation is
// cancelled if the view is deallocated.
- (void)assignDistillerViewer:
    (std::unique_ptr<dom_distiller::DistillerViewer>)viewer;

// Starts the waiting animation.
- (void)start;

// Call this method when this view is removed from the visible view hierarchy.
// |completion| will be called when this view is done animating out.
- (void)stopWaitingWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_READER_MODE_READER_MODE_VIEW_H_
