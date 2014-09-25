// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_confirmation_view_controller.h"

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/chrome_style.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/controls/hyperlink_text_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font_list.h"

using namespace password_manager::mac::ui;

@interface ManagePasswordsBubbleConfirmationViewController ()
- (void)onOKClicked:(id)sender;
@end

@implementation ManagePasswordsBubbleConfirmationViewController

- (id)initWithModel:(ManagePasswordsBubbleModel*)model
           delegate:(id<ManagePasswordsBubbleContentViewDelegate>)delegate {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    model_ = model;
    delegate_ = delegate;
  }
  return self;
}

- (void)onOKClicked:(id)sender {
  model_->OnOKClicked();
  [delegate_ viewShouldDismiss];
}

- (BOOL)textView:(NSTextView*)textView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
  model_->OnManageLinkClicked();
  [delegate_ viewShouldDismiss];
  return YES;
}

- (void)loadView {
  self.view = [[[NSView alloc] initWithFrame:NSZeroRect] autorelease];

  // -----------------------------------
  // |  Title                          |
  // |                                 |
  // |  Saved! Click [here] to view.   |
  // |                                 |
  // |                           [OK]  |
  // -----------------------------------

  // Create the elements and add them to the view.

  // Title.
  NSTextField* titleLabel =
      [self addTitleLabel:base::SysUTF16ToNSString(model_->title())];

  // Text.
  confirmationText_.reset([[HyperlinkTextView alloc] initWithFrame:NSZeroRect]);
  NSFont* font = ResourceBundle::GetSharedInstance()
      .GetFontList(ResourceBundle::SmallFont)
      .GetPrimaryFont()
      .GetNativeFont();
  NSColor* textColor = [NSColor blackColor];
  [confirmationText_
        setMessage:base::SysUTF16ToNSString(model_->save_confirmation_text())
          withFont:font
      messageColor:textColor];
  NSColor* linkColor =
      gfx::SkColorToCalibratedNSColor(chrome_style::GetLinkColor());
  [confirmationText_
      addLinkRange:model_->save_confirmation_link_range().ToNSRange()
          withName:@""
         linkColor:linkColor];
  [confirmationText_ setDelegate:self];
  [[confirmationText_ textContainer] setLineFragmentPadding:0.0f];
  // Force the text to wrap to fit in the bubble size.
  [confirmationText_ setVerticallyResizable:YES];
  [confirmationText_
      setFrameSize:NSMakeSize(kDesiredBubbleWidth - 2 * kFramePadding,
                              MAXFLOAT)];
  [confirmationText_ sizeToFit];
  // Create the link with no underlining.
  [confirmationText_ setLinkTextAttributes:nil];
  NSTextStorage* text = [confirmationText_ textStorage];
  [text addAttribute:NSUnderlineStyleAttributeName
               value:[NSNumber numberWithInt:NSUnderlineStyleNone]
               range:model_->save_confirmation_link_range().ToNSRange()];
  [[self view] addSubview:confirmationText_];

  // OK button.
  okButton_.reset([[self addButton:l10n_util::GetNSString(IDS_OK)
                            target:self
                            action:@selector(onOKClicked:)] retain]);

  // Layout the elements, starting at the bottom and moving up.
  const CGFloat width = kDesiredBubbleWidth;

  // OK button goes on the bottom row and is right-aligned.
  CGFloat curX = width - kFramePadding - NSWidth([okButton_ frame]);
  CGFloat curY = kFramePadding;
  [okButton_ setFrameOrigin:NSMakePoint(curX, curY)];

  // Text goes on the next row and is shifted right.
  curX = kFramePadding;
  curY = NSMaxY([okButton_ frame]) + kUnrelatedControlVerticalPadding;
  [confirmationText_ setFrameOrigin:NSMakePoint(curX, curY)];

  // Title goes at the top after some padding.
  curY = NSMaxY([confirmationText_ frame]) + kUnrelatedControlVerticalPadding;
  [titleLabel setFrameOrigin:NSMakePoint(curX, curY)];

  // Update the bubble size.
  const CGFloat height = NSMaxY([titleLabel frame]) + kFramePadding;
  [self.view setFrame:NSMakeRect(0, 0, width, height)];
}

@end

@implementation ManagePasswordsBubbleConfirmationViewController (Testing)

- (HyperlinkTextView*)confirmationText {
  return confirmationText_.get();
}

- (NSButton*)okButton {
  return okButton_.get();
}

@end
