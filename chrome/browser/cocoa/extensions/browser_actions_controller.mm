// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "browser_actions_controller.h"

#include <string>

#include "app/gfx/canvas_paint.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/cocoa/extensions/extension_action_context_menu.h"
#include "chrome/browser/cocoa/extensions/extension_popup_controller.h"
#include "chrome/browser/cocoa/toolbar_button_cell.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "skia/ext/skia_utils_mac.h"

static const CGFloat kBrowserActionBadgeOriginYOffset = 5;

// Since the container is the maximum height of the toolbar, we have to move the
// buttons up by this amount in order to have them look vertically centered
// within the toolbar.
static const CGFloat kBrowserActionOriginYOffset = 5;

// The size of each button on the toolbar.
static const CGFloat kBrowserActionHeight = 27;
extern const CGFloat kBrowserActionWidth = 29;

// The padding between browser action buttons.
extern const CGFloat kBrowserActionButtonPadding = 3;

NSString* const kBrowserActionsChangedNotification = @"BrowserActionsChanged";

@interface BrowserActionCell : ToolbarButtonCell {
 @private
  // The current tab ID used when drawing the cell.
  int tabId_;

  // The action we're drawing the cell for. Weak.
  ExtensionAction* extensionAction_;
}

@property(readwrite, nonatomic) int tabId;
@property(readwrite, nonatomic) ExtensionAction* extensionAction;

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

class ExtensionImageTrackerBridge;

@interface BrowserActionButton : NSButton {
 @private
  scoped_ptr<ExtensionImageTrackerBridge> imageLoadingBridge_;

  scoped_nsobject<NSImage> defaultIcon_;

  scoped_nsobject<NSImage> tabSpecificIcon_;

  // The extension for this button. Weak.
  Extension* extension_;

  // Weak. Owns us.
  BrowserActionsController* controller_;
}

- (id)initWithExtension:(Extension*)extension
             controller:(BrowserActionsController*)controller
                xOffset:(int)xOffset;

- (void)setDefaultIcon:(NSImage*)image;

- (void)setTabSpecificIcon:(NSImage*)image;

- (void)updateState;

@property(readonly, nonatomic) Extension* extension;

@end

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
             controller:(BrowserActionsController*)controller
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
    [cell setTabId:[controller currentTabId]];
    [cell setExtensionAction:extension->browser_action()];

    [self setTitle:@""];
    [self setButtonType:NSMomentaryChangeButton];
    [self setShowsBorderOnlyWhileMouseInside:YES];

    [self setTarget:controller];
    [self setAction:@selector(browserActionClicked:)];

    [self setMenu:[[[ExtensionActionContextMenu alloc]
        initWithExtension:extension] autorelease]];

    extension_ = extension;
    controller_ = controller;
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
  int tabId = [controller_ currentTabId];
  if (tabId < 0)
    return;

  std::string tooltip = extension_->browser_action()->GetTitle(tabId);
  if (tooltip.empty()) {
    [self setToolTip:nil];
  } else {
    [self setToolTip:base::SysUTF8ToNSString(tooltip)];
  }

  SkBitmap image = extension_->browser_action()->GetIcon(tabId);
  if (!image.isNull()) {
    [self setTabSpecificIcon:gfx::SkBitmapToNSImage(image)];
    [self setImage:tabSpecificIcon_];
  } else if (defaultIcon_) {
    [self setImage:defaultIcon_];
  }

  [[self cell] setTabId:tabId];

  [self setNeedsDisplay:YES];
}

@synthesize extension = extension_;

@end

@interface BrowserActionsController(Private)

- (void)createActionButtonForExtension:(Extension*)extension;
- (void)removeActionButtonForExtension:(Extension*)extension;
- (void)repositionActionButtons;

@end

// A helper class to proxy extension notifications to the view controller's
// appropriate methods.
class ExtensionsServiceObserverBridge : public NotificationObserver {
 public:
  ExtensionsServiceObserverBridge(BrowserActionsController* owner,
                                  Profile* profile) : owner_(owner) {
    registrar_.Add(this, NotificationType::EXTENSION_LOADED,
                   Source<Profile>(profile));
    registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                   Source<Profile>(profile));
    registrar_.Add(this, NotificationType::EXTENSION_UNLOADED_DISABLED,
                   Source<Profile>(profile));
    registrar_.Add(this, NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                   Source<Profile>(profile));
  }

  // Runs |owner_|'s method corresponding to the event type received from the
  // notification system.
  // Overridden from NotificationObserver.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details) {
    switch (type.value) {
      case NotificationType::EXTENSION_LOADED: {
        Extension* extension = Details<Extension>(details).ptr();
        [owner_ createActionButtonForExtension:extension];
        [owner_ browserActionVisibilityHasChanged];
        break;
      }
      case NotificationType::EXTENSION_UNLOADED:
      case NotificationType::EXTENSION_UNLOADED_DISABLED: {
        Extension* extension = Details<Extension>(details).ptr();
        [owner_ removeActionButtonForExtension:extension];
        [owner_ browserActionVisibilityHasChanged];
        break;
      }
      case NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE: {
        ExtensionPopupController* popup = [owner_ popup];
        if (popup && Details<ExtensionHost>([popup host]) == details)
          [owner_ hidePopup];

        break;
      }
      default:
        NOTREACHED() << L"Unexpected notification";
    }
  }

 private:
  // The object we need to inform when we get a notification. Weak. Owns us.
  BrowserActionsController* owner_;

  // Used for registering to receive notifications and automatic clean up.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsServiceObserverBridge);
};

