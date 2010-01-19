// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/extensions/browser_action_button.h"

#include "app/gfx/canvas_paint.h"
#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/cocoa/extensions/extension_action_context_menu.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "skia/ext/skia_utils_mac.h"

static const CGFloat kBrowserActionBadgeOriginYOffset = 5;

// Since the container is the maximum height of the toolbar, we have to move the
// buttons up by this amount in order to have them look vertically centered
// within the toolbar.
static const CGFloat kBrowserActionOriginYOffset = 5;

// The size of each button on the toolbar.
static const CGFloat kBrowserActionHeight = 27;
extern const CGFloat kBrowserActionWidth = 29;

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

@implementation BrowserActionButton

+ (Class)cellClass {
  return [BrowserActionCell class];
}

- (id)initWithExtension:(Extension*)extension
                  tabId:(int)tabId
                xOffset:(int)xOffset {
  NSRect frame = NSMakeRect(xOffset,
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
        initWithExtension:extension] autorelease]];

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
}

@synthesize tabId = tabId_;
@synthesize extension = extension_;

@end

@implementation BrowserActionCell

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  [super drawWithFrame:cellFrame inView:controlView];

  // CanvasPaint draws its content to the current NSGraphicsContext in its
  // destructor. If anything needs to be drawn afterwards, then enclose this
  // in a nested block.
  cellFrame.origin.y += kBrowserActionBadgeOriginYOffset;
  gfx::CanvasPaint canvas(cellFrame, false);
  canvas.set_composite_alpha(true);
  gfx::Rect boundingRect(NSRectToCGRect(cellFrame));
  extensionAction_->PaintBadge(&canvas, boundingRect, tabId_);
}

@synthesize tabId = tabId_;
@synthesize extensionAction = extensionAction_;

@end
