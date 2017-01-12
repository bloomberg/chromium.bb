// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_PRESENTERS_MENU_PRESENTATION_DELEGATE_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_PRESENTERS_MENU_PRESENTATION_DELEGATE_H_

#import <Foundation/Foundation.h>

// Protocol for an object to assist with menu presentation by providing a
// frame for the menu.
@protocol MenuPresentationDelegate
// Return the rect, in the coordinate space of the presenting view controller's
// view, that |presentation| should use for the presenting view controller.
- (CGRect)frameForMenuPresentation:(UIPresentationController*)presentation;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_PRESENTERS_MENU_PRESENTATION_DELEGATE_H_
