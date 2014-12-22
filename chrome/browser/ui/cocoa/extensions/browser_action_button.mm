// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/browser_action_button.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/extensions/browser_actions_controller.h"
#import "chrome/browser/ui/cocoa/extensions/extension_action_context_menu_controller.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_action_view_delegate_cocoa.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMNSAnimation+Duration.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

NSString* const kBrowserActionButtonDraggingNotification =
    @"BrowserActionButtonDraggingNotification";
NSString* const kBrowserActionButtonDragEndNotification =
    @"BrowserActionButtonDragEndNotification";

static const CGFloat kBrowserActionBadgeOriginYOffset = 5;
static const CGFloat kAnimationDuration = 0.2;
static const CGFloat kMinimumDragDistance = 5;

// A class to bridge the ToolbarActionViewController and the
// BrowserActionButton.
class ToolbarActionViewDelegateBridge : public ToolbarActionViewDelegateCocoa {
 public:
  ToolbarActionViewDelegateBridge(BrowserActionButton* owner,
                                  BrowserActionsController* controller,
                                  ToolbarActionViewController* viewController);
  ~ToolbarActionViewDelegateBridge();

  ExtensionActionContextMenuController* menuController() {
    return menuController_;
  }

 private:
  // ToolbarActionViewDelegateCocoa:
  ToolbarActionViewController* GetPreferredPopupViewController() override;
  content::WebContents* GetCurrentWebContents() const override;
  void UpdateState() override;
  NSPoint GetPopupPoint() override;
  void SetContextMenuController(
      ExtensionActionContextMenuController* menuController) override;

  // The owning button. Weak.
  BrowserActionButton* owner_;

  // The BrowserActionsController that owns the button. Weak.
  BrowserActionsController* controller_;

  // The ToolbarActionViewController for which this is the delegate. Weak.
  ToolbarActionViewController* viewController_;

  // The context menu controller. Weak.
  ExtensionActionContextMenuController* menuController_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarActionViewDelegateBridge);
};

ToolbarActionViewDelegateBridge::ToolbarActionViewDelegateBridge(
    BrowserActionButton* owner,
    BrowserActionsController* controller,
    ToolbarActionViewController* viewController)
    : owner_(owner),
      controller_(controller),
      viewController_(viewController),
      menuController_(nil) {
  viewController_->SetDelegate(this);
}

ToolbarActionViewDelegateBridge::~ToolbarActionViewDelegateBridge() {
  viewController_->SetDelegate(nullptr);
}

ToolbarActionViewController*
ToolbarActionViewDelegateBridge::GetPreferredPopupViewController() {
  return viewController_;
}

content::WebContents* ToolbarActionViewDelegateBridge::GetCurrentWebContents()
    const {
  return [controller_ currentWebContents];
}

void ToolbarActionViewDelegateBridge::UpdateState() {
  [owner_ updateState];
}

NSPoint ToolbarActionViewDelegateBridge::GetPopupPoint() {
  return [controller_ popupPointForId:[owner_ viewController]->GetId()];
}

void ToolbarActionViewDelegateBridge::SetContextMenuController(
    ExtensionActionContextMenuController* menuController) {
  menuController_ = menuController;
}

@interface BrowserActionCell (Internals)
- (void)drawBadgeWithinFrame:(NSRect)frame
              forWebContents:(content::WebContents*)webContents;
@end

@interface BrowserActionButton (Private)
- (void)endDrag;
@end

@implementation BrowserActionButton

@synthesize isBeingDragged = isBeingDragged_;

+ (Class)cellClass {
  return [BrowserActionCell class];
}