@implementation BrowserActionsController

- (id)initWithBrowser:(Browser*)browser
        containerView:(NSView*)container {
  DCHECK(browser && container);

  if ((self = [super init])) {
    browser_ = browser;
    profile_ = browser->profile();

    containerView_ = container;
    [containerView_ setHidden:YES];
    observer_.reset(new ExtensionsServiceObserverBridge(self, profile_));
    buttons_.reset([[NSMutableDictionary alloc] init]);
    buttonOrder_.reset([[NSMutableArray alloc] init]);
  }

  return self;
}

- (void)update {
  for (BrowserActionButton* button in [buttons_ allValues]) {
    [button updateState];
  }
}

- (void)hidePopup {
  [popupController_ close];
  popupController_ = nil;
}

- (ExtensionPopupController*)popup {
  return popupController_;
}

- (void)browserActionVisibilityHasChanged {
  [containerView_ setNeedsDisplay:YES];
}

- (void)createButtons {
  ExtensionsService* extensionsService = profile_->GetExtensionsService();
  if (!extensionsService)  // |extensionsService| can be NULL in Incognito.
    return;

  for (size_t i = 0; i < extensionsService->extensions()->size(); ++i) {
    Extension* extension = extensionsService->GetExtensionById(
        extensionsService->extensions()->at(i)->id(), false);
    if (extension->browser_action()) {
      [self createActionButtonForExtension:extension];
    }
  }
}

- (void)createActionButtonForExtension:(Extension*)extension {
  if (!extension->browser_action())
    return;

  if ([buttons_ count] == 0) {
    // Only call if we're adding our first button, otherwise it will be shown
    // already.
    [containerView_ setHidden:NO];
  }

  int xOffset =
      [buttons_ count] * (kBrowserActionWidth + kBrowserActionButtonPadding);
  BrowserActionButton* newButton =
      [[[BrowserActionButton alloc] initWithExtension:extension
                                           controller:self
                                              xOffset:xOffset] autorelease];
  NSString* buttonKey = base::SysUTF8ToNSString(extension->id());
  [buttons_ setObject:newButton forKey:buttonKey];
  [buttonOrder_ addObject:newButton];
  [containerView_ addSubview:newButton];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:kBrowserActionsChangedNotification object:self];
}

- (void)removeActionButtonForExtension:(Extension*)extension {
  if (!extension->browser_action())
    return;

  NSString* buttonKey = base::SysUTF8ToNSString(extension->id());

  BrowserActionButton* button = [buttons_ objectForKey:buttonKey];
  if (!button) {
    NOTREACHED();
    return;
  }
  [button removeFromSuperview];
  [buttons_ removeObjectForKey:buttonKey];
  [buttonOrder_ removeObject:button];
  if ([buttons_ count] == 0) {
    // No more buttons? Hide the container.
    [containerView_ setHidden:YES];
  } else {
    // repositionActionButtons only needs to be called if removing a browser
    // action button because adding one will always append to the end of the
    // container, while removing one may require that those to the right of it
    // be shifted to the left.
    [self repositionActionButtons];
  }
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kBrowserActionsChangedNotification object:self];
}

- (void)repositionActionButtons {
  for (NSUInteger i = 0; i < [buttonOrder_ count]; ++i) {
    CGFloat xOffset = i * (kBrowserActionWidth + kBrowserActionButtonPadding);
    BrowserActionButton* button = [buttonOrder_ objectAtIndex:i];
    NSRect buttonFrame = [button frame];
    buttonFrame.origin.x = xOffset;
    [button setFrame:buttonFrame];
  }
}

- (int)buttonCount {
  return [buttons_ count];
}

- (void)browserActionClicked:(BrowserActionButton*)sender {
  ExtensionAction* action = [sender extension]->browser_action();
  if (action->has_popup()) {
    NSString* extensionId = base::SysUTF8ToNSString([sender extension]->id());
    // If the extension ID is not valid UTF-8, then the NSString will be nil
    // and an exception will be thrown when calling objectForKey below, hosing
    // the browser. Check it.
    DCHECK(extensionId);
    if (!extensionId)
      return;
    BrowserActionButton* actionButton = [buttons_ objectForKey:extensionId];
    NSRect relativeButtonBounds = [[[actionButton window] contentView]
        convertRect:[actionButton bounds]
           fromView:actionButton];
    NSPoint arrowPoint = [[actionButton window] convertBaseToScreen:NSMakePoint(
        NSMinX(relativeButtonBounds),
        NSMinY(relativeButtonBounds))];
    // Adjust the anchor point to be at the center of the browser action button.
    arrowPoint.x += kBrowserActionWidth / 2;

    // Close any existing (or loading) popup owned by this controller.
    if (popupController_)
      [self hidePopup];

    popupController_ = [ExtensionPopupController showURL:action->popup_url()
                                               inBrowser:browser_
                                              anchoredAt:arrowPoint
                                           arrowLocation:kTopRight];
  } else {
    ExtensionBrowserEventRouter::GetInstance()->BrowserActionExecuted(
       profile_, action->extension_id(), browser_);
  }
}

- (int)currentTabId {
  TabContents* selected_tab = browser_->GetSelectedTabContents();
  if (!selected_tab)
    return -1;

  return selected_tab->controller().session_id().id();
}

- (NSButton*)buttonWithIndex:(int)index {
  return [buttonOrder_ objectAtIndex:(NSUInteger)index];
}

@end
