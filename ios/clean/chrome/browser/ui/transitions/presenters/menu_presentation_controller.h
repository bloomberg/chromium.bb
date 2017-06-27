// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TRANSITIONS_PRESENTERS_MENU_PRESENTATION_CONTROLLER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TRANSITIONS_PRESENTERS_MENU_PRESENTATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol ToolsMenuCommands;

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
// The dispatcher for this presentation controller.
@property(nonatomic, weak) id<ToolsMenuCommands> dispatcher;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TRANSITIONS_PRESENTERS_MENU_PRESENTATION_CONTROLLER_H_
