// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_PRIMARY_TOOLBAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_PRIMARY_TOOLBAR_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/toolbar/adaptive/adaptive_toolbar_view_controller.h"

// ViewController for the primary toobar part of the adaptive toolbar. It is the
// part always displayed and containing the location bar.
@interface PrimaryToolbarViewController : AdaptiveToolbarViewController

// Sets the location bar view, containing the omnibox.
- (void)setLocationBarView:(UIView*)locationBarView;

// Shows the animation when transitioning to a prerendered page.
- (void)showPrerenderingAnimation;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_PRIMARY_TOOLBAR_VIEW_CONTROLLER_H_
