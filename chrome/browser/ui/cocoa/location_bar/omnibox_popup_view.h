// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_OMNIBOX_POPUP_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_OMNIBOX_POPUP_VIEW_H_
#pragma once

#import <Cocoa/Cocoa.h>

// The content view for the omnibox popup.  Supports up to two subviews (the
// AutocompleteMatrix containing autocomplete results and (optionally) an
// InstantOptInView.
@interface OmniboxPopupView : NSView
@end

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_OMNIBOX_POPUP_VIEW_H_