- (id)initWithFrame:(NSRect)frame
     viewController:(ToolbarActionViewController*)viewController
         controller:(BrowserActionsController*)controller {
  if ((self = [super initWithFrame:frame])) {
    BrowserActionCell* cell = [[[BrowserActionCell alloc] init] autorelease];
    // [NSButton setCell:] warns to NOT use setCell: other than in the
    // initializer of a control.  However, we are using a basic
    // NSButton whose initializer does not take an NSCell as an
    // object.  To honor the assumed semantics, we do nothing with
    // NSButton between alloc/init and setCell:.
    [self setCell:cell];

    browserActionsController_ = controller;
    viewController_ = viewController;
    viewControllerDelegate_.reset(
        new ToolbarActionViewDelegateBridge(self, controller, viewController));

    [cell setBrowserActionsController:controller];
    [cell setViewController:viewController_];
    [cell
        accessibilitySetOverrideValue:base::SysUTF16ToNSString(
            viewController_->GetAccessibleName([controller currentWebContents]))
        forAttribute:NSAccessibilityDescriptionAttribute];
    [cell setImageID:IDR_BROWSER_ACTION
      forButtonState:image_button_cell::kDefaultState];
    [cell setImageID:IDR_BROWSER_ACTION_H
      forButtonState:image_button_cell::kHoverState];
    [cell setImageID:IDR_BROWSER_ACTION_P
      forButtonState:image_button_cell::kPressedState];
    [cell setImageID:IDR_BROWSER_ACTION
      forButtonState:image_button_cell::kDisabledState];

    [self setTitle:@""];
    [self setButtonType:NSMomentaryChangeButton];
    [self setShowsBorderOnlyWhileMouseInside:YES];

    base::scoped_nsobject<NSMenu> contextMenu(
        [[NSMenu alloc] initWithTitle:@""]);
    [contextMenu setDelegate:self];
    [self setMenu:contextMenu];

    moveAnimation_.reset([[NSViewAnimation alloc] init]);
    [moveAnimation_ gtm_setDuration:kAnimationDuration
                          eventMask:NSLeftMouseUpMask];
    [moveAnimation_ setAnimationBlockingMode:NSAnimationNonblocking];

    [self updateState];
  }

  return self;
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (void)mouseDown:(NSEvent*)theEvent {
  NSPoint location = [self convertPoint:[theEvent locationInWindow]
                               fromView:nil];
  if (NSPointInRect(location, [self bounds])) {
    [[self cell] setHighlighted:YES];
    dragCouldStart_ = YES;
    dragStartPoint_ = [self convertPoint:[theEvent locationInWindow]
                                fromView:nil];
  }
}

- (void)mouseDragged:(NSEvent*)theEvent {
  if (!dragCouldStart_)
    return;

  NSPoint eventPoint = [theEvent locationInWindow];
  if (!isBeingDragged_) {
    // Don't initiate a drag until it moves at least kMinimumDragDistance.
    NSPoint dragStart = [self convertPoint:dragStartPoint_ toView:nil];
    CGFloat dx = eventPoint.x - dragStart.x;
    CGFloat dy = eventPoint.y - dragStart.y;
    if (dx*dx + dy*dy < kMinimumDragDistance*kMinimumDragDistance)
      return;

    // The start of a drag. Position the button above all others.
    [[self superview] addSubview:self positioned:NSWindowAbove relativeTo:nil];

    // We reset the |dragStartPoint_| so that the mouse can always be in the
    // same point along the button's x axis, and we avoid a "jump" when first
    // starting to drag.
    dragStartPoint_ = [self convertPoint:eventPoint fromView:nil];

    isBeingDragged_ = YES;
  }

  NSRect buttonFrame = [self frame];
  // The desired x is the current mouse point, minus the original offset of the
  // mouse into the button.
  NSPoint localPoint = [[self superview] convertPoint:eventPoint fromView:nil];
  CGFloat desiredX = localPoint.x - dragStartPoint_.x;
  // Clamp the button to be within its superview along the X-axis.
  NSRect containerBounds = [[self superview] bounds];
  desiredX = std::min(std::max(NSMinX(containerBounds), desiredX),
                      NSMaxX(containerBounds) - NSWidth(buttonFrame));
  buttonFrame.origin.x = desiredX;

  // If the button is in the overflow menu, it could move along the y-axis, too.
  if ([browserActionsController_ isOverflow]) {
    CGFloat desiredY = localPoint.y - dragStartPoint_.y;
    desiredY = std::min(std::max(NSMinY(containerBounds), desiredY),
                        NSMaxY(containerBounds) - NSHeight(buttonFrame));
    buttonFrame.origin.y = desiredY;
  }

  [self setFrame:buttonFrame];
  [self setNeedsDisplay:YES];
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kBrowserActionButtonDraggingNotification
      object:self];
}

- (void)mouseUp:(NSEvent*)theEvent {
  dragCouldStart_ = NO;
  // There are non-drag cases where a mouseUp: may happen
  // (e.g. mouse-down, cmd-tab to another application, move mouse,
  // mouse-up).
  NSPoint location = [self convertPoint:[theEvent locationInWindow]
                               fromView:nil];
  if (NSPointInRect(location, [self bounds]) && !isBeingDragged_) {
    // Only perform the click if we didn't drag the button.
    [self performClick:self];
  } else {
    // Make sure an ESC to end a drag doesn't trigger 2 endDrags.
    if (isBeingDragged_) {
      [self endDrag];
    } else {
      [super mouseUp:theEvent];
    }
  }
}

- (void)endDrag {
  isBeingDragged_ = NO;
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kBrowserActionButtonDragEndNotification object:self];
  [[self cell] setHighlighted:NO];
}

- (void)setFrame:(NSRect)frameRect animate:(BOOL)animate {
  if (!animate) {
    [self setFrame:frameRect];
  } else {
    if ([moveAnimation_ isAnimating])
      [moveAnimation_ stopAnimation];

    NSDictionary* animationDictionary = @{
      NSViewAnimationTargetKey : self,
      NSViewAnimationStartFrameKey : [NSValue valueWithRect:[self frame]],
      NSViewAnimationEndFrameKey : [NSValue valueWithRect:frameRect]
    };
    [moveAnimation_ setViewAnimations: @[ animationDictionary ]];
    [moveAnimation_ startAnimation];
  }
}

