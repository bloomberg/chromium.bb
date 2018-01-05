// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_LEGACY_TOOLBAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_LEGACY_TOOLBAR_COORDINATOR_H_

#import "ios/chrome/browser/ui/bubble/bubble_view_anchor_point_provider.h"

// TODO(crbug.com/788705): Remove this protocol during old toolbar cleanup.
// This protocol is containing the legacy methods implemented by the
// LegacyToolbarCoordinator. It is used to separate those methods from the ones
// used in the refactored toolbar.
@protocol LegacyToolbarCoordinator<BubbleViewAnchorPointProvider, NSObject>

- (void)selectedTabChanged;
- (void)setTabCount:(NSInteger)tabCount;
- (void)browserStateDestroyed;
- (void)setShareButtonEnabled:(BOOL)enabled;
- (void)currentPageLoadStarted;
- (void)adjustToolbarHeight;
- (CGRect)visibleOmniboxFrame;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_LEGACY_TOOLBAR_COORDINATOR_H_
