// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#import "chrome/browser/ui/cocoa/passwords/confirmation_password_saved_view_controller.h"

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/passwords/passwords_bubble_utils.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "grit/components_strings.h"
#include "grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/controls/hyperlink_text_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font_list.h"

@interface ConfirmationPasswordSavedViewController ()
- (void)onOKClicked:(id)sender;
@end

@implementation ConfirmationPasswordSavedViewController

- (NSButton*)defaultButton {
  return okButton_;
}

- (ManagePasswordsBubbleModel*)model {
  return [self.delegate model];
}

- (void)onOKClicked:(id)sender {
  if (self.model)
    self.model->OnOKClicked();
  [self.delegate viewShouldDismiss];
}

- (BOOL)textView:(NSTextView*)textView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
  if (self.model)
    self.model->OnManageLinkClicked();
  [self.delegate viewShouldDismiss];
  return YES;
}

- (void)loadView {
  base::scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:NSZeroRect]);

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
      [self addTitleLabel:base::SysUTF16ToNSString(self.model->title())
                   toView:view];

  // Text.
  confirmationText_.reset([[HyperlinkTextView alloc] initWithFrame:NSZeroRect]);
  NSFont* font = ResourceBundle::GetSharedInstance()
      .GetFontList(ResourceBundle::SmallFont)
      .GetPrimaryFont()
      .GetNativeFont();
  NSColor* textColor = [NSColor blackColor];
  [confirmationText_ setMessage:base::SysUTF16ToNSString(
                                    self.model->save_confirmation_text())
                       withFont:font
                   messageColor:textColor];
  NSColor* linkColor =
      skia::SkColorToCalibratedNSColor(chrome_style::GetLinkColor());
  [confirmationText_
      addLinkRange:self.model->save_confirmation_link_range().ToNSRange()
           withURL:nil
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
               range:self.model->save_confirmation_link_range().ToNSRange()];
  [view addSubview:confirmationText_];

  // OK button.
  okButton_.reset([[self addButton:l10n_util::GetNSString(IDS_OK)
                            toView:view
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
  [view setFrame:NSMakeRect(0, 0, width, height)];

  [self setView:view];
}

@end

@implementation ConfirmationPasswordSavedViewController (Testing)

- (HyperlinkTextView*)confirmationText {
  return confirmationText_.get();
}

- (NSButton*)okButton {
  return okButton_.get();
}

@end
