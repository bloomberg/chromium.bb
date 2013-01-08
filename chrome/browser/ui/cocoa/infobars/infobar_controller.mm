// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/infobars/infobar_controller.h"

#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "grit/ui_resources.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#import "chrome/browser/ui/cocoa/animatable_view.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/hyperlink_text_view.h"
#import "chrome/browser/ui/cocoa/image_button_cell.h"
#include "chrome/browser/ui/cocoa/infobars/infobar.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_container_controller.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_gradient_view.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "ui/gfx/image/image.h"

namespace {
// Durations set to match the default SlideAnimation duration.
const float kAnimateOpenDuration = 0.12;
const float kAnimateCloseDuration = 0.12;
}

@interface InfoBarController (PrivateMethods)
// Sets |label_| based on |labelPlaceholder_|, sets |labelPlaceholder_| to nil.
- (void)initializeLabel;

// Performs final cleanup after an animation is finished or stopped, including
// notifying the InfoBarDelegate that the infobar was closed and removing the
// infobar from its container, if necessary.
- (void)cleanUpAfterAnimation:(BOOL)finished;

// Returns the point, in window coordinates, at which the apex of the infobar
// tip should be drawn.
- (NSPoint)pointForTipApex;
@end

@implementation InfoBarController

@synthesize containerController = containerController_;
@synthesize delegate = delegate_;

- (id)initWithDelegate:(InfoBarDelegate*)delegate
                 owner:(InfoBarService*)owner {
  DCHECK(delegate);
  if ((self = [super initWithNibName:@"InfoBar"
                              bundle:base::mac::FrameworkBundle()])) {
    delegate_ = delegate;
    owner_ = owner;
  }
  return self;
}

// All infobars have an icon, so we set up the icon in the base class
// awakeFromNib.
- (void)awakeFromNib {
  DCHECK(delegate_);

  [[closeButton_ cell] setImageID:IDR_CLOSE_BAR
                   forButtonState:image_button_cell::kDefaultState];
  [[closeButton_ cell] setImageID:IDR_CLOSE_BAR_H
                   forButtonState:image_button_cell::kHoverState];
  [[closeButton_ cell] setImageID:IDR_CLOSE_BAR_P
                   forButtonState:image_button_cell::kPressedState];
  [[closeButton_ cell] setImageID:IDR_CLOSE_BAR
                   forButtonState:image_button_cell::kDisabledState];

  if (delegate_->GetIcon()) {
    [image_ setImage:delegate_->GetIcon()->ToNSImage()];
  } else {
    // No icon, remove it from the view and grow the textfield to include the
    // space.
    NSRect imageFrame = [image_ frame];
    NSRect labelFrame = [labelPlaceholder_ frame];
    labelFrame.size.width += NSMinX(imageFrame) - NSMinX(labelFrame);
    labelFrame.origin.x = imageFrame.origin.x;
    [image_ removeFromSuperview];
    image_ = nil;
    [labelPlaceholder_ setFrame:labelFrame];
  }
  [self initializeLabel];

  [self addAdditionalControls];

  infoBarView_.tipApex = [self pointForTipApex];
  [infoBarView_ setInfobarType:delegate_->GetInfoBarType()];
}

- (void)dealloc {
  [okButton_ setTarget:nil];
  [cancelButton_ setTarget:nil];
  [closeButton_ setTarget:nil];
  [super dealloc];
}

// Called when someone clicks on the embedded link.
- (BOOL)textView:(NSTextView*)textView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
  if ([self respondsToSelector:@selector(linkClicked)])
    [self performSelector:@selector(linkClicked)];
  return YES;
}

- (BOOL)isOwned {
  return !!owner_;
}

// Called when someone clicks on the ok button.
- (void)ok:(id)sender {
  // Subclasses must override this method if they do not hide the ok button.
  NOTREACHED();
}

// Called when someone clicks on the cancel button.
- (void)cancel:(id)sender {
  // Subclasses must override this method if they do not hide the cancel button.
  NOTREACHED();
}

// Called when someone clicks on the close button.
- (void)dismiss:(id)sender {
  if (![self isOwned])
    return;
  delegate_->InfoBarDismissed();
  [self removeSelf];
}

