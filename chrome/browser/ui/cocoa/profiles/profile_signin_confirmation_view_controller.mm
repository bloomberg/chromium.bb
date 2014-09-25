// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/profiles/profile_signin_confirmation_view_controller.h"

#include <algorithm>
#include <cmath>

#include "base/callback_helpers.h"
#include "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#import "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_control_utils.h"
#import "chrome/browser/ui/cocoa/hover_close_button.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/sync/profile_signin_confirmation_helper.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/controls/hyperlink_button_cell.h"
#import "ui/base/cocoa/controls/hyperlink_text_view.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const CGFloat kWindowMinWidth = 500;
const CGFloat kButtonGap = 6;
const CGFloat kDialogAlertBarBorderWidth = 1;

// Determine the frame required to fit the content of a string.  Uses the
// provided height and width as preferred dimensions, where a value of
// 0.0 indicates no preference.
NSRect ComputeFrame(NSAttributedString* text, CGFloat width, CGFloat height) {
  NSRect frame =
      [text boundingRectWithSize:NSMakeSize(width, height)
                         options:NSStringDrawingUsesLineFragmentOrigin];
  // boundingRectWithSize is known to underestimate the width.
  static const CGFloat kTextViewPadding = 10;
  frame.size.width += kTextViewPadding;
  return frame;
}

// Make the indicated range of characters in a text view bold.
void MakeTextBold(NSTextField* textField, int offset, int length) {
  base::scoped_nsobject<NSMutableAttributedString> text(
      [[textField attributedStringValue] mutableCopy]);
  NSFont* currentFont =
      [text attribute:NSFontAttributeName
              atIndex:offset
               effectiveRange:NULL];
  NSFontManager* fontManager = [NSFontManager sharedFontManager];
  NSFont* boldFont = [fontManager convertFont:currentFont
                                  toHaveTrait:NSBoldFontMask];
  [text beginEditing];
  [text addAttribute:NSFontAttributeName
               value:boldFont
               range:NSMakeRange(offset, length)];
  [text endEditing];
  [textField setAttributedStringValue:text];
}

// Remove underlining from the specified range of characters in a text view.
void RemoveUnderlining(NSTextView* textView, int offset, int length) {
  // Clear the default link attributes that were set by the
  // HyperlinkTextView, otherwise removing the underline doesn't matter.
  [textView setLinkTextAttributes:nil];
  NSTextStorage* text = [textView textStorage];
  NSRange range = NSMakeRange(offset, length);
  [text addAttribute:NSUnderlineStyleAttributeName
               value:[NSNumber numberWithInt:NSUnderlineStyleNone]
               range:range];
}

// Create a new NSTextView and add it to the specified parent.
NSTextView* AddTextView(
    NSView* parent,
    id<NSTextViewDelegate> delegate,
    const base::string16& message,
    const base::string16& link,
    int offset,
    const ui::ResourceBundle::FontStyle& font_style) {
  base::scoped_nsobject<HyperlinkTextView> textView(
      [[HyperlinkTextView alloc] initWithFrame:NSZeroRect]);
  NSFont* font = ui::ResourceBundle::GetSharedInstance().GetFont(
      font_style).GetNativeFont();
  NSColor* linkColor = gfx::SkColorToCalibratedNSColor(
      chrome_style::GetLinkColor());
  [textView setMessageAndLink:base::SysUTF16ToNSString(message)
                     withLink:base::SysUTF16ToNSString(link)
                     atOffset:offset
                         font:font
                 messageColor:[NSColor blackColor]
                    linkColor:linkColor];
  RemoveUnderlining(textView, offset, link.size());
  [textView setDelegate:delegate];
  [parent addSubview:textView];
  return textView.autorelease();
}

// Create a new NSTextField and add it to the specified parent.
NSTextField* AddTextField(
    NSView* parent,
    const base::string16& message,
    const ui::ResourceBundle::FontStyle& font_style) {
  NSTextField* textField = constrained_window::CreateLabel();
  [textField setAttributedStringValue:
      constrained_window::GetAttributedLabelString(
          SysUTF16ToNSString(message),
          font_style,
          NSNaturalTextAlignment,
          NSLineBreakByWordWrapping)];
  [parent addSubview:textField];
  return textField;
}

}  // namespace

