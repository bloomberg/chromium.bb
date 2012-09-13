// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/browser_action_button.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/ui/cocoa/extensions/extension_action_context_menu.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_resource.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/GTM/AppKit/GTMNSAnimation+Duration.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"
#include "ui/gfx/size.h"

using extensions::Extension;

NSString* const kBrowserActionButtonDraggingNotification =
    @"BrowserActionButtonDraggingNotification";
NSString* const kBrowserActionButtonDragEndNotification =
    @"BrowserActionButtonDragEndNotification";

const CGFloat kBrowserActionBadgeOriginYOffset = 5;
const CGFloat kAnimationDuration = 0.2;

// A helper class to bridge the asynchronous Skia bitmap loading mechanism to
// the extension's button.
class ExtensionImageTrackerBridge : public content::NotificationObserver,
                                    public ImageLoadingTracker::Observer {
 public:
  ExtensionImageTrackerBridge(BrowserActionButton* owner,
                              const Extension* extension)
      : owner_(owner),
        tracker_(this),
        browser_action_(extension->browser_action()) {
    // The Browser Action API does not allow the default icon path to be
    // changed at runtime, so we can load this now and cache it.
    std::string path = extension->browser_action()->default_icon_path();
    if (!path.empty()) {
      tracker_.LoadImage(extension, extension->GetResource(path),
                         gfx::Size(Extension::kBrowserActionIconMaxSize,
                                   Extension::kBrowserActionIconMaxSize),
                         ImageLoadingTracker::DONT_CACHE);
    }
    registrar_.Add(
        this, chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_UPDATED,
        content::Source<ExtensionAction>(browser_action_));
  }

  ~ExtensionImageTrackerBridge() {}

  // ImageLoadingTracker::Observer implementation.
  void OnImageLoaded(const gfx::Image& image,
                     const std::string& extension_id,
                     int index) OVERRIDE {
    browser_action_->CacheIcon(image);
    [owner_ updateState];
  }

  // Overridden from content::NotificationObserver.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) {
    if (type == chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_UPDATED)
      [owner_ updateState];
    else
      NOTREACHED();
  }

 private:
  // Weak. Owns us.
  BrowserActionButton* owner_;

  // Loads the button's icons for us on the file thread.
  ImageLoadingTracker tracker_;

  // The browser action whose images we're loading.
  ExtensionAction* const browser_action_;

  // Used for registering to receive notifications and automatic clean up.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionImageTrackerBridge);
};

@interface BrowserActionCell (Internals)
- (void)drawBadgeWithinFrame:(NSRect)frame;
@end

@interface BrowserActionButton (Private)
- (void)endDrag;
@end

@implementation BrowserActionButton

@synthesize isBeingDragged = isBeingDragged_;
@synthesize extension = extension_;
@synthesize tabId = tabId_;

+ (Class)cellClass {
  return [BrowserActionCell class];
}

