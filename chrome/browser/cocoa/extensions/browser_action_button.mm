// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/extensions/browser_action_button.h"

#include "app/gfx/canvas_paint.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/cocoa/extensions/extension_action_context_menu.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "gfx/rect.h"
#include "gfx/size.h"
#include "skia/ext/skia_utils_mac.h"

extern const NSString* kBrowserActionButtonUpdatedNotification =
    @"BrowserActionButtonUpdatedNotification";

static const CGFloat kBrowserActionBadgeOriginYOffset = 5;

// Since the container is the maximum height of the toolbar, we have to move the
// buttons up by this amount in order to have them look vertically centered
// within the toolbar.
static const CGFloat kBrowserActionOriginYOffset = 5;

// The size of each button on the toolbar.
static const CGFloat kBrowserActionHeight = 27;
extern const CGFloat kBrowserActionWidth = 29;

namespace {
const CGFloat kShadowOffset = 2.0;
}  // anonymous namespace

// A helper class to bridge the asynchronous Skia bitmap loading mechanism to
// the extension's button.
class ExtensionImageTrackerBridge : public NotificationObserver,
                                    public ImageLoadingTracker::Observer {
 public:
  ExtensionImageTrackerBridge(BrowserActionButton* owner, Extension* extension)
      : owner_(owner),
        tracker_(NULL) {
    // The Browser Action API does not allow the default icon path to be
    // changed at runtime, so we can load this now and cache it.
    std::string path = extension->browser_action()->default_icon_path();
    if (!path.empty()) {
      tracker_ = new ImageLoadingTracker(this, 1);
      tracker_->PostLoadImageTask(extension->GetResource(path),
          gfx::Size(Extension::kBrowserActionIconMaxSize,
                    Extension::kBrowserActionIconMaxSize));
    }
    registrar_.Add(this, NotificationType::EXTENSION_BROWSER_ACTION_UPDATED,
                   Source<ExtensionAction>(extension->browser_action()));
  }

  ~ExtensionImageTrackerBridge() {
    if (tracker_)
      tracker_->StopTrackingImageLoad();
  }

  // ImageLoadingTracker::Observer implementation.
  void OnImageLoaded(SkBitmap* image, size_t index) {
    if (image)
      [owner_ setDefaultIcon:gfx::SkBitmapToNSImage(*image)];
    tracker_ = NULL;
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

  // Loads the button's icons for us on the file thread. Weak.
  ImageLoadingTracker* tracker_;

  // Used for registering to receive notifications and automatic clean up.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionImageTrackerBridge);
};

@interface BrowserActionCell(Internals)
- (void)setIconShadow;
- (void)drawBadgeWithinFrame:(NSRect)frame;
@end

@implementation BrowserActionButton

+ (Class)cellClass {
  return [BrowserActionCell class];
}

- (id)initWithExtension:(Extension*)extension
                profile:(Profile*)profile
                  tabId:(int)tabId {
  NSRect frame = NSMakeRect(0.0,
                            kBrowserActionOriginYOffset,
                            kBrowserActionWidth,
                            kBrowserActionHeight);
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
        initWithExtension:extension profile:profile] autorelease]];

    tabId_ = tabId;
    extension_ = extension;
    imageLoadingBridge_.reset(new ExtensionImageTrackerBridge(self, extension));

    [self updateState];
  }

  return self;
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

- (NSImage*)compositedImage {
  NSRect bounds = NSMakeRect(0, 0, kBrowserActionWidth, kBrowserActionHeight);
  NSBitmapImageRep* bitmap = [[NSBitmapImageRep alloc]
      initWithBitmapDataPlanes:NULL
                    pixelsWide:NSWidth(bounds)
                    pixelsHigh:NSWidth(bounds)
                 bitsPerSample:8
               samplesPerPixel:4
                      hasAlpha:YES
                      isPlanar:NO
                colorSpaceName:NSCalibratedRGBColorSpace
                  bitmapFormat:0
                   bytesPerRow:0
                  bitsPerPixel:0];

  [NSGraphicsContext saveGraphicsState];
  [NSGraphicsContext setCurrentContext:
      [NSGraphicsContext graphicsContextWithBitmapImageRep:bitmap]];
  [[self cell] setIconShadow];

  NSImage* actionImage = [self image];
  // Never draw within a flipped coordinate system.
  // TODO(andybons): Figure out why |flipped| can be yes in certain cases.
  // http://crbug.com/38943
  [actionImage setFlipped:NO];
  CGFloat xPos = floor((NSWidth(bounds) - [actionImage size].width) / 2);
  CGFloat yPos = floor((NSHeight(bounds) - [actionImage size].height) / 2);
  [actionImage drawAtPoint:NSMakePoint(xPos, yPos)
                  fromRect:NSZeroRect
                 operation:NSCompositeSourceOver
                  fraction:1.0];

  bounds.origin.y += kShadowOffset - kBrowserActionBadgeOriginYOffset;
  bounds.origin.x -= kShadowOffset;
  [[self cell] drawBadgeWithinFrame:bounds];

  [NSGraphicsContext restoreGraphicsState];
  NSImage* compositeImage =
      [[[NSImage alloc] initWithSize:[bitmap size]] autorelease];
  [compositeImage addRepresentation:bitmap];
  return compositeImage;
}

@synthesize tabId = tabId_;
@synthesize extension = extension_;

@end

@implementation BrowserActionCell

- (void)setIconShadow {
  // Create the shadow below and to the right of the drawn image.
  scoped_nsobject<NSShadow> imgShadow([[NSShadow alloc] init]);
  [imgShadow.get() setShadowOffset:NSMakeSize(kShadowOffset, -kShadowOffset)];
  [imgShadow setShadowBlurRadius:2.0];
  [imgShadow.get() setShadowColor:[[NSColor blackColor]
          colorWithAlphaComponent:0.3]];
  [imgShadow set];
}

- (void)drawBadgeWithinFrame:(NSRect)frame {
  gfx::CanvasPaint canvas(frame, false);
  canvas.set_composite_alpha(true);
  gfx::Rect boundingRect(NSRectToCGRect(frame));
  extensionAction_->PaintBadge(&canvas, boundingRect, tabId_);
}

- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  [NSGraphicsContext saveGraphicsState];
  [self setIconShadow];
  [super drawInteriorWithFrame:cellFrame inView:controlView];
  cellFrame.origin.y += kBrowserActionBadgeOriginYOffset;
  [self drawBadgeWithinFrame:cellFrame];
  [NSGraphicsContext restoreGraphicsState];
}

@synthesize tabId = tabId_;
@synthesize extensionAction = extensionAction_;

@end
