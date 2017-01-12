// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_ANIMATORS_ZOOM_TRANSITION_DELEGATE_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_ANIMATORS_ZOOM_TRANSITION_DELEGATE_H_

#import <UIKit/UIKit.h>

@protocol ZoomTransitionDelegate<NSObject>
// The rectangle to start or end a zoom transition in for the object identified
// by |key|, expressed in |view|'s coordinates.
- (CGRect)rectForZoomWithKey:(NSObject*)key inView:(UIView*)view;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_ANIMATORS_ZOOM_TRANSITION_DELEGATE_H_