@interface ProfileSigninConfirmationViewController ()
- (void)learnMore;
- (void)addButton:(NSButton*)button
        withTitle:(int)resourceID
           target:(id)target
           action:(SEL)action
   shouldAutoSize:(BOOL)shouldAutoSize;
@end

@implementation ProfileSigninConfirmationViewController

- (id)initWithBrowser:(Browser*)browser
             username:(const std::string&)username
             delegate:(ui::ProfileSigninConfirmationDelegate*)delegate
  closeDialogCallback:(const base::Closure&)closeDialogCallback
 offerProfileCreation:(bool)offer {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    browser_ = browser;
    username_ = username;
    delegate_ = delegate;
    closeDialogCallback_ = closeDialogCallback;
    offerProfileCreation_ = offer;
  }
  return self;
}

- (void)loadView {
  self.view = [[[NSView alloc] initWithFrame:NSZeroRect] autorelease];
  cancelButton_.reset(
      [[ConstrainedWindowButton alloc] initWithFrame:NSZeroRect]);
  okButton_.reset(
      [[ConstrainedWindowButton alloc] initWithFrame:NSZeroRect]);
  if (offerProfileCreation_) {
    createProfileButton_.reset(
        [[ConstrainedWindowButton alloc] initWithFrame:NSZeroRect]);
  }
  promptBox_.reset(
      [[NSBox alloc] initWithFrame:NSZeroRect]);
  closeButton_.reset(
      [[WebUIHoverCloseButton alloc] initWithFrame:NSZeroRect]);

  // -------------------------------
  // | Title                     x |
  // |-----------------------------| (1 px border)
  // | Prompt    (box)             |
  // |-----------------------------| (1 px border)
  // | Explanation                 |
  // |                             |
  // | [create]      [cancel] [ok] |
  // -------------------------------

  // The width of the dialog should be sufficient to fit the buttons on
  // one line and the title and the close button on one line, but not
  // smaller than kWindowMinWidth.  Therefore we first layout the title
  // and the buttons and then compute the necessary width.

  // OK button.
  [self addButton:okButton_
        withTitle:IDS_ENTERPRISE_SIGNIN_CONTINUE_NEW_STYLE
           target:self
           action:@selector(ok:)
   shouldAutoSize:YES];

  // Cancel button.
  [self addButton:cancelButton_
        withTitle:IDS_ENTERPRISE_SIGNIN_CANCEL
           target:self
           action:@selector(cancel:)
   shouldAutoSize:YES];

  // Add the close button.
  [self addButton:closeButton_
        withTitle:0
           target:self
           action:@selector(close:)
   shouldAutoSize:NO];
  NSRect closeButtonFrame = [closeButton_ frame];
  closeButtonFrame.size.width = chrome_style::GetCloseButtonSize();
  closeButtonFrame.size.height = chrome_style::GetCloseButtonSize();
  [closeButton_ setFrame:closeButtonFrame];

  // Create Profile link.
  if (offerProfileCreation_) {
    [self addButton:createProfileButton_
          withTitle:IDS_ENTERPRISE_SIGNIN_CREATE_NEW_PROFILE_NEW_STYLE
             target:self
             action:@selector(createProfile:)
     shouldAutoSize:YES];
  }

  // Add the title label.
  titleField_.reset(
      [AddTextField([self view],
                    l10n_util::GetStringUTF16(
                        IDS_ENTERPRISE_SIGNIN_TITLE_NEW_STYLE),
                    chrome_style::kTitleFontStyle) retain]);
  [titleField_ setFrame:ComputeFrame(
      [titleField_ attributedStringValue], 0.0, 0.0)];

  // Compute the dialog width using the title and buttons.
  const CGFloat buttonsWidth =
      (offerProfileCreation_ ? NSWidth([createProfileButton_ frame]) : 0) +
      kButtonGap + NSWidth([cancelButton_ frame]) +
      kButtonGap + NSWidth([okButton_ frame]);
  const CGFloat titleWidth =
      NSWidth([titleField_ frame]) + NSWidth([closeButton_ frame]);
  // Dialog minimum width must include the padding.
  const CGFloat minWidth =
      kWindowMinWidth - 2 * chrome_style::kHorizontalPadding;
  const CGFloat width = std::max(minWidth,
                                 std::max(buttonsWidth, titleWidth));
  const CGFloat dialogWidth = width + 2 * chrome_style::kHorizontalPadding;

  // Now setup the prompt and explanation text using the computed width.

  // Prompt box.
  [promptBox_ setBorderColor:gfx::SkColorToCalibratedNSColor(
      ui::GetSigninConfirmationPromptBarColor(
          ui::kSigninConfirmationPromptBarBorderAlpha))];
  [promptBox_ setBorderWidth:kDialogAlertBarBorderWidth];
  [promptBox_ setFillColor:gfx::SkColorToCalibratedNSColor(
      ui::GetSigninConfirmationPromptBarColor(
          ui::kSigninConfirmationPromptBarBackgroundAlpha))];
  [promptBox_ setBoxType:NSBoxCustom];
  [promptBox_ setTitlePosition:NSNoTitle];
  [[self view] addSubview:promptBox_];

  // Prompt text.
  size_t offset;
  const base::string16 domain =
      base::ASCIIToUTF16(gaia::ExtractDomainName(username_));
  const base::string16 username = base::ASCIIToUTF16(username_);
  const base::string16 prompt_text =
      l10n_util::GetStringFUTF16(
          IDS_ENTERPRISE_SIGNIN_ALERT_NEW_STYLE,
          domain, &offset);
  promptField_.reset(
      [AddTextField(promptBox_, prompt_text, chrome_style::kTextFontStyle)
          retain]);
  MakeTextBold(promptField_, offset, domain.size());
  [promptField_ setFrame:ComputeFrame(
        [promptField_ attributedStringValue], width, 0.0)];

  // Set the height of the prompt box from the prompt text, padding, and border.
  CGFloat boxHeight =
      kDialogAlertBarBorderWidth +
      chrome_style::kRowPadding +
      NSHeight([promptField_ frame]) +
      chrome_style::kRowPadding +
      kDialogAlertBarBorderWidth;
  [promptBox_ setFrame:NSMakeRect(0, 0, dialogWidth, boxHeight)];

  // Explanation text.
  std::vector<size_t> offsets;
  const base::string16 learn_more_text =
      l10n_util::GetStringUTF16(
          IDS_ENTERPRISE_SIGNIN_PROFILE_LINK_LEARN_MORE);
  const base::string16 explanation_text =
      l10n_util::GetStringFUTF16(
          offerProfileCreation_ ?
          IDS_ENTERPRISE_SIGNIN_EXPLANATION_WITH_PROFILE_CREATION_NEW_STYLE :
          IDS_ENTERPRISE_SIGNIN_EXPLANATION_WITHOUT_PROFILE_CREATION_NEW_STYLE,
          username, learn_more_text, &offsets);
  // HyperlinkTextView requires manually inserting the link text
  // into the middle of the message text.  To do this we slice out
  // the "learn more" string from the message so that it can be
  // inserted again.
  const base::string16 explanation_message_text =
    explanation_text.substr(0, offsets[1]) +
    explanation_text.substr(offsets[1] + learn_more_text.size());
  explanationField_.reset(
      [AddTextView([self view], self, explanation_message_text, learn_more_text,
                   offsets[1], chrome_style::kTextFontStyle) retain]);

  [explanationField_ setFrame:ComputeFrame(
        [explanationField_ attributedString], width, 0.0)];

  // Layout the elements, starting at the bottom and moving up.

  CGFloat curX = dialogWidth - chrome_style::kHorizontalPadding;
  CGFloat curY = chrome_style::kClientBottomPadding;

  // Buttons should go |Cancel|Continue|CreateProfile|, unless
  // |CreateProfile| isn't shown.
  if (offerProfileCreation_) {
    curX -= NSWidth([createProfileButton_ frame]);
    [createProfileButton_ setFrameOrigin:NSMakePoint(curX, curY)];
    curX -= kButtonGap;
  }
  curX -= NSWidth([okButton_ frame]);
  [okButton_ setFrameOrigin:NSMakePoint(curX, curY)];
  curX -= (kButtonGap + NSWidth([cancelButton_ frame]));
  [cancelButton_ setFrameOrigin:NSMakePoint(curX, curY)];

  curY += NSHeight([cancelButton_ frame]);

  // Explanation text.
  curY += chrome_style::kRowPadding;
  [explanationField_
      setFrameOrigin:NSMakePoint(chrome_style::kHorizontalPadding, curY)];
  curY += NSHeight([explanationField_ frame]);

  // Prompt box goes all the way to the edges.
  curX = 0;
  curY += chrome_style::kRowPadding;
  [promptBox_ setFrameOrigin:NSMakePoint(curX, curY)];
  curY += NSHeight([promptBox_ frame]);

  // Prompt label fits in the middle of the box.
  NSRect boxClientFrame = [[promptBox_ contentView] bounds];
  CGFloat boxHorizontalMargin =
      roundf((dialogWidth - NSWidth(boxClientFrame)) / 2);
  CGFloat boxVerticalMargin =
      roundf((boxHeight - NSHeight(boxClientFrame)) / 2);
  [promptField_ setFrameOrigin:NSMakePoint(
      chrome_style::kHorizontalPadding - boxHorizontalMargin,
      chrome_style::kRowPadding - boxVerticalMargin)];

  // Title goes at the top.
  curY += chrome_style::kRowPadding;
  [titleField_
      setFrameOrigin:NSMakePoint(chrome_style::kHorizontalPadding, curY)];
  curY += NSHeight([titleField_ frame]);

  // Find the height required to fit everything with the necessary padding.
  CGFloat dialogHeight = curY + chrome_style::kTitleTopPadding;

  // Update the dialog frame with the computed dimensions.
  [[self view] setFrame:NSMakeRect(0, 0, dialogWidth, dialogHeight)];

  // Close button goes in the top-right corner.
  NSPoint closeOrigin = NSMakePoint(
      dialogWidth - chrome_style::kCloseButtonPadding -
          NSWidth(closeButtonFrame),
      dialogHeight - chrome_style::kCloseButtonPadding -
          NSWidth(closeButtonFrame));
  [closeButton_ setFrameOrigin:closeOrigin];
}

