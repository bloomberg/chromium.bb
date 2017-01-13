// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_PRESENTERS_MENU_PRESENTATION_CONTROLLER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_PRESENTERS_MENU_PRESENTATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol ToolbarCommands;

// A presentation controller for presenting a "menu" interface: a (usually)
// rectangular presentation that covers part of the window, and which doesn't
// obscure the presenting view. The presenting view remains in the view
// hierarchy.
// The presented view controller (that is, the view controller that runs the
// menu) should set the size of its view in -loadView or -viewDidLoad.
// If the presenting view controller conforms to the MenuPresentationDelegate
// protocol, that protocol will be used to determine the region the menu
// appears in; otherwise it will appear in the center of the presenting view
// the region the menu appears in. Otherwise a default rectangular area is
// controller's view.
@interface MenuPresentationController : UIPresentationController
@property(nonatomic, assign) id<ToolbarCommands> toolbarCommandHandler;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_PRESENTERS_MENU_PRESENTATION_CONTROLLER_H_
