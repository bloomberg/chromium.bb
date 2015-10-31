// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#import "chrome/browser/ui/cocoa/passwords/pending_password_view_controller.h"

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/hover_close_button.h"
#import "chrome/browser/ui/cocoa/passwords/passwords_list_view_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/controls/hyperlink_text_view.h"
#include "ui/base/l10n/l10n_util.h"

using namespace password_manager::mac::ui;

const SkColor kWarmWelcomeColor = SkColorSetRGB(0x64, 0x64, 0x64);

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

- (base::scoped_nsobject<NSButton>)newCloseButton {
  const int dimension = chrome_style::GetCloseButtonSize();
  NSRect frame = NSMakeRect(0, 0, dimension, dimension);
  base::scoped_nsobject<NSButton> button(
      [[WebUIHoverCloseButton alloc] initWithFrame:frame]);
  [button setAction:@selector(viewShouldDismiss)];
  [button setTarget:delegate_];
  return button;
}

- (void)loadView {
  base::scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:NSZeroRect]);

  // -----------------------------------
  // |  Title                        x |
  // |  username   password            |
  // |  Smart Lock  welcome (optional) |
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

  // Close button.
  closeButton_ = [self newCloseButton];
  [view addSubview:closeButton_];

  // Title.
  base::scoped_nsobject<HyperlinkTextView> titleView(
      [[HyperlinkTextView alloc] initWithFrame:NSZeroRect]);
  NSColor* textColor = [NSColor blackColor];
  NSFont* font = ResourceBundle::GetSharedInstance()
                     .GetFontList(ResourceBundle::SmallFont)
                     .GetPrimaryFont()
                     .GetNativeFont();
  [titleView setMessage:base::SysUTF16ToNSString(model_->title())
               withFont:font
           messageColor:textColor];
  NSRange titleBrandLinkRange = model_->title_brand_link_range().ToNSRange();
  if (titleBrandLinkRange.length) {
    NSColor* linkColor =
        gfx::SkColorToCalibratedNSColor(chrome_style::GetLinkColor());
    [titleView addLinkRange:titleBrandLinkRange
                    withURL:@"about:blank"  // using a link here is bad ui
                  linkColor:linkColor];
    [titleView.get() setDelegate:self];

    // Create the link with no underlining.
    [titleView setLinkTextAttributes:nil];
    NSTextStorage* text = [titleView textStorage];
    [text addAttribute:NSUnderlineStyleAttributeName
                 value:@(NSUnderlineStyleNone)
                 range:titleBrandLinkRange];
  } else {
    // TODO(vasilii): remove if crbug.com/515189 is fixed.
    [titleView setRefusesFirstResponder:YES];
  }

  // Force the text to wrap to fit in the bubble size.
  int titleRightPadding =
      2 * chrome_style::kCloseButtonPadding + NSWidth([closeButton_ frame]);
  int titleWidth = kDesiredBubbleWidth - kFramePadding - titleRightPadding;
  [titleView setVerticallyResizable:YES];
  [titleView setFrameSize:NSMakeSize(titleWidth, MAXFLOAT)];
  [[titleView textContainer] setLineFragmentPadding:0];
  [titleView sizeToFit];

  [view addSubview:titleView];

  // Password item.
  // It should be at least as wide as the box without the padding.
  std::vector<const autofill::PasswordForm*> password_forms;
  password_forms.push_back(&model_->pending_password());
  passwordItem_.reset([[PasswordsListViewController alloc]
      initWithModel:model_
              forms:password_forms]);
  NSView* password = [passwordItem_ view];
  [view addSubview:password];

  base::scoped_nsobject<NSTextField> warm_welcome;
  if (model_->ShouldShowGoogleSmartLockWelcome()) {
    NSString* label_text =
        l10n_util::GetNSString(IDS_PASSWORD_MANAGER_SMART_LOCK_WELCOME);
    warm_welcome.reset([[self addLabel:label_text
                                toView:view] retain]);
    [warm_welcome setFrameSize:NSMakeSize(kDesiredBubbleWidth - 2*kFramePadding,
                                          MAXFLOAT)];
    [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:warm_welcome];
    NSColor* color = gfx::SkColorToCalibratedNSColor(kWarmWelcomeColor);
    [warm_welcome setTextColor:color];
  }

  // Save button.
  saveButton_.reset(
      [[self addButton:l10n_util::GetNSString(IDS_PASSWORD_MANAGER_SAVE_BUTTON)
                toView:view
                target:self
                action:@selector(onSaveClicked:)] retain]);

  // Never button.
  NSString* neverButtonText =
      l10n_util::GetNSString(IDS_PASSWORD_MANAGER_BUBBLE_BLACKLIST_BUTTON);
  neverButton_.reset(
      [[self addButton:neverButtonText
                toView:view
                target:self
                action:@selector(onNeverForThisSiteClicked:)] retain]);

  // Compute the bubble width using the password item.
  const CGFloat contentWidth =
      kFramePadding + NSWidth([titleView frame]) + titleRightPadding;
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

  curX = kFramePadding;
  curY = NSMaxY([saveButton_ frame]) + kUnrelatedControlVerticalPadding;
  // The Smart Lock warm welcome is placed above after some padding.
  if (warm_welcome) {
    [warm_welcome setFrameOrigin:NSMakePoint(curX, curY)];
    curY = NSMaxY([warm_welcome frame]) + kUnrelatedControlVerticalPadding;
  }

  // Password item goes on the next row.
  [password setFrameOrigin:NSMakePoint(curX, curY)];

  // Title goes at the top after some padding.
  curY = NSMaxY([password frame]) + kUnrelatedControlVerticalPadding;
  [titleView setFrameOrigin:NSMakePoint(curX, curY)];
  const CGFloat height = NSMaxY([titleView frame]) + kFramePadding;

  // The close button is in the corner.
  NSPoint closeButtonOrigin = NSMakePoint(
      width - NSWidth([closeButton_ frame]) - chrome_style::kCloseButtonPadding,
      height - NSHeight([closeButton_ frame]) -
          chrome_style::kCloseButtonPadding);
  [closeButton_ setFrameOrigin:closeButtonOrigin];

  // Update the bubble size.

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

- (NSButton*)closeButton {
  return closeButton_.get();
}

@end