- (IBAction)cancel:(id)sender {
  if (delegate_) {
    delegate_->OnCancelSignin();
    delegate_ = NULL;
    closeDialogCallback_.Run();
  }
}

- (IBAction)ok:(id)sender {
  if (delegate_) {
    delegate_->OnContinueSignin();
    delegate_ = NULL;
    closeDialogCallback_.Run();
  }
}

- (IBAction)close:(id)sender {
  if (delegate_) {
    delegate_->OnCancelSignin();
    delegate_ = NULL;
  }
  closeDialogCallback_.Run();
}

- (IBAction)createProfile:(id)sender {
  if (delegate_) {
    delegate_->OnSigninWithNewProfile();
    delegate_ = NULL;
    closeDialogCallback_.Run();
  }
}

- (void)learnMore {
  chrome::NavigateParams params(
      browser_, GURL(chrome::kChromeEnterpriseSignInLearnMoreURL),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = NEW_POPUP;
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  chrome::Navigate(&params);
}

- (BOOL)textView:(NSTextView*)textView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
  if (textView == explanationField_.get()) {
    [self learnMore];
    return YES;
  }
  return NO;
}

- (void)addButton:(NSButton*)button
        withTitle:(int)resourceID
           target:(id)target
           action:(SEL)action
   shouldAutoSize:(BOOL)shouldAutoSize {
  if (resourceID)
    [button setTitle:base::SysUTF16ToNSString(
        l10n_util::GetStringUTF16(resourceID))];
  [button setTarget:target];
  [button setAction:action];
  [[self view] addSubview:button];
  if (shouldAutoSize)
    [GTMUILocalizerAndLayoutTweaker sizeToFitView:button];
}

@end

@implementation ProfileSigninConfirmationViewController (TestingAPI)

- (ui::ProfileSigninConfirmationDelegate*)delegate {
  return delegate_;
}

- (NSButton*)createProfileButton {
  return createProfileButton_.get();
}

- (NSTextView*)explanationField {
  return explanationField_.get();
}

@end
