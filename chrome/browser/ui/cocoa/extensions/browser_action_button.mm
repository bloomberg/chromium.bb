// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/browser_action_button.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/ui/cocoa/extensions/extension_action_context_menu.h"
#import "chrome/browser/ui/cocoa/image_utils.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_resource.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_source.h"
#include "content/common/notification_type.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/GTM/AppKit/GTMNSAnimation+Duration.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

NSString* const kBrowserActionButtonUpdatedNotification =
    @"BrowserActionButtonUpdatedNotification";

NSString* const kBrowserActionButtonDraggingNotification =
    @"BrowserActionButtonDraggingNotification";
NSString* const kBrowserActionButtonDragEndNotification =
    @"BrowserActionButtonDragEndNotification";

static const CGFloat kBrowserActionBadgeOriginYOffset = 5;

namespace {
const CGFloat kAnimationDuration = 0.2;
}  // anonymous namespace

// A helper class to bridge the asynchronous Skia bitmap loading mechanism to
// the extension's button.
class ExtensionImageTrackerBridge : public NotificationObserver,
                                    public ImageLoadingTracker::Observer {
 public:
  ExtensionImageTrackerBridge(BrowserActionButton* owner,
                              const Extension* extension)
      : owner_(owner),
        tracker_(this) {
    // The Browser Action API does not allow the default icon path to be
    // changed at runtime, so we can load this now and cache it.
    std::string path = extension->browser_action()->default_icon_path();
    if (!path.empty()) {
      tracker_.LoadImage(extension, extension->GetResource(path),
                         gfx::Size(Extension::kBrowserActionIconMaxSize,
                                   Extension::kBrowserActionIconMaxSize),
                         ImageLoadingTracker::DONT_CACHE);
    }
    registrar_.Add(this, NotificationType::EXTENSION_BROWSER_ACTION_UPDATED,
                   Source<ExtensionAction>(extension->browser_action()));
  }

  ~ExtensionImageTrackerBridge() {}

  // ImageLoadingTracker::Observer implementation.
  void OnImageLoaded(SkBitmap* image, const ExtensionResource& resource,
                     int index) {
    if (image)
      [owner_ setDefaultIcon:gfx::SkBitmapToNSImage(*image)];
    [owner_ updateState];
  }

  // Overridden from NotificationObserver.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details) {
    if (type == NotificationType::EXTENSION_BROWSER_ACTION_UPDATED)
      [owner_ updateState];
    else
      NOTREACHED();
  }

 private:
  // Weak. Owns us.
  BrowserActionButton* owner_;

  // Loads the button's icons for us on the file thread.
  ImageLoadingTracker tracker_;

  // Used for registering to receive notifications and automatic clean up.
  NotificationRegistrar registrar_;

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
            profile:(Profile*)profile
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

    [self setTitle:@""];
    [self setButtonType:NSMomentaryChangeButton];
    [self setShowsBorderOnlyWhileMouseInside:YES];

    [self setMenu:[[[ExtensionActionContextMenu alloc]
        initWithExtension:extension
                  profile:profile
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
      postNotificationName:kBrowserActionButtonDragEndNotification
                    object:self];
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

- (void)setDefaultIcon:(NSImage*)image {
  defaultIcon_.reset([image retain]);
}

- (void)setTabSpecificIcon:(NSImage*)image {
  tabSpecificIcon_.reset([image retain]);
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

  SkBitmap image = extension_->browser_action()->GetIcon(tabId_);
  if (!image.isNull()) {
    [self setTabSpecificIcon:gfx::SkBitmapToNSImage(image)];
    [self setImage:tabSpecificIcon_];
  } else if (defaultIcon_) {
    [self setImage:defaultIcon_];
  }

  [[self cell] setTabId:tabId_];

  [self setNeedsDisplay:YES];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:kBrowserActionButtonUpdatedNotification
      object:self];
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
             neverFlipped:YES];

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

- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  [NSGraphicsContext saveGraphicsState];
  [super drawInteriorWithFrame:cellFrame inView:controlView];
  cellFrame.origin.y += kBrowserActionBadgeOriginYOffset;
  [self drawBadgeWithinFrame:cellFrame];
  [NSGraphicsContext restoreGraphicsState];
}

@end
