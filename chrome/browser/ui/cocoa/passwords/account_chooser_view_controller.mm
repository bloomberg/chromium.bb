// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/account_chooser_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/key_equivalent_constants.h"
#import "chrome/browser/ui/cocoa/passwords/account_avatar_fetcher_manager.h"
#import "chrome/browser/ui/cocoa/passwords/credential_item_button.h"
#import "chrome/browser/ui/cocoa/passwords/passwords_bubble_utils.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/passwords/password_dialog_controller.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/cocoa/controls/hyperlink_text_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

// Standard height of one credential item. It can be bigger though in case the
// text needs more vertical space than the avatar.
constexpr CGFloat kCredentialHeight =
    kAvatarImageSize + 2 * kVerticalAvatarMargin;

// Maximum height of the credential list. The unit is one row height.
constexpr CGFloat kMaxHeightAccounts = 3.5;

}  // namespace

@interface AccountChooserViewController () {
  NSButton* cancelButton_;  // Weak.
  NSButton* signInButton_;  // Weak.
  NSTextView* titleView_;   // Weak.
  base::scoped_nsobject<NSArray> credentialButtons_;
  base::scoped_nsobject<AccountAvatarFetcherManager> avatarManager_;
}
- (void)onCancelClicked:(id)sender;
- (void)onCredentialClicked:(id)sender;
- (void)onSignInClicked:(id)sender;
- (void)loadCredentialItems;
@end

@implementation AccountChooserViewController
@synthesize bridge = bridge_;

- (instancetype)initWithBridge:(PasswordPromptBridgeInterface*)bridge {
  base::scoped_nsobject<AccountAvatarFetcherManager> avatarManager(
      [[AccountAvatarFetcherManager alloc]
           initWithRequestContext:bridge->GetRequestContext()]);
  return [self initWithBridge:bridge avatarManager:avatarManager];
}

- (NSButton*)defaultButton {
  return signInButton_;
}

- (void)loadView {
  base::scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:NSZeroRect]);

  // ------------------------------------
  // |                                  |
  // | Choose an account etc etc        |
  // |                                  |
  // | ----                             |
  // | |  |  credential view       (i)  |
  // | ----                             |
  // | |  |  credential view       (i)  |
  // | ----                             |
  // |                                  |
  // |             [ Cancel ] [Sign In] |
  // ------------------------------------

  // Create the views.
  // Title.
  PasswordDialogController* controller = bridge_->GetDialogController();
  std::pair<base::string16, gfx::Range> title_text =
      controller->GetAccoutChooserTitle();
  titleView_ =
      TitleDialogLabelWithLink(title_text.first, title_text.second, self);
  // Force the text to wrap to fit in the bubble size.
  [titleView_ setVerticallyResizable:YES];
  const CGFloat width = kDesiredBubbleWidth - 2*kFramePadding;
  [titleView_ setFrameSize:NSMakeSize(width, MAXFLOAT)];
  [titleView_ sizeToFit];
  [[titleView_ textContainer] setLineFragmentPadding:0];
  [view addSubview:titleView_];

  // Credentials list.
  [self loadCredentialItems];

  // "Cancel" button.
  cancelButton_ = BiggerDialogButton(l10n_util::GetNSString(IDS_APP_CANCEL));
  [cancelButton_ setTarget:self];
  [cancelButton_ setAction:@selector(onCancelClicked:)];
  [cancelButton_ setKeyEquivalent:kKeyEquivalentEscape];
  [view addSubview:cancelButton_];

  // "Sign In" button.
  if (controller->ShouldShowSignInButton()) {
    signInButton_ = BiggerDialogButton(
        l10n_util::GetNSString(IDS_PASSWORD_MANAGER_ACCOUNT_CHOOSER_SIGN_IN));
    [signInButton_ setTarget:self];
    [signInButton_ setAction:@selector(onSignInClicked:)];
    [signInButton_ setKeyEquivalent:kKeyEquivalentReturn];
    [view addSubview:signInButton_];
  }

  // Lay out the views.
  CGFloat curX = kFramePadding + width;
  if (signInButton_) {
    curX -= NSWidth([signInButton_ frame]);
    [signInButton_ setFrameOrigin:NSMakePoint(curX, kFramePadding)];
  }
  [cancelButton_ setFrameOrigin:NSMakePoint(
      curX - NSWidth([cancelButton_ frame]),
      kFramePadding)];

  base::scoped_nsobject<NSScrollView> scrollView([[NSScrollView alloc]
      initWithFrame:NSMakeRect(0, 0, kDesiredBubbleWidth, kCredentialHeight)]);
  [scrollView
      setHasVerticalScroller:[credentialButtons_ count] > kMaxHeightAccounts
                                 ? YES
                                 : NO];
  [scrollView setBorderType:NSNoBorder];
  const CGFloat buttonWidth = [scrollView contentSize].width;
  CGFloat curY = 0;
  base::scoped_nsobject<NSView> documentView([[NSView alloc]
      initWithFrame:NSZeroRect]);
  for (CredentialItemButton* button in credentialButtons_.get()) {
    [documentView addSubview:button];
    CGFloat cellHeight = [[button cell] cellSize].height;
    [button setFrame:NSMakeRect(0, curY, buttonWidth,
                                cellHeight + 2 * kVerticalAvatarMargin)];
    if (button.passwordForm->is_public_suffix_match) {
      NSRect rect = NSMakeRect(
          base::i18n::IsRTL() ? kFramePadding
                              : buttonWidth - kInfoIconSize - kFramePadding,
          (NSHeight([button frame]) - kInfoIconSize) / 2, kInfoIconSize,
          kInfoIconSize);
      NSView* icon = [button
          addInfoIcon:base::SysUTF8ToNSString(
                          button.passwordForm->origin.GetOrigin().spec())];
      [icon setFrame:rect];
    }
    curY = NSMaxY([button frame]);
  }
  [documentView setFrameSize:NSMakeSize(buttonWidth, curY)];
  [scrollView setDocumentView:documentView];
  [scrollView setFrameSize:NSMakeSize(
      kDesiredBubbleWidth,
      [scrollView hasVerticalScroller] ? kMaxHeightAccounts * kCredentialHeight
                                       : curY)];
  [view addSubview:scrollView];
  [documentView scrollRectToVisible:NSMakeRect(0, curY, buttonWidth, 0)];
  curY = NSMaxY([cancelButton_ frame]) + 3 * kRelatedControlVerticalSpacing;
  [scrollView setFrameOrigin:NSMakePoint(0, curY)];
  curY = NSMaxY([scrollView frame]) + 2 * kRelatedControlVerticalSpacing;

  [titleView_ setFrameOrigin:NSMakePoint(kFramePadding, curY)];

  const CGFloat frameHeight = NSMaxY([titleView_ frame]) + kFramePadding;
  [view setFrame:NSMakeRect(0, 0, kDesiredBubbleWidth, frameHeight)];
  [self setView:view];
}