- (void)removeSelf {
  // |owner_| should never be NULL here.  If it is, then someone violated what
  // they were supposed to do -- e.g. a ConfirmInfoBarDelegate subclass returned
  // true from Accept() or Cancel() even though the infobar was already closing.
  // In the worst case, if we also switched tabs during that process, then
  // |this| has already been destroyed.  But if that's the case, then we're
  // going to deref a garbage |this| pointer here whether we check |owner_| or
  // not, and in other cases (where we're still closing and |this| is valid),
  // checking |owner_| here will avoid a NULL deref.
  if (owner_)
    owner_->RemoveInfoBar(delegate_);
}

- (AnimatableView*)animatableView {
  return static_cast<AnimatableView*>([self view]);
}

- (void)open {
  // Simply reset the frame size to its opened size, forcing a relayout.
  CGFloat finalHeight = [[self view] frame].size.height;
  [[self animatableView] setHeight:finalHeight];
}

- (void)animateOpen {
  // Force the frame size to be 0 and then start an animation.
  NSRect frame = [[self view] frame];
  CGFloat finalHeight = frame.size.height;
  frame.size.height = 0;
  [[self view] setFrame:frame];
  [[self animatableView] animateToNewHeight:finalHeight
                                   duration:kAnimateOpenDuration];
}

- (void)close {
  // Stop any running animations.
  [[self animatableView] stopAnimation];
  infoBarClosing_ = YES;
  [self cleanUpAfterAnimation:YES];
}

- (void)animateClosed {
  // Notify the container of our intentions.
  [containerController_ willRemoveController:self];

  // Start animating closed.  We will receive a notification when the animation
  // is done, at which point we can remove our view from the hierarchy and
  // notify the delegate that the infobar was closed.
  [[self animatableView] animateToNewHeight:0 duration:kAnimateCloseDuration];

  // The above call may trigger an animationDidStop: notification for any
  // currently-running animations, so do not set |infoBarClosing_| until after
  // starting the animation.
  infoBarClosing_ = YES;
}

- (void)addAdditionalControls {
  // Default implementation does nothing.
}

- (void)infobarWillClose {
  owner_ = NULL;
}

- (void)removeButtons {
  // Extend the label all the way across.
  NSRect labelFrame = [label_.get() frame];
  labelFrame.size.width = NSMaxX([cancelButton_ frame]) - NSMinX(labelFrame);
  [okButton_ removeFromSuperview];
  okButton_ = nil;
  [cancelButton_ removeFromSuperview];
  cancelButton_ = nil;
  [label_.get() setFrame:labelFrame];
}

- (void)disablePopUpMenu:(NSMenu*)menu {
  // Remove the menu if visible.
  [menu cancelTracking];

  // If the menu is re-opened, prevent queries to update items.
  [menu setDelegate:nil];

  // Prevent target/action messages to the controller.
  for (NSMenuItem* item in [menu itemArray]) {
    [item setEnabled:NO];
    [item setTarget:nil];
  }
}

@end

@implementation InfoBarController (PrivateMethods)

- (void)initializeLabel {
  // Replace the label placeholder NSTextField with the real label NSTextView.
  // The former doesn't show links in a nice way, but the latter can't be added
  // in IB without a containing scroll view, so create the NSTextView
  // programmatically.
  label_.reset([[HyperlinkTextView alloc]
      initWithFrame:[labelPlaceholder_ frame]]);
  [label_.get() setAutoresizingMask:[labelPlaceholder_ autoresizingMask]];
  [[labelPlaceholder_ superview]
      replaceSubview:labelPlaceholder_ with:label_.get()];
  labelPlaceholder_ = nil;  // Now released.
  [label_.get() setDelegate:self];
}

- (void)cleanUpAfterAnimation:(BOOL)finished {
  // Don't need to do any cleanup if the bar was animating open.
  if (!infoBarClosing_)
    return;

  if (delegate_) {
    delete delegate_;
    delegate_ = NULL;
  }

  // If the animation ran to completion, then we need to remove ourselves from
  // the container.  If the animation was interrupted, then the container will
  // take care of removing us.
  // TODO(rohitrao): UGH!  This works for now, but should be cleaner.
  if (finished)
    [containerController_ removeController:self];
}

- (void)animationDidStop:(NSAnimation*)animation {
  [self cleanUpAfterAnimation:NO];
}

- (void)animationDidEnd:(NSAnimation*)animation {
  [self cleanUpAfterAnimation:YES];
}

- (NSPoint)pointForTipApex {
  BrowserWindowController* windowController =
      [containerController_ browserWindowController];
  if (!windowController) {
    // This should only happen in unit tests.
    return NSZeroPoint;
  }

  LocationBarViewMac* locationBar = [windowController locationBarBridge];
  return locationBar->GetPageInfoBubblePoint();
}

@end
