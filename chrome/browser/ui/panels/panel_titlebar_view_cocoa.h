// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_TITLEBAR_VIEW_COCOA_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_TITLEBAR_VIEW_COCOA_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/cocoa_protocols.h"
#import "chrome/browser/ui/cocoa/tracking_area.h"

@class CrTrackingArea;
@class HoverImageButton;
@class PanelWindowControllerCocoa;

// A class that works as a custom titlebar for Panels. It is placed on top of
// the regular Cocoa titlebar. We paint theme image on it, and it's
// the place for the close button, page favicon, title label.
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

// This view overlays the titlebar on top. It is used to intercept
// mouse input to prevent reordering of the other browser windows when clicking
// on the titlebar (to minimize or reorder) while in a docked strip.
@interface PanelTitlebarOverlayView : NSView {
 @private
  IBOutlet PanelWindowControllerCocoa* controller_;
  BOOL disableReordering_;
}
@end

@interface RepaintAnimation : NSAnimation {
 @private
  NSView* targetView_;
}
- (id)initWithView:(NSView*)targetView duration:(double) duration;
- (void)setCurrentProgress:(NSAnimationProgress)progress;
@end

@interface PanelTitlebarViewCocoa : NSView<NSAnimationDelegate> {
 @private
  IBOutlet PanelWindowControllerCocoa* controller_;
  IBOutlet NSView* icon_;
  IBOutlet NSTextField* title_;
  // Transparent view on top of entire titlebar. It catches mouse events to
  // prevent window activation by the system on mouseDown.
  IBOutlet NSView* overlay_;
  NSButton* closeButton_;  // Created explicitly, not from NIB. Weak, destroyed
                           // when view is destroyed, as a subview.
  ScopedCrTrackingArea closeButtonTrackingArea_;
  PanelDragState dragState_;
  BOOL isDrawingAttention_;

  // "Glint" animation is used in "Draw Attention" mode.
  scoped_nsobject<RepaintAnimation> glintAnimation_;
  scoped_nsobject<NSTimer> glintAnimationTimer_;
  double glintInterval_;

  // Drag support.
  NSPoint dragStartLocation_;  // In cocoa's screen coordinates.
  // Vertical distance, in screen coordinates, from the top edge of the window
  // to the drag start mouse location. Used to prevent dragging the window under
  // the top level menu.
  double dragStartYOffset_;
  // Capture the visible frame and full screen bounds used during drag.
  // These are used to prevent dragging the panel under the system menu bar in
  // the way the system drag does.
  NSRect visibleFrameForDrag_;
  NSRect screenBoundsForDrag_;
}

  // Callback from Close button.
- (void)onCloseButtonClick:(id)sender;

  // Attaches this view to the controller_'s window as a titlebar.
- (void)attach;

- (void)setTitle:(NSString*)newTitle;
- (void)setIcon:(NSView*)newIcon;

- (NSView*)icon;

  // Should be called when size of the titlebar changes.
- (void)updateCloseButtonLayout;
- (void)updateIconAndTitleLayout;

// Various events that we'll need to redraw our titlebar for.
- (void)didChangeFrame:(NSNotification*)notification;
- (void)didChangeTheme:(NSNotification*)notification;
- (void)didChangeMainWindow:(NSNotification*)notification;

// Helpers to control title drag operation, called from more then one place.
// |mouseLocation| is in Cocoa's screen coordinates.
- (void)startDrag:(NSPoint)mouseLocation;
- (void)endDrag:(BOOL)cancelled;
- (void)drag:(NSPoint)mouseLocation;

// Draw Attention methods - change appearance of titlebar to attract user.
- (void)drawAttention;
- (void)stopDrawingAttention;
- (BOOL)isDrawingAttention;
- (void)startGlintAnimation;
- (void)restartGlintAnimation:(NSTimer*)timer;
- (void)stopGlintAnimation;

// Returns width of titlebar when shown in "icon only" mode.
- (int)iconOnlyWidthInScreenCoordinates;

@end  // @interface PanelTitlebarView

// Methods which are either only for testing, or only public for testing.
@interface PanelTitlebarViewCocoa(TestingAPI)

- (PanelWindowControllerCocoa*)controller;

- (NSTextField*)title;

// Simulates click on a close button. Used to test panel closing.
- (void)simulateCloseButtonClick;

// NativePanelTesting support.
// |mouseLocation| is in Cocoa's screen coordinates.
- (void)pressLeftMouseButtonTitlebar:(NSPoint)mouseLocation
                           modifiers:(int)modifierFlags;
- (void)releaseLeftMouseButtonTitlebar:(int)modifierFlags;
- (void)dragTitlebar:(NSPoint)mouseLocation;
- (void)cancelDragTitlebar;
- (void)finishDragTitlebar;

@end  // @interface PanelTitlebarViewCocoa(TestingAPI)

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_TITLEBAR_VIEW_COCOA_H_
