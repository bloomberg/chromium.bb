// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/infobars/extension_infobar_controller.h"

#include <cmath>

#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_infobar_delegate.h"
#include "chrome/browser/extensions/image_loader.h"
#include "chrome/browser/ui/browser_finder.h"
#import "chrome/browser/ui/cocoa/animatable_view.h"
#import "chrome/browser/ui/cocoa/extensions/extension_action_context_menu.h"
#include "chrome/browser/ui/cocoa/infobars/infobar.h"
#import "chrome/browser/ui/cocoa/menu_button.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_resource.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"

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
class InfobarBridge : public ExtensionInfoBarDelegate::DelegateObserver {
 public:
  explicit InfobarBridge(ExtensionInfoBarController* owner)
      : owner_(owner),
        delegate_([owner delegate]->AsExtensionInfoBarDelegate()),
        ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
    delegate_->set_observer(this);
    LoadIcon();
  }

  virtual ~InfobarBridge() {
    if (delegate_)
      delegate_->set_observer(NULL);
  }

  // Load the Extension's icon image.
  void LoadIcon() {
    const extensions::Extension* extension = delegate_->extension_host()->
        extension();
    ExtensionResource icon_resource =
        extension->GetIconResource(extension_misc::EXTENSION_ICON_BITTY,
                                   ExtensionIconSet::MATCH_EXACTLY);
    extensions::ImageLoader* loader =
        extensions::ImageLoader::Get(delegate_->extension_host()->profile());
    loader->LoadImageAsync(extension, icon_resource,
                           gfx::Size(extension_misc::EXTENSION_ICON_BITTY,
                                     extension_misc::EXTENSION_ICON_BITTY),
                           base::Bind(&InfobarBridge::OnImageLoaded,
                                      weak_ptr_factory_.GetWeakPtr()));
  }

  // ImageLoadingTracker::Observer implementation.
  // TODO(andybons): The infobar view implementations share a lot of the same
  // code. Come up with a strategy to share amongst them.
  void OnImageLoaded(const gfx::Image& image) {
    if (!delegate_)
      return;  // The delegate can go away while the image asynchronously loads.

    ResourceBundle& rb = ResourceBundle::GetSharedInstance();

    // Fall back on the default extension icon on failure.
    const gfx::ImageSkia* icon;
    if (image.IsEmpty())
      icon = rb.GetImageSkiaNamed(IDR_EXTENSIONS_SECTION);
    else
      icon = image.ToImageSkia();

    gfx::ImageSkia* drop_image = rb.GetImageSkiaNamed(IDR_APP_DROPARROW);

    const int image_size = extension_misc::EXTENSION_ICON_BITTY;
    scoped_ptr<gfx::Canvas> canvas(
        new gfx::Canvas(
            gfx::Size(image_size + kDropArrowLeftMarginPx + drop_image->width(),
                      image_size), ui::SCALE_FACTOR_100P, false));
    canvas->DrawImageInt(*icon,
                         0, 0, icon->width(), icon->height(),
                         0, 0, image_size, image_size,
                         false);
    canvas->DrawImageInt(*drop_image,
                         image_size + kDropArrowLeftMarginPx,
                         image_size / 2);
    [owner_ setButtonImage:gfx::SkBitmapToNSImage(
        canvas->ExtractImageRep().sk_bitmap())];
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

  base::WeakPtrFactory<InfobarBridge> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(InfobarBridge);
};


@implementation ExtensionInfoBarController

- (id)initWithDelegate:(InfoBarDelegate*)delegate
                 owner:(InfoBarService*)owner
                window:(NSWindow*)window {
  if ((self = [super initWithDelegate:delegate owner:owner])) {
    window_ = window;
    dropdownButton_.reset([[MenuButton alloc] init]);
    [dropdownButton_ setOpenMenuOnClick:YES];

    extensions::ExtensionHost* extensionHost =
        delegate_->AsExtensionInfoBarDelegate()->extension_host();
    Browser* browser =
        chrome::FindBrowserWithWebContents(owner->GetWebContents());
    contextMenu_.reset([[ExtensionActionContextMenu alloc]
        initWithExtension:extensionHost->extension()
                  browser:browser
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

  // Add the extension's RenderWidgetHostView to the view hierarchy of the
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
         selector:@selector(adjustExtensionViewSize)
             name:NSWindowDidResizeNotification
           object:window_];
}

- (void)infobarWillClose {
  [self disablePopUpMenu:contextMenu_.get()];
  [super infobarWillClose];
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
  CGFloat height = delegate_->AsExtensionInfoBarDelegate()->height();
  return std::max(kToolbarMinHeightPx, std::min(height, kToolbarMaxHeightPx));
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

InfoBar* ExtensionInfoBarDelegate::CreateInfoBar(InfoBarService* owner) {
  NSWindow* window =
      [(NSView*)owner->GetWebContents()->GetContentNativeView() window];
  ExtensionInfoBarController* controller =
      [[ExtensionInfoBarController alloc] initWithDelegate:this
                                                     owner:owner
                                                    window:window];
  return new InfoBar(controller, this);
}
