// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PANELS_PANEL_TITLEBAR_VIEW_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_PANELS_PANEL_TITLEBAR_VIEW_COCOA_H_

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/panels/mouse_drag_controller.h"
#import "chrome/browser/ui/cocoa/tracking_area.h"

@class CrTrackingArea;
@class HoverImageButton;
@class MouseDragController;
@class PanelWindowControllerCocoa;

// A class that works as a custom titlebar for Panels. It is placed on top of
// the regular Cocoa titlebar. It's the place for the close button, page
// favicon, title label and a button to minimize/restore the panel.
// It also facilitates dragging and minimization of the panels, and changes
// color as 'new activity' indicator.
// One way to have custom titlebar would be to use NSBorderlessWindow,
// but it seems to affect too many other behaviors (for example, it draws shadow
// differently based on being key window) so it appears easier to simply overlay
// the standard titlebar.

// This view overlays the titlebar on top. It is used to intercept
// mouse input to prevent reordering of the other browser windows when clicking
// on the titlebar (to minimize or reorder) while in a docked collection.
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

@interface PanelTitlebarViewCocoa : NSView
                                   <NSAnimationDelegate,
                                    MouseDragControllerClient> {
 @private
  IBOutlet PanelWindowControllerCocoa* controller_;
  IBOutlet NSView* icon_;
  IBOutlet NSTextField* title_;
  IBOutlet HoverImageButton* minimizeButton_;
  IBOutlet HoverImageButton* restoreButton_;
  IBOutlet HoverImageButton* customCloseButton_;
  // Transparent view on top of entire titlebar. It catches mouse events to
  // prevent window activation by the system on mouseDown.
  IBOutlet NSView* overlay_;
  NSButton* closeButton_;  // Created explicitly, not from NIB. Weak, destroyed
                           // when view is destroyed, as a subview.
  ScopedCrTrackingArea closeButtonTrackingArea_;
  BOOL isDrawingAttention_;

  // "Glint" animation is used in "Draw Attention" mode.
  scoped_nsobject<RepaintAnimation> glintAnimation_;
  scoped_nsobject<NSTimer> glintAnimationTimer_;
  double glintInterval_;

  // Drag support.
  scoped_nsobject<MouseDragController> dragController_;
}

// Callbacks from Close, Minimize, and Restore buttons.
- (void)onCloseButtonClick:(id)sender;
- (void)onMinimizeButtonClick:(id)sender;
- (void)onRestoreButtonClick:(id)sender;

// Attaches this view to the controller_'s window as a titlebar.
- (void)attach;

- (void)setTitle:(NSString*)newTitle;
- (void)setIcon:(NSView*)newIcon;

- (NSView*)icon;

// Set the visibility of the minimize and restore buttons.
- (void)setMinimizeButtonVisibility:(BOOL)visible;
- (void)setRestoreButtonVisibility:(BOOL)visible;

// Should be called when size of the titlebar changes.
- (void)updateCustomButtonsLayout;
- (void)updateIconAndTitleLayout;

// Various events that we'll need to redraw our titlebar for.
- (void)didChangeFrame:(NSNotification*)notification;
- (void)didChangeMainWindow:(NSNotification*)notification;

// Draw Attention methods - change appearance of titlebar to attract user.
- (void)drawAttention;
- (void)stopDrawingAttention;
- (BOOL)isDrawingAttention;
- (void)startGlintAnimation;
- (void)restartGlintAnimation:(NSTimer*)timer;
- (void)stopGlintAnimation;

@end  // @interface PanelTitlebarView

// Methods which are either only for testing, or only public for testing.
@interface PanelTitlebarViewCocoa(TestingAPI)

- (PanelWindowControllerCocoa*)controller;

- (NSTextField*)title;
- (NSButton*)closeButton;
- (NSButton*)minimizeButton;
- (NSButton*)restoreButton;

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

#endif  // CHROME_BROWSER_UI_COCOA_PANELS_PANEL_TITLEBAR_VIEW_COCOA_H_
