// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_POSITIONER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_POSITIONER_H_

#import <UIKit/UIKit.h>

@protocol OmniboxPopupPositioner

// Returns the view the popup is anchored next to. Callers are responsible for
// adding the popup as a sibling either above or below this view.
// TODO(crbug.com/788705): Remove this method when removing legacy toolbar.
- (UIView*)popupAnchorView;

// View to which the popup view should be added as subview.
- (UIView*)popupParentView;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_POSITIONER_H_
