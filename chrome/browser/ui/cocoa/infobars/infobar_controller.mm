// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/logging.h"  // for NOTREACHED()
#include "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#import "chrome/browser/ui/cocoa/animatable_view.h"
#include "chrome/browser/ui/cocoa/event_utils.h"
#include "chrome/browser/ui/cocoa/infobars/infobar.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_container_controller.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_controller.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "webkit/glue/window_open_disposition.h"

namespace {
// Durations set to match the default SlideAnimation duration.
const float kAnimateOpenDuration = 0.12;
const float kAnimateCloseDuration = 0.12;
}

// This simple subclass of |NSTextView| just doesn't show the (text) cursor
// (|NSTextView| displays the cursor with full keyboard accessibility enabled).
@interface InfoBarTextView : NSTextView
- (void)fixupCursor;
@end

@implementation InfoBarTextView

// Never draw the insertion point (otherwise, it shows up without any user
// action if full keyboard accessibility is enabled).
- (BOOL)shouldDrawInsertionPoint {
  return NO;
}

- (NSRange)selectionRangeForProposedRange:(NSRange)proposedSelRange
                              granularity:(NSSelectionGranularity)granularity {
  // Do not allow selections.
  return NSMakeRange(0, 0);
}

// Convince NSTextView to not show an I-Beam cursor when the cursor is over the
// text view but not over actual text.
//
// http://www.mail-archive.com/cocoa-dev@lists.apple.com/msg10791.html
// "NSTextView sets the cursor over itself dynamically, based on considerations
// including the text under the cursor. It does so in -mouseEntered:,
// -mouseMoved:, and -cursorUpdate:, so those would be points to consider
// overriding."
- (void)mouseMoved:(NSEvent*)e {
  [super mouseMoved:e];
  [self fixupCursor];
}

- (void)mouseEntered:(NSEvent*)e {
  [super mouseEntered:e];
  [self fixupCursor];
}

- (void)cursorUpdate:(NSEvent*)e {
  [super cursorUpdate:e];
  [self fixupCursor];
}

- (void)fixupCursor {
  if ([[NSCursor currentCursor] isEqual:[NSCursor IBeamCursor]])
    [[NSCursor arrowCursor] set];
}

@end

@interface InfoBarController (PrivateMethods)
// Sets |label_| based on |labelPlaceholder_|, sets |labelPlaceholder_| to nil.
- (void)initializeLabel;

// Asks the container controller to remove the infobar for this delegate.  This
// call will trigger a notification that starts the infobar animating closed.
- (void)removeInfoBar;

// Performs final cleanup after an animation is finished or stopped, including
// notifying the InfoBarDelegate that the infobar was closed and removing the
// infobar from its container, if necessary.
- (void)cleanUpAfterAnimation:(BOOL)finished;

// Sets the info bar message to the specified |message|, with a hypertext
// style link. |link| will be inserted into message at |linkOffset|.
- (void)setLabelToMessage:(NSString*)message
                 withLink:(NSString*)link
                 atOffset:(NSUInteger)linkOffset;
@end

@implementation InfoBarController

@synthesize containerController = containerController_;
@synthesize delegate = delegate_;

- (id)initWithDelegate:(InfoBarDelegate*)delegate {
  DCHECK(delegate);
  if ((self = [super initWithNibName:@"InfoBar"
                              bundle:base::mac::MainAppBundle()])) {
    delegate_ = delegate;
  }
  return self;
}

// All infobars have an icon, so we set up the icon in the base class
// awakeFromNib.
- (void)awakeFromNib {
  DCHECK(delegate_);
  if (delegate_->GetIcon()) {
    [image_ setImage:gfx::SkBitmapToNSImage(*(delegate_->GetIcon()))];
  } else {
    // No icon, remove it from the view and grow the textfield to include the
    // space.
    NSRect imageFrame = [image_ frame];
    NSRect labelFrame = [labelPlaceholder_ frame];
    labelFrame.size.width += NSMinX(imageFrame) - NSMinX(labelFrame);
    labelFrame.origin.x = imageFrame.origin.x;
    [image_ removeFromSuperview];
    [labelPlaceholder_ setFrame:labelFrame];
  }
  [self initializeLabel];

  [self addAdditionalControls];
}

// Called when someone clicks on the embedded link.
- (BOOL) textView:(NSTextView*)textView
    clickedOnLink:(id)link
          atIndex:(NSUInteger)charIndex {
  if ([self respondsToSelector:@selector(linkClicked)])
    [self performSelector:@selector(linkClicked)];
  return YES;
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
  [self removeInfoBar];
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
  // Slide the origin down so it doesn't animate on top of the toolbar, but
  // rather just in the content area.
  frame.origin.y -= infobars::kAntiSpoofHeight;
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

  // Take out the anti-spoof height so that the animation does not jump.
  NSRect frame = [[self view] frame];
  frame.size.height -= infobars::kAntiSpoofHeight;
  [[self view] setFrame:frame];

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
  // Default implementation does nothing.
}

