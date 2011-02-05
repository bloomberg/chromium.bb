// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/extension_infobar_controller.h"

#include <cmath>

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#import "chrome/browser/ui/cocoa/animatable_view.h"
#import "chrome/browser/ui/cocoa/extensions/extension_action_context_menu.h"
#import "chrome/browser/ui/cocoa/menu_button.h"
#include "chrome/browser/ui/cocoa/infobars/infobar.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_resource.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"

namespace {
const CGFloat kAnimationDuration = 0.12;
const CGFloat kBottomBorderHeightPx = 1.0;
const CGFloat kButtonHeightPx = 26.0;
const CGFloat kButtonLeftMarginPx = 2.0;
const CGFloat kButtonWidthPx = 34.0;
const CGFloat kDropArrowLeftMarginPx = 3.0;
const CGFloat kToolbarMinHeightPx = 36.0;
const CGFloat kToolbarMaxHeightPx = 72.0;
}  // namespace

@interface ExtensionInfoBarController(Private)
// Called when the extension's hosted NSView has been resized.
- (void)extensionViewFrameChanged;
// Returns the clamped height of the extension view to be within the min and max
// values defined above.
- (CGFloat)clampedExtensionViewHeight;
// Adjusts the width of the extension's hosted view to match the window's width
// and sets the proper height for it as well.
- (void)adjustExtensionViewSize;
// Sets the image to be used in the button on the left side of the infobar.
- (void)setButtonImage:(NSImage*)image;
@end

// A helper class to bridge the asynchronous Skia bitmap loading mechanism to
// the extension's button.
class InfobarBridge : public ExtensionInfoBarDelegate::DelegateObserver,
                      public ImageLoadingTracker::Observer {
 public:
  explicit InfobarBridge(ExtensionInfoBarController* owner)
      : owner_(owner),
        delegate_([owner delegate]->AsExtensionInfoBarDelegate()),
        tracker_(this) {
    delegate_->set_observer(this);
    LoadIcon();
  }

  virtual ~InfobarBridge() {
    if (delegate_)
      delegate_->set_observer(NULL);
  }

  // Load the Extension's icon image.
  void LoadIcon() {
    const Extension* extension = delegate_->extension_host()->extension();
    ExtensionResource icon_resource = extension->GetIconResource(
        Extension::EXTENSION_ICON_BITTY, ExtensionIconSet::MATCH_EXACTLY);
    if (!icon_resource.relative_path().empty()) {
      tracker_.LoadImage(extension, icon_resource,
                         gfx::Size(Extension::EXTENSION_ICON_BITTY,
                                   Extension::EXTENSION_ICON_BITTY),
                         ImageLoadingTracker::DONT_CACHE);
    } else {
      OnImageLoaded(NULL, icon_resource, 0);
    }
  }

  // ImageLoadingTracker::Observer implementation.
  // TODO(andybons): The infobar view implementations share a lot of the same
  // code. Come up with a strategy to share amongst them.
  virtual void OnImageLoaded(
      SkBitmap* image, ExtensionResource resource, int index) {
    if (!delegate_)
      return;  // The delegate can go away while the image asynchronously loads.

    ResourceBundle& rb = ResourceBundle::GetSharedInstance();

    // Fall back on the default extension icon on failure.
    SkBitmap* icon;
    if (!image || image->empty())
      icon = rb.GetBitmapNamed(IDR_EXTENSIONS_SECTION);
    else
      icon = image;

    SkBitmap* drop_image = rb.GetBitmapNamed(IDR_APP_DROPARROW);

    const int image_size = Extension::EXTENSION_ICON_BITTY;
    scoped_ptr<gfx::CanvasSkia> canvas(
        new gfx::CanvasSkia(
            image_size + kDropArrowLeftMarginPx + drop_image->width(),
            image_size, false));
    canvas->DrawBitmapInt(*icon,
                          0, 0, icon->width(), icon->height(),
                          0, 0, image_size, image_size,
                          false);
    canvas->DrawBitmapInt(*drop_image,
                          image_size + kDropArrowLeftMarginPx,
                          image_size / 2);
    [owner_ setButtonImage:gfx::SkBitmapToNSImage(canvas->ExtractBitmap())];
  }

  // Overridden from ExtensionInfoBarDelegate::DelegateObserver:
  virtual void OnDelegateDeleted() {
    delegate_ = NULL;
  }

 private:
  // Weak. Owns us.
  ExtensionInfoBarController* owner_;

  // Weak.
  ExtensionInfoBarDelegate* delegate_;

  // Loads the extensions's icon on the file thread.
  ImageLoadingTracker tracker_;

  DISALLOW_COPY_AND_ASSIGN(InfobarBridge);
};


@implementation ExtensionInfoBarController