- (id)initWithFrame:(NSRect)frame
          extension:(const Extension*)extension
            browser:(Browser*)browser
              tabId:(int)tabId {
  if ((self = [super initWithFrame:frame])) {
    BrowserActionCell* cell = [[[BrowserActionCell alloc] init] autorelease];
    // [NSButton setCell:] warns to NOT use setCell: other than in the
    // initializer of a control.  However, we are using a basic
    // NSButton whose initializer does not take an NSCell as an
    // object.  To honor the assumed semantics, we do nothing with
    // NSButton between alloc/init and setCell:.
    [self setCell:cell];
    [cell setTabId:tabId];
    [cell setExtensionAction:extension->browser_action()];
    [cell
        accessibilitySetOverrideValue:base::SysUTF8ToNSString(extension->name())
        forAttribute:NSAccessibilityDescriptionAttribute];
    [cell setImageID:IDR_BROWSER_ACTION
      forButtonState:image_button_cell::kDefaultState];
    [cell setImageID:IDR_BROWSER_ACTION_H
      forButtonState:image_button_cell::kHoverState];
    [cell setImageID:IDR_BROWSER_ACTION_P
      forButtonState:image_button_cell::kPressedState];

    [self setTitle:@""];
    [self setButtonType:NSMomentaryChangeButton];
    [self setShowsBorderOnlyWhileMouseInside:YES];

    [self setMenu:[[[ExtensionActionContextMenu alloc]
        initWithExtension:extension
                  browser:browser
          extensionAction:extension->browser_action()] autorelease]];

    tabId_ = tabId;
    extension_ = extension;
    imageLoadingBridge_.reset(new ExtensionImageTrackerBridge(self, extension));

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
  [[self cell] setHighlighted:YES];
  dragCouldStart_ = YES;
}

- (void)mouseDragged:(NSEvent*)theEvent {
  if (!dragCouldStart_)
    return;

  if (!isBeingDragged_) {
    // The start of a drag. Position the button above all others.
    [[self superview] addSubview:self positioned:NSWindowAbove relativeTo:nil];
  }
  isBeingDragged_ = YES;
  NSRect buttonFrame = [self frame];
  // TODO(andybons): Constrain the buttons to be within the container.
  // Clamp the button to be within its superview along the X-axis.
  buttonFrame.origin.x += [theEvent deltaX];
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

    NSDictionary* animationDictionary =
        [NSDictionary dictionaryWithObjectsAndKeys:
            self, NSViewAnimationTargetKey,
            [NSValue valueWithRect:[self frame]], NSViewAnimationStartFrameKey,
            [NSValue valueWithRect:frameRect], NSViewAnimationEndFrameKey,
            nil];
    [moveAnimation_ setViewAnimations:
        [NSArray arrayWithObject:animationDictionary]];
    [moveAnimation_ startAnimation];
  }
}

- (void)updateState {
  if (tabId_ < 0)
    return;

  std::string tooltip = extension_->browser_action()->GetTitle(tabId_);
  if (tooltip.empty()) {
    [self setToolTip:nil];
  } else {
    [self setToolTip:base::SysUTF8ToNSString(tooltip)];
  }

  gfx::Image image = extension_->browser_action()->GetIcon(tabId_);
  if (!image.IsEmpty())
    [self setImage:image.ToNSImage()];

  [[self cell] setTabId:tabId_];

  bool enabled = extension_->browser_action()->GetIsVisible(tabId_);
  [self setEnabled:enabled ? YES : NO];

  [self setNeedsDisplay:YES];
}

- (BOOL)isAnimating {
  return [moveAnimation_ isAnimating];
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
  [[self cell] drawBadgeWithinFrame:bounds];

  [image unlockFocus];
  return image;
}

@end

@implementation BrowserActionCell

@synthesize tabId = tabId_;
@synthesize extensionAction = extensionAction_;

- (void)drawBadgeWithinFrame:(NSRect)frame {
  gfx::CanvasSkiaPaint canvas(frame, false);
  canvas.set_composite_alpha(true);
  gfx::Rect boundingRect(NSRectToCGRect(frame));
  extensionAction_->PaintBadge(&canvas, boundingRect, tabId_);
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  gfx::ScopedNSGraphicsContextSaveGState scopedGState;
  [super drawWithFrame:cellFrame inView:controlView];
  const NSSize imageSize = self.image.size;
  const NSRect imageRect =
      NSMakeRect(std::floor((NSWidth(cellFrame) - imageSize.width) / 2.0),
                 std::floor((NSHeight(cellFrame) - imageSize.height) / 2.0),
                 imageSize.width, imageSize.height);
  [self.image drawInRect:imageRect
                fromRect:NSZeroRect
               operation:NSCompositeSourceOver
                fraction:1.0
          respectFlipped:YES
                   hints:nil];

  cellFrame.origin.y += kBrowserActionBadgeOriginYOffset;
  [self drawBadgeWithinFrame:cellFrame];
}

@end
