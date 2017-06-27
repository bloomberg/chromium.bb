// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TRANSITIONS_PRESENTERS_MENU_PRESENTATION_DELEGATE_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TRANSITIONS_PRESENTERS_MENU_PRESENTATION_DELEGATE_H_

#import <Foundation/Foundation.h>

// Protocol for an object to assist with menu presentation by providing
// spatial information for the menu.
@protocol MenuPresentationDelegate
// Returns the CGRect in which the Menu presentation will take place.
- (CGRect)boundsForMenuPresentation;
// Returns the origin in the coordinate space of the presenting view
// controller's view, in which the Menu presentation will present from.
- (CGRect)originForMenuPresentation;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TRANSITIONS_PRESENTERS_MENU_PRESENTATION_DELEGATE_H_