- (void)setLabelToMessage:(NSString*)message {
  NSMutableDictionary* attributes = [NSMutableDictionary dictionary];
  NSFont* font = [NSFont labelFontOfSize:
      [NSFont systemFontSizeForControlSize:NSRegularControlSize]];
  [attributes setObject:font
                 forKey:NSFontAttributeName];
  [attributes setObject:[NSCursor arrowCursor]
                 forKey:NSCursorAttributeName];
  scoped_nsobject<NSAttributedString> attributedString(
      [[NSAttributedString alloc] initWithString:message
                                      attributes:attributes]);
  [[label_.get() textStorage] setAttributedString:attributedString];
}

- (void)removeButtons {
  // Extend the label all the way across.
  NSRect labelFrame = [label_.get() frame];
  labelFrame.size.width = NSMaxX([cancelButton_ frame]) - NSMinX(labelFrame);
  [okButton_ removeFromSuperview];
  [cancelButton_ removeFromSuperview];
  [label_.get() setFrame:labelFrame];
}

@end

@implementation InfoBarController (PrivateMethods)

- (void)initializeLabel {
  // Replace the label placeholder NSTextField with the real label NSTextView.
  // The former doesn't show links in a nice way, but the latter can't be added
  // in IB without a containing scroll view, so create the NSTextView
  // programmatically.
  label_.reset([[InfoBarTextView alloc]
      initWithFrame:[labelPlaceholder_ frame]]);
  [label_.get() setAutoresizingMask:[labelPlaceholder_ autoresizingMask]];
  [[labelPlaceholder_ superview]
      replaceSubview:labelPlaceholder_ with:label_.get()];
  labelPlaceholder_ = nil;  // Now released.
  [label_.get() setDelegate:self];
  [label_.get() setEditable:NO];
  [label_.get() setDrawsBackground:NO];
  [label_.get() setHorizontallyResizable:NO];
  [label_.get() setVerticallyResizable:NO];
}

- (void)removeInfoBar {
  // TODO(rohitrao): This method can be called even if the infobar has already
  // been removed and |delegate_| is NULL.  Is there a way to rewrite the code
  // so that inner event loops don't cause us to try and remove the infobar
  // twice?  http://crbug.com/54253
  [containerController_ removeDelegate:delegate_];
}