- (id)initWithDelegate:(InfoBarDelegate*)delegate
                window:(NSWindow*)window {
  if ((self = [super initWithDelegate:delegate])) {
    window_ = window;
    dropdownButton_.reset([[MenuButton alloc] init]);
    [dropdownButton_ setOpenMenuOnClick:YES];

    ExtensionHost* extensionHost = delegate_->AsExtensionInfoBarDelegate()->
        extension_host();
    contextMenu_.reset([[ExtensionActionContextMenu alloc]
        initWithExtension:extensionHost->extension()
                  profile:extensionHost->profile()
          extensionAction:NULL]);
    // See menu_button.h for documentation on why this is needed.
    NSMenuItem* dummyItem =
        [[[NSMenuItem alloc] initWithTitle:@""
                                    action:nil
                             keyEquivalent:@""] autorelease];
    [contextMenu_ insertItem:dummyItem atIndex:0];
    [dropdownButton_ setAttachedMenu:contextMenu_.get()];

    bridge_.reset(new InfobarBridge(self));
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)addAdditionalControls {
  [self removeButtons];

  extensionView_ = delegate_->AsExtensionInfoBarDelegate()->extension_host()->
      view()->native_view();

  // Add the extension's RenderWidgetHostViewMac to the view hierarchy of the
  // InfoBar and make sure to place it below the Close button.
  [infoBarView_ addSubview:extensionView_
                positioned:NSWindowBelow
                relativeTo:(NSView*)closeButton_];

  // Add the context menu button to the hierarchy.
  [dropdownButton_ setShowsBorderOnlyWhileMouseInside:YES];
  CGFloat buttonY =
      std::floor(NSMidY([infoBarView_ frame]) - (kButtonHeightPx / 2.0)) +
          kBottomBorderHeightPx;
  NSRect buttonFrame = NSMakeRect(
      kButtonLeftMarginPx, buttonY, kButtonWidthPx, kButtonHeightPx);
  [dropdownButton_ setFrame:buttonFrame];
  [dropdownButton_ setAutoresizingMask:NSViewMinYMargin | NSViewMaxYMargin];
  [infoBarView_ addSubview:dropdownButton_];

  // Because the parent view has a bottom border, account for it during
  // positioning.
  NSRect extensionFrame = [extensionView_ frame];
  extensionFrame.origin.y = kBottomBorderHeightPx;

  [extensionView_ setFrame:extensionFrame];
  // The extension's native view will only have a height that is non-zero if it
  // already has been loaded and rendered, which is the case when you switch
  // back to a tab with an extension infobar within it. The reason this is
  // needed is because the extension view's frame will not have changed in the
  // above case, so the NSViewFrameDidChangeNotification registered below will
  // never fire.
  if (NSHeight(extensionFrame) > 0.0) {
    NSSize infoBarSize = [[self view] frame].size;
    infoBarSize.height = [self clampedExtensionViewHeight] +
        kBottomBorderHeightPx;
    [[self view] setFrameSize:infoBarSize];
    [infoBarView_ setFrameSize:infoBarSize];
  }

  [self adjustExtensionViewSize];

  // These two notification handlers are here to ensure the width of the
  // native extension view is the same as the browser window's width and that
  // the parent infobar view matches the height of the extension's native view.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(extensionViewFrameChanged)
             name:NSViewFrameDidChangeNotification
           object:extensionView_];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(adjustWidthToFitWindow)
             name:NSWindowDidResizeNotification
           object:window_];
}

- (void)extensionViewFrameChanged {
  [self adjustExtensionViewSize];

  AnimatableView* view = [self animatableView];
  NSRect infoBarFrame = [view frame];
  CGFloat newHeight = [self clampedExtensionViewHeight] + kBottomBorderHeightPx;
  [infoBarView_ setPostsFrameChangedNotifications:NO];
  infoBarFrame.size.height = newHeight;
  [infoBarView_ setFrame:infoBarFrame];
  [infoBarView_ setPostsFrameChangedNotifications:YES];
  [view animateToNewHeight:newHeight duration:kAnimationDuration];
}

- (CGFloat)clampedExtensionViewHeight {
  return std::max(kToolbarMinHeightPx,
      std::min(NSHeight([extensionView_ frame]), kToolbarMaxHeightPx));
}

- (void)adjustExtensionViewSize {
  [extensionView_ setPostsFrameChangedNotifications:NO];
  NSSize extensionViewSize = [extensionView_ frame].size;
  extensionViewSize.width = NSWidth([window_ frame]);
  extensionViewSize.height = [self clampedExtensionViewHeight];
  [extensionView_ setFrameSize:extensionViewSize];
  [extensionView_ setPostsFrameChangedNotifications:YES];
}

- (void)setButtonImage:(NSImage*)image {
  [dropdownButton_ setImage:image];
}

@end

InfoBar* ExtensionInfoBarDelegate::CreateInfoBar() {
  NSWindow* window = [(NSView*)tab_contents_->GetContentNativeView() window];
  ExtensionInfoBarController* controller =
      [[ExtensionInfoBarController alloc] initWithDelegate:this
                                                    window:window];
  return new InfoBar(controller);
}
