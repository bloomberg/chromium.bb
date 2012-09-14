// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/logging.h"  // for NOTREACHED()
#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"
#include "grit/ui_resources.h"
#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/api/infobars/link_infobar_delegate.h"
#import "chrome/browser/ui/cocoa/animatable_view.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/event_utils.h"
#import "chrome/browser/ui/cocoa/hyperlink_text_view.h"
#import "chrome/browser/ui/cocoa/image_button_cell.h"
#include "chrome/browser/ui/cocoa/infobars/infobar.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_container_controller.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_controller.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_gradient_view.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/gfx/image/image.h"
#include "webkit/glue/window_open_disposition.h"

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

  // Notify the delegate that the infobar was closed.  The delegate will delete
  // itself as a result of InfoBarClosed(), so we null out its pointer.
  if (delegate_) {
    delegate_->InfoBarClosed();
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


/////////////////////////////////////////////////////////////////////////
// LinkInfoBarController implementation

@implementation LinkInfoBarController

// Link infobars have a text message, of which part is linkified.  We
// use an NSAttributedString to display styled text, and we set a
// NSLink attribute on the hyperlink portion of the message.  Infobars
// use a custom NSTextField subclass, which allows us to override
// textView:clickedOnLink:atIndex: and intercept clicks.
//
- (void)addAdditionalControls {
  // No buttons.
  [self removeButtons];

  LinkInfoBarDelegate* delegate = delegate_->AsLinkInfoBarDelegate();
  DCHECK(delegate);
  size_t offset = string16::npos;
  string16 message = delegate->GetMessageTextWithOffset(&offset);
  string16 link = delegate->GetLinkText();
  NSFont* font = [NSFont labelFontOfSize:
                  [NSFont systemFontSizeForControlSize:NSRegularControlSize]];
  HyperlinkTextView* view = (HyperlinkTextView*)label_.get();
  [view setMessageAndLink:base::SysUTF16ToNSString(message)
                 withLink:base::SysUTF16ToNSString(link)
                 atOffset:offset
                     font:font
             messageColor:[NSColor blackColor]
                linkColor:[NSColor blueColor]];
}

// Called when someone clicks on the link in the infobar.  This method
// is called by the InfobarTextField on its delegate (the
// LinkInfoBarController).
- (void)linkClicked {
  if (![self isOwned])
    return;
  WindowOpenDisposition disposition =
      event_utils::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  if (delegate_->AsLinkInfoBarDelegate()->LinkClicked(disposition))
    [self removeSelf];
}

@end


/////////////////////////////////////////////////////////////////////////
// ConfirmInfoBarController implementation

@implementation ConfirmInfoBarController

// Called when someone clicks on the "OK" button.
- (IBAction)ok:(id)sender {
  if (![self isOwned])
    return;
  if (delegate_->AsConfirmInfoBarDelegate()->Accept())
    [self removeSelf];
}

// Called when someone clicks on the "Cancel" button.
- (IBAction)cancel:(id)sender {
  if (![self isOwned])
    return;
  if (delegate_->AsConfirmInfoBarDelegate()->Cancel())
    [self removeSelf];
}

// Confirm infobars can have OK and/or cancel buttons, depending on
// the return value of GetButtons().  We create each button if
// required and position them to the left of the close button.
- (void)addAdditionalControls {
  ConfirmInfoBarDelegate* delegate = delegate_->AsConfirmInfoBarDelegate();
  DCHECK(delegate);
  int visibleButtons = delegate->GetButtons();

  NSRect okButtonFrame = [okButton_ frame];
  NSRect cancelButtonFrame = [cancelButton_ frame];

  DCHECK(NSMaxX(cancelButtonFrame) < NSMinX(okButtonFrame))
      << "Ok button expected to be on the right of the Cancel button in nib";

  CGFloat rightEdge = NSMaxX(okButtonFrame);
  CGFloat spaceBetweenButtons =
      NSMinX(okButtonFrame) - NSMaxX(cancelButtonFrame);
  CGFloat spaceBeforeButtons =
      NSMinX(cancelButtonFrame) - NSMaxX([label_.get() frame]);

  // Update and position the OK button if needed.  Otherwise, hide it.
  if (visibleButtons & ConfirmInfoBarDelegate::BUTTON_OK) {
    [okButton_ setTitle:base::SysUTF16ToNSString(
          delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK))];
    [GTMUILocalizerAndLayoutTweaker sizeToFitView:okButton_];
    okButtonFrame = [okButton_ frame];

    // Position the ok button to the left of the Close button.
    okButtonFrame.origin.x = rightEdge - okButtonFrame.size.width;
    [okButton_ setFrame:okButtonFrame];

    // Update the rightEdge
    rightEdge = NSMinX(okButtonFrame);
  } else {
    [okButton_ removeFromSuperview];
    okButton_ = nil;
  }

  // Update and position the Cancel button if needed.  Otherwise, hide it.
  if (visibleButtons & ConfirmInfoBarDelegate::BUTTON_CANCEL) {
    [cancelButton_ setTitle:base::SysUTF16ToNSString(
          delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL))];
    [GTMUILocalizerAndLayoutTweaker sizeToFitView:cancelButton_];
    cancelButtonFrame = [cancelButton_ frame];

    // If we had a Ok button, leave space between the buttons.
    if (visibleButtons & ConfirmInfoBarDelegate::BUTTON_OK) {
      rightEdge -= spaceBetweenButtons;
    }

    // Position the Cancel button on our current right edge.
    cancelButtonFrame.origin.x = rightEdge - cancelButtonFrame.size.width;
    [cancelButton_ setFrame:cancelButtonFrame];

    // Update the rightEdge.
    rightEdge = NSMinX(cancelButtonFrame);
  } else {
    [cancelButton_ removeFromSuperview];
    cancelButton_ = nil;
  }

  // If we had either button, leave space before the edge of the textfield.
  if ((visibleButtons & ConfirmInfoBarDelegate::BUTTON_CANCEL) ||
      (visibleButtons & ConfirmInfoBarDelegate::BUTTON_OK)) {
    rightEdge -= spaceBeforeButtons;
  }

  NSRect frame = [label_.get() frame];
  DCHECK(rightEdge > NSMinX(frame))
      << "Need to make the xib larger to handle buttons with text this long";
  frame.size.width = rightEdge - NSMinX(frame);
  [label_.get() setFrame:frame];

  // Set the text and link.
  NSString* message = base::SysUTF16ToNSString(delegate->GetMessageText());
  string16 link = delegate->GetLinkText();
  if (!link.empty()) {
    // Add spacing between the label and the link.
    message = [message stringByAppendingString:@"   "];
  }
  NSFont* font = [NSFont labelFontOfSize:
      [NSFont systemFontSizeForControlSize:NSRegularControlSize]];
  HyperlinkTextView* view = (HyperlinkTextView*)label_.get();
  [view setMessageAndLink:message
                 withLink:base::SysUTF16ToNSString(link)
                 atOffset:[message length]
                     font:font
             messageColor:[NSColor blackColor]
                linkColor:[NSColor blueColor]];
}

// Called when someone clicks on the link in the infobar.  This method
// is called by the InfobarTextField on its delegate (the
// LinkInfoBarController).
- (void)linkClicked {
  if (![self isOwned])
    return;
  WindowOpenDisposition disposition =
      event_utils::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  if (delegate_->AsConfirmInfoBarDelegate()->LinkClicked(disposition))
    [self removeSelf];
}

@end


//////////////////////////////////////////////////////////////////////////
// CreateInfoBar() implementations

InfoBar* LinkInfoBarDelegate::CreateInfoBar(InfoBarService* owner) {
  LinkInfoBarController* controller =
      [[LinkInfoBarController alloc] initWithDelegate:this owner:owner];
  return new InfoBar(controller, this);
}

InfoBar* ConfirmInfoBarDelegate::CreateInfoBar(InfoBarService* owner) {
  ConfirmInfoBarController* controller =
      [[ConfirmInfoBarController alloc] initWithDelegate:this owner:owner];
  return new InfoBar(controller, this);
}