- (BOOL)textView:(NSTextView*)textView
    clickedOnLink:(id)link
          atIndex:(NSUInteger)charIndex {
  if (bridge_ && bridge_->GetDialogController())
    bridge_->GetDialogController()->OnSmartLockLinkClicked();
  return YES;
}

- (void)onCancelClicked:(id)sender {
  if (bridge_)
    bridge_->PerformClose();
}

- (void)onCredentialClicked:(id)sender {
  CredentialItemButton* button =
      base::mac::ObjCCastStrict<CredentialItemButton>(sender);
  if (bridge_ && bridge_->GetDialogController()) {
    bridge_->GetDialogController()->OnChooseCredentials(*button.passwordForm,
                                                        button.credentialType);
  }
}

- (void)onSignInClicked:(id)sender {
  if (bridge_ && bridge_->GetDialogController())
    bridge_->GetDialogController()->OnSignInClicked();
}

- (void)loadCredentialItems {
  base::scoped_nsobject<NSMutableArray> items([[NSMutableArray alloc] init]);
  PasswordDialogController* controller = self.bridge->GetDialogController();
  for (const auto& form : controller->GetLocalForms()) {
    base::scoped_nsobject<CredentialItemButton> item(
        [[CredentialItemButton alloc]
              initWithFrame:NSZeroRect
            backgroundColor:[NSColor textBackgroundColor]
                 hoverColor:skia::SkColorToSRGBNSColor(kButtonHoverColor)]);
    [item setPasswordForm:form.get()];
    [item setCredentialType:password_manager::CredentialType::
                                CREDENTIAL_TYPE_PASSWORD];
    std::pair<base::string16, base::string16> labels =
        GetCredentialLabelsForAccountChooser(*form);
    if (labels.second.empty()) {
      [item setTitle:base::SysUTF16ToNSString(labels.first)];
    } else {
      NSString* text = base::SysUTF16ToNSString(
          labels.first + base::ASCIIToUTF16("\n") + labels.second);
      NSFont* font = ResourceBundle::GetSharedInstance()
                         .GetFontList(ResourceBundle::SmallFont)
                         .GetPrimaryFont()
                         .GetNativeFont();
      NSDictionary* attrsDictionary =
          [NSDictionary dictionaryWithObject:font forKey:NSFontAttributeName];
      base::scoped_nsobject<NSMutableAttributedString> attributed_string(
          [[NSMutableAttributedString alloc] initWithString:text
                                                 attributes:attrsDictionary]);
      [attributed_string beginEditing];
      [attributed_string
          addAttribute:NSForegroundColorAttributeName
                 value:skia::SkColorToSRGBNSColor(kAutoSigninTextColor)
                 range:NSMakeRange(labels.first.size() + 1,
                                   labels.second.size())];
      [attributed_string endEditing];
      [item setAttributedTitle:attributed_string];
    }
    [item setImage:[CredentialItemButton defaultAvatar]];
    if (form->icon_url.is_valid())
      [avatarManager_ fetchAvatar:form->icon_url forView:item];
    [item setTarget:self];
    [item setAction:@selector(onCredentialClicked:)];
    [items addObject:item];
  }
  credentialButtons_.reset(items.release());
}

@end

@implementation AccountChooserViewController(Testing)

- (instancetype)initWithBridge:(PasswordPromptBridgeInterface*)bridge
                 avatarManager:(AccountAvatarFetcherManager*)avatarManager {
  DCHECK(bridge);
  if (self = [super initWithNibName:nil bundle:nil]) {
    bridge_ = bridge;
    avatarManager_.reset([avatarManager retain]);
  }
  return self;
}

- (NSButton*)cancelButton {
  return cancelButton_;
}

- (NSButton*)signInButton {
  return signInButton_;
}

- (NSArray*)credentialButtons {
  return credentialButtons_;
}

- (NSTextView*)titleView {
  return titleView_;
}

@end
