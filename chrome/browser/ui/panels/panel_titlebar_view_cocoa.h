// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_TITLEBAR_VIEW_COCOA_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_TITLEBAR_VIEW_COCOA_H_

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/tracking_area.h"

@class CrTrackingArea;
@class HoverImageButton;
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

// When Drag is cancelled by hitting ESC key, we may still receive
// the mouseDragged events but should ignore them until the mouse button is
// released. Use these simple states to track this condition.
enum PanelDragState {
  PANEL_DRAG_CAN_START,  // Mouse key went down, drag may be started.
  PANEL_DRAG_IN_PROGRESS,
  PANEL_DRAG_SUPPRESSED  // Ignore drag events until PANEL_DRAG_CAN_START.
};

@interface PanelTitlebarViewCocoa : NSView {
 @private
  IBOutlet PanelWindowControllerCocoa* controller_;
  IBOutlet NSView* icon_;
  IBOutlet NSTextField* title_;
  IBOutlet HoverImageButton* settingsButton_;
  NSButton* closeButton_;  // Created explicitly, not from NIB. Weak, destroyed
                           // when view is destroyed, as a subview.
  ScopedCrTrackingArea closeButtonTrackingArea_;
  PanelDragState dragState_;
  BOOL isDrawingAttention_;
}

  // Callback from Close button.
- (void)onCloseButtonClick:(id)sender;

  // Callback from Settings button.
- (void)onSettingsButtonClick:(id)sender;

  // Attaches this view to the controller_'s window as a titlebar.
- (void)attach;

- (void)setTitle:(NSString*)newTitle;
- (void)setIcon:(NSView*)newIcon;

- (NSView*)icon;

  // Should be called when size of the titlebar changes.
- (void)updateCloseButtonLayout;
- (void)updateIconAndTitleLayout;

// We need to update our look when the Chrome theme changes or window focus
// changes.
- (void)didChangeTheme:(NSNotification*)notification;
- (void)didChangeMainWindow:(NSNotification*)notification;

// Helpers to control title drag operation, called from more then one place.
// TODO(dimich): replace BOOL parameter that we have to explicitly specify at
// callsites with an enum defined in PanelManager.
- (void)startDrag;
- (void)endDrag:(BOOL)cancelled;
- (void)dragWithDeltaX:(int)deltaX;

  // Update the visibility of settings button.
- (void)updateSettingsButtonVisibility:(BOOL)mouseOverWindow;
- (void)checkMouseAndUpdateSettingsButtonVisibility;

// Draw Attention methods - change appearance of titlebar to attract user.
- (void)drawAttention;
- (void)stopDrawingAttention;
- (BOOL)isDrawingAttention;
@end  // @interface PanelTitlebarView

// Methods which are either only for testing, or only public for testing.
@interface PanelTitlebarViewCocoa(TestingAPI)

- (PanelWindowControllerCocoa*)controller;

// Simulates click on a close button. Used to test panel closing.
- (void)simulateCloseButtonClick;

// NativePanelTesting support.
- (void)pressLeftMouseButtonTitlebar;
- (void)releaseLeftMouseButtonTitlebar;
- (void)dragTitlebarDeltaX:(double)delta_x
                    deltaY:(double)delta_y;
- (void)cancelDragTitlebar;
- (void)finishDragTitlebar;

@end  // @interface PanelTitlebarViewCocoa(TestingAPI)

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_TITLEBAR_VIEW_COCOA_H_
