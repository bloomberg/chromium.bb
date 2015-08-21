// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_pending_view_controller.h"

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/passwords/manage_password_item_view_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/controls/hyperlink_text_view.h"
#include "ui/base/l10n/l10n_util.h"

using namespace password_manager::mac::ui;

@interface ManagePasswordsBubblePendingViewController ()
- (void)onSaveClicked:(id)sender;
- (void)onNeverForThisSiteClicked:(id)sender;
@end

@implementation ManagePasswordsBubblePendingViewController

- (id)initWithModel:(ManagePasswordsBubbleModel*)model
           delegate:(id<ManagePasswordsBubbleContentViewDelegate>)delegate {
  if (([super initWithDelegate:delegate])) {
    model_ = model;
  }
  return self;
}

- (NSButton*)defaultButton {
  return saveButton_;
}

- (void)onSaveClicked:(id)sender {
  model_->OnSaveClicked();
  [delegate_ viewShouldDismiss];
}

- (void)onNeverForThisSiteClicked:(id)sender {
  model_->OnNeverForThisSiteClicked();
  [delegate_ viewShouldDismiss];
}

- (BOOL)textView:(NSTextView*)textView
    clickedOnLink:(id)link
          atIndex:(NSUInteger)charIndex {
  model_->OnBrandLinkClicked();
  [delegate_ viewShouldDismiss];
  return YES;
}

- (void)loadView {
  base::scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:NSZeroRect]);

  // -----------------------------------
  // |  Title                          |
  // |  username   password            |
  // |                [Never]  [Save]  |
  // -----------------------------------

  // The title text depends on whether the user is signed in and therefore syncs
  // their password
  // Do you want [Google Smart Lock]/Chrome/Chromium to save password
  // for this site.

  // The bubble should be wide enough to fit the title row, the username and
  // password row, and the buttons row on one line each, but not smaller than
  // kDesiredBubbleWidth.

  // Create the elements and add them to the view.

  // Title.
  titleView_.reset([[HyperlinkTextView alloc] initWithFrame:NSZeroRect]);
  NSColor* textColor = [NSColor blackColor];
  NSFont* font = ResourceBundle::GetSharedInstance()
                     .GetFontList(ResourceBundle::SmallFont)
                     .GetPrimaryFont()
                     .GetNativeFont();
  [titleView_ setMessage:base::SysUTF16ToNSString(model_->title())
                withFont:font
            messageColor:textColor];
  NSRange titleBrandLinkRange = model_->title_brand_link_range().ToNSRange();
  if (titleBrandLinkRange.length) {
    NSColor* linkColor =
        gfx::SkColorToCalibratedNSColor(chrome_style::GetLinkColor());
    [titleView_ addLinkRange:titleBrandLinkRange
                    withName:@""
                   linkColor:linkColor];
    [titleView_.get() setDelegate:self];

    // Create the link with no underlining.
    [titleView_ setLinkTextAttributes:nil];
    NSTextStorage* text = [titleView_ textStorage];
    [text addAttribute:NSUnderlineStyleAttributeName
                 value:@(NSUnderlineStyleNone)
                 range:titleBrandLinkRange];
  } else {
    // TODO(vasilii): remove if crbug.com/515189 is fixed.
    [titleView_ setRefusesFirstResponder:YES];
  }

  // Force the text to wrap to fit in the bubble size.
  [titleView_ setVerticallyResizable:YES];
  [titleView_ setFrameSize:NSMakeSize(kDesiredBubbleWidth - 2 * kFramePadding,
                                      MAXFLOAT)];
  [titleView_ sizeToFit];
  [[titleView_ textContainer] setLineFragmentPadding:0];

  [view addSubview:titleView_];

  // Password item.
  // It should be at least as wide as the box without the padding.
  passwordItem_.reset([[ManagePasswordItemViewController alloc]
      initWithModel:model_
       passwordForm:model_->pending_password()
           position:password_manager::ui::FIRST_ITEM]);
  NSView* password = [passwordItem_ view];
  [view addSubview:password];

  // Save button.
  saveButton_.reset(
      [[self addButton:l10n_util::GetNSString(IDS_PASSWORD_MANAGER_SAVE_BUTTON)
                toView:view
                target:self
                action:@selector(onSaveClicked:)] retain]);

  // Never button.
  NSString* neverButtonText =
      l10n_util::GetNSString(IDS_PASSWORD_MANAGER_BUBBLE_BLACKLIST_BUTTON1);
  neverButton_.reset(
      [[self addButton:neverButtonText
                toView:view
                target:self
                action:@selector(onNeverForThisSiteClicked:)] retain]);

  // Compute the bubble width using the password item.
  const CGFloat contentWidth =
      kFramePadding + NSWidth([titleView_ frame]) + kFramePadding;
  const CGFloat width = std::max(kDesiredBubbleWidth, contentWidth);

  // Layout the elements, starting at the bottom and moving up.

  // Buttons go on the bottom row and are right-aligned.
  // Start with [Save].
  CGFloat curX = width - kFramePadding - NSWidth([saveButton_ frame]);
  CGFloat curY = kFramePadding;
  [saveButton_ setFrameOrigin:NSMakePoint(curX, curY)];

  // [Never] goes to the left of [Save].
  curX -= kRelatedControlHorizontalPadding + NSWidth([neverButton_ frame]);
  [neverButton_ setFrameOrigin:NSMakePoint(curX, curY)];

  // Password item goes on the next row and is shifted right.
  curX = kFramePadding;
  curY = NSMaxY([saveButton_ frame]) + kUnrelatedControlVerticalPadding;
  [password setFrameOrigin:NSMakePoint(curX, curY)];

  // Title goes at the top after some padding.
  curY = NSMaxY([password frame]) + kUnrelatedControlVerticalPadding;
  [titleView_ setFrameOrigin:NSMakePoint(curX, curY)];

  // Update the bubble size.
  const CGFloat height = NSMaxY([titleView_ frame]) + kFramePadding;
  [view setFrame:NSMakeRect(0, 0, width, height)];

  [self setView:view];
}

@end

@implementation ManagePasswordsBubblePendingViewController (Testing)

- (NSButton*)saveButton {
  return saveButton_.get();
}

- (NSButton*)neverButton {
  return neverButton_.get();
}

@end