- (void)cleanUpAfterAnimation:(BOOL)finished {
  // Don't need to do any cleanup if the bar was animating open.
  if (!infoBarClosing_)
    return;

  // Notify the delegate that the infobar was closed.  The delegate may delete
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

// TODO(joth): This method factors out some common functionality between the
// various derived infobar classes, however the class hierarchy itself could
// use refactoring to reduce this duplication. http://crbug.com/38924
- (void)setLabelToMessage:(NSString*)message
                 withLink:(NSString*)link
                 atOffset:(NSUInteger)linkOffset {
  if (linkOffset == std::wstring::npos) {
    // linkOffset == std::wstring::npos means the link should be right-aligned,
    // which is not supported on Mac (http://crbug.com/47728).
    NOTIMPLEMENTED();
    linkOffset = [message length];
  }
  // Create an attributes dictionary for the entire message.  We have
  // to expicitly set the font the control's font.  We also override
  // the cursor to give us the normal cursor rather than the text
  // insertion cursor.
  NSMutableDictionary* linkAttributes = [NSMutableDictionary dictionary];
  [linkAttributes setObject:[NSCursor arrowCursor]
                     forKey:NSCursorAttributeName];
  NSFont* font = [NSFont labelFontOfSize:
      [NSFont systemFontSizeForControlSize:NSRegularControlSize]];
  [linkAttributes setObject:font
                     forKey:NSFontAttributeName];

  // Create the attributed string for the main message text.
  scoped_nsobject<NSMutableAttributedString> infoText(
      [[NSMutableAttributedString alloc] initWithString:message]);
  [infoText.get() addAttributes:linkAttributes
                    range:NSMakeRange(0, [infoText.get() length])];
  // Add additional attributes to style the link text appropriately as
  // well as linkify it.
  [linkAttributes setObject:[NSColor blueColor]
                     forKey:NSForegroundColorAttributeName];
  [linkAttributes setObject:[NSNumber numberWithBool:YES]
                     forKey:NSUnderlineStyleAttributeName];
  [linkAttributes setObject:[NSCursor pointingHandCursor]
                     forKey:NSCursorAttributeName];
  [linkAttributes setObject:[NSNumber numberWithInt:NSSingleUnderlineStyle]
                     forKey:NSUnderlineStyleAttributeName];
  [linkAttributes setObject:[NSString string]  // dummy value
                     forKey:NSLinkAttributeName];

  // Insert the link text into the string at the appropriate offset.
  scoped_nsobject<NSAttributedString> attributedString(
      [[NSAttributedString alloc] initWithString:link
                                      attributes:linkAttributes]);
  [infoText.get() insertAttributedString:attributedString.get()
                                 atIndex:linkOffset];
  // Update the label view with the new text.
  [[label_.get() textStorage] setAttributedString:infoText];
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
  size_t offset = std::wstring::npos;
  string16 message = delegate->GetMessageTextWithOffset(&offset);
  [self setLabelToMessage:base::SysUTF16ToNSString(message)
                 withLink:base::SysUTF16ToNSString(delegate->GetLinkText())
                 atOffset:offset];
}

// Called when someone clicks on the link in the infobar.  This method
// is called by the InfobarTextField on its delegate (the
// LinkInfoBarController).
- (void)linkClicked {
  WindowOpenDisposition disposition =
      event_utils::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  if (delegate_ && delegate_->AsLinkInfoBarDelegate()->LinkClicked(disposition))
    [self removeInfoBar];
}

@end


/////////////////////////////////////////////////////////////////////////
// ConfirmInfoBarController implementation

@implementation ConfirmInfoBarController

// Called when someone clicks on the "OK" button.
- (IBAction)ok:(id)sender {
  if (delegate_ && delegate_->AsConfirmInfoBarDelegate()->Accept())
    [self removeInfoBar];
}

// Called when someone clicks on the "Cancel" button.
- (IBAction)cancel:(id)sender {
  if (delegate_ && delegate_->AsConfirmInfoBarDelegate()->Cancel())
    [self removeInfoBar];
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

  DCHECK(NSMaxX(okButtonFrame) < NSMinX(cancelButtonFrame))
      << "Cancel button expected to be on the right of the Ok button in nib";

  CGFloat rightEdge = NSMaxX(cancelButtonFrame);
  CGFloat spaceBetweenButtons =
      NSMinX(cancelButtonFrame) - NSMaxX(okButtonFrame);
  CGFloat spaceBeforeButtons =
      NSMinX(okButtonFrame) - NSMaxX([label_.get() frame]);

  // Update and position the Cancel button if needed.  Otherwise, hide it.
  if (visibleButtons & ConfirmInfoBarDelegate::BUTTON_CANCEL) {
    [cancelButton_ setTitle:base::SysUTF16ToNSString(
          delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL))];
    [GTMUILocalizerAndLayoutTweaker sizeToFitView:cancelButton_];
    cancelButtonFrame = [cancelButton_ frame];

    // Position the cancel button to the left of the Close button.
    cancelButtonFrame.origin.x = rightEdge - cancelButtonFrame.size.width;
    [cancelButton_ setFrame:cancelButtonFrame];

    // Update the rightEdge
    rightEdge = NSMinX(cancelButtonFrame);
  } else {
    [cancelButton_ removeFromSuperview];
  }

  // Update and position the OK button if needed.  Otherwise, hide it.
  if (visibleButtons & ConfirmInfoBarDelegate::BUTTON_OK) {
    [okButton_ setTitle:base::SysUTF16ToNSString(
          delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK))];
    [GTMUILocalizerAndLayoutTweaker sizeToFitView:okButton_];
    okButtonFrame = [okButton_ frame];

    // If we had a Cancel button, leave space between the buttons.
    if (visibleButtons & ConfirmInfoBarDelegate::BUTTON_CANCEL) {
      rightEdge -= spaceBetweenButtons;
    }

    // Position the OK button on our current right edge.
    okButtonFrame.origin.x = rightEdge - okButtonFrame.size.width;
    [okButton_ setFrame:okButtonFrame];


    // Update the rightEdge
    rightEdge = NSMinX(okButtonFrame);
  } else {
    [okButton_ removeFromSuperview];
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
  if (link.empty()) {
    // Simple case: no link, so just set the message directly.
    [self setLabelToMessage:message];
  } else {
    // Inserting the link unintentionally causes the text to have a slightly
    // different result to the simple case above: text is truncated on word
    // boundaries (if needed) rather than elided with ellipses.

    // Add spacing between the label and the link.
    message = [message stringByAppendingString:@"   "];
    [self setLabelToMessage:message
                   withLink:base::SysUTF16ToNSString(link)
                   atOffset:[message length]];
  }
}

// Called when someone clicks on the link in the infobar.  This method
// is called by the InfobarTextField on its delegate (the
// LinkInfoBarController).
- (void)linkClicked {
  WindowOpenDisposition disposition =
      event_utils::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  if (delegate_ &&
      delegate_->AsConfirmInfoBarDelegate()->LinkClicked(disposition))
    [self removeInfoBar];
}

@end


//////////////////////////////////////////////////////////////////////////
// CreateInfoBar() implementations

InfoBar* LinkInfoBarDelegate::CreateInfoBar() {
  LinkInfoBarController* controller =
      [[LinkInfoBarController alloc] initWithDelegate:this];
  return new InfoBar(controller);
}

InfoBar* ConfirmInfoBarDelegate::CreateInfoBar() {
  ConfirmInfoBarController* controller =
      [[ConfirmInfoBarController alloc] initWithDelegate:this];
  return new InfoBar(controller);
}