- (void)updateState {
  content::WebContents* webContents =
      [browserActionsController_ currentWebContents];
  if (!webContents)
    return;

  base::string16 tooltip = viewController_->GetTooltip(webContents);
  [self setToolTip:(tooltip.empty() ? nil : base::SysUTF16ToNSString(tooltip))];

  gfx::Image image = viewController_->GetIcon(webContents);

  if (!image.IsEmpty())
    [self setImage:image.ToNSImage()];

  [self setEnabled:viewController_->IsEnabled(webContents)];

  [self setNeedsDisplay:YES];
}

- (void)onRemoved {
  // The button is being removed from the toolbar, and the backing controller
  // will also be removed. Destroy the delegate.
  // We only need to do this because in cocoa's memory management, removing the
  // button from the toolbar doesn't synchronously dealloc it.
  viewControllerDelegate_.reset();
}

- (BOOL)isAnimating {
  return [moveAnimation_ isAnimating];
}

- (NSRect)frameAfterAnimation {
  if ([moveAnimation_ isAnimating]) {
    NSRect endFrame = [[[[moveAnimation_ viewAnimations] objectAtIndex:0]
        valueForKey:NSViewAnimationEndFrameKey] rectValue];
    return endFrame;
  } else {
    return [self frame];
  }
}

- (ToolbarActionViewController*)viewController {
  return viewController_;
}

- (NSImage*)compositedImage {
  NSRect bounds = [self bounds];
  NSImage* image = [[[NSImage alloc] initWithSize:bounds.size] autorelease];
  [image lockFocus];

  [[NSColor clearColor] set];
  NSRectFill(bounds);

  NSImage* actionImage = [self image];
  const NSSize imageSize = [actionImage size];
  const NSRect imageRect =
      NSMakeRect(std::floor((NSWidth(bounds) - imageSize.width) / 2.0),
                 std::floor((NSHeight(bounds) - imageSize.height) / 2.0),
                 imageSize.width, imageSize.height);
  [actionImage drawInRect:imageRect
                 fromRect:NSZeroRect
                operation:NSCompositeSourceOver
                 fraction:1.0
           respectFlipped:YES
                    hints:nil];

  bounds.origin.y += kBrowserActionBadgeOriginYOffset;
  [[self cell] drawBadgeWithinFrame:bounds
                     forWebContents:
                            [browserActionsController_ currentWebContents]];

  [image unlockFocus];
  return image;
}

- (void)menuNeedsUpdate:(NSMenu*)menu {
  [menu removeAllItems];
  // |menuController()| can be nil if we don't show context menus for the given
  // action.
  if (viewControllerDelegate_->menuController())
    [viewControllerDelegate_->menuController() populateMenu:menu];
}

@end

@implementation BrowserActionCell

@synthesize browserActionsController = browserActionsController_;
@synthesize viewController = viewController_;

- (void)drawBadgeWithinFrame:(NSRect)frame
              forWebContents:(content::WebContents*)webContents {
  gfx::CanvasSkiaPaint canvas(frame, false);
  canvas.set_composite_alpha(true);
  gfx::Rect boundingRect(NSRectToCGRect(frame));
  viewController_->PaintExtra(&canvas, boundingRect, webContents);
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  gfx::ScopedNSGraphicsContextSaveGState scopedGState;
  [super drawWithFrame:cellFrame inView:controlView];
  DCHECK(viewController_);
  content::WebContents* webContents =
      [browserActionsController_ currentWebContents];
  bool enabled = viewController_->IsEnabled(webContents);
  const NSSize imageSize = self.image.size;
  const NSRect imageRect =
      NSMakeRect(std::floor((NSWidth(cellFrame) - imageSize.width) / 2.0),
                 std::floor((NSHeight(cellFrame) - imageSize.height) / 2.0),
                 imageSize.width, imageSize.height);
  [self.image drawInRect:imageRect
                fromRect:NSZeroRect
               operation:NSCompositeSourceOver
                fraction:enabled ? 1.0 : 0.4
          respectFlipped:YES
                   hints:nil];

  cellFrame.origin.y += kBrowserActionBadgeOriginYOffset;
  [self drawBadgeWithinFrame:cellFrame
              forWebContents:webContents];
}

- (ui::ThemeProvider*)themeProviderForWindow:(NSWindow*)window {
  ui::ThemeProvider* themeProvider = [window themeProvider];
  if (!themeProvider)
    themeProvider =
        [[browserActionsController_ browser]->window()->GetNativeWindow()
            themeProvider];
  return themeProvider;
}

@end
