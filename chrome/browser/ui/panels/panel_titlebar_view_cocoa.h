// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_TITLEBAR_VIEW_COCOA_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_TITLEBAR_VIEW_COCOA_H_

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/background_gradient_view.h"
#import "chrome/browser/ui/cocoa/tracking_area.h"

@class CrTrackingArea;
@class PanelWindowControllerCocoa;

// A class that works as a custom titlebar for Panels. It is placed on top of
// the regular Cocoa titlebar. We paint theme image on it, and it's
// the place for the close button, wrench button, page favicon, title label.
// It also facilitates dragging and minimization of the panels, and changes
// color as 'new activity' indicator.
// One way to have custom titlebar would be to use NSBorderlessWindow,
// but it seems to affect too many other behaviors (for example, it draws shadow
// differently based on being key window) so it appears easier to simply overlay
// the standard titlebar.

@interface PanelTitlebarViewCocoa : BackgroundGradientView {
  IBOutlet PanelWindowControllerCocoa* controller_;
  NSButton* closeButton_;  // Created explicitly, not from NIB. Weak, destroyed
                           // when view is destroyed, as a subview.
  ScopedCrTrackingArea closeButtonTrackingArea_;
}

  // Callback from Close button.
- (void)onCloseButtonClick:(id)sender;

  // Attaches this view to the controller_'s window as a titlebar.
- (void)attach;

- (void)setTitle:(NSString*)newTitle;

  // Should be called when size of the titlebar changes.
- (void)updateCloseButtonLayout;

  // Accessor to Panel's controller.
- (PanelWindowControllerCocoa*)controller;

@end  // @interface PanelTitlebarView

// Methods which are either only for testing, or only public for testing.
@interface PanelTitlebarViewCocoa(TestingAPI)

// Simulates click on a close button. Used to test panel closing.
- (void)simulateCloseButtonClick;

@end  // @interface PanelTitlebarViewCocoa(TestingAPI)

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_TITLEBAR_VIEW_COCOA_H_
