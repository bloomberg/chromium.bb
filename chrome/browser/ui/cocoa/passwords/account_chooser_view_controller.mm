// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/account_chooser_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/passwords/account_avatar_fetcher_manager.h"
#import "chrome/browser/ui/cocoa/passwords/credential_item_button.h"
#import "chrome/browser/ui/cocoa/passwords/passwords_bubble_utils.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/passwords/password_dialog_controller.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/cocoa/controls/hyperlink_text_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

// Returns height of one credential item.
constexpr CGFloat kCredentialHeight =
    kAvatarImageSize + 2 * kVerticalAvatarMargin;

// Maximum number of accounts displayed before vertical scrolling appears.
constexpr size_t kMaxAccounts = 3;

}  // namespace

@interface AccountChooserViewController () {
  NSButton* cancelButton_;  // Weak.
  NSTextView* titleView_;   //  Weak.
  base::scoped_nsobject<NSArray> credentialButtons_;
  base::scoped_nsobject<AccountAvatarFetcherManager> avatarManager_;
}
- (void)onCancelClicked:(id)sender;
- (void)onCredentialClicked:(id)sender;
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

- (void)loadView {
  base::scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:NSZeroRect]);

  // ------------------------------------
  // |                                  |
  // | Choose an account etc etc        |
  // |                                  |
  // | ----                             |
  // | |  |  credential view            |
  // | ----                             |
  // | |  |  credential view            |
  // | ----                             |
  // |                                  |
  // |                      [ Cancel ]  |
  // ------------------------------------

  // Create the views.
  // Title.
  std::pair<base::string16, gfx::Range> title_text =
      bridge_->GetDialogController()->GetAccoutChooserTitle();
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
  cancelButton_ = DialogButton(l10n_util::GetNSString(IDS_APP_CANCEL));
  [cancelButton_ setTarget:self];
  [cancelButton_ setAction:@selector(onCancelClicked:)];
  [view addSubview:cancelButton_];

  // Lay out the views.
  [cancelButton_ setFrameOrigin:NSMakePoint(
      kFramePadding + width - NSWidth([cancelButton_ frame]),
      kFramePadding)];

  NSSize buttonsSize = NSMakeSize(
      kDesiredBubbleWidth,
      std::min([credentialButtons_ count], kMaxAccounts) * kCredentialHeight);
  base::scoped_nsobject<NSScrollView> scrollView = [[NSScrollView alloc]
      initWithFrame:NSMakeRect(0, 0, buttonsSize.width, buttonsSize.height)];
  [scrollView setHasVerticalScroller:[credentialButtons_ count] > kMaxAccounts
                                         ? YES
                                         : NO];
  [scrollView setBorderType:NSNoBorder];
  CGFloat buttonWidth = [scrollView contentSize].width;
  CGFloat curY = 0;
  base::scoped_nsobject<NSView> documentView([[NSView alloc]
      initWithFrame:NSMakeRect(0, 0, buttonWidth, [credentialButtons_ count] *
                                                      kCredentialHeight)]);
  for (CredentialItemButton* button in credentialButtons_.get()) {
    [documentView addSubview:button];
    [button setFrame:NSMakeRect(0, curY, buttonWidth, kCredentialHeight)];
    curY = NSMaxY([button frame]);
  }
  [scrollView setDocumentView:documentView];
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

- (void)loadCredentialItems {
  base::scoped_nsobject<NSMutableArray> items([[NSMutableArray alloc] init]);
  PasswordDialogController* controller = self.bridge->GetDialogController();
  NSRect rect = NSMakeRect(0, 0, kDesiredBubbleWidth, kCredentialHeight);
  for (const auto& form : controller->GetLocalForms()) {
    base::scoped_nsobject<CredentialItemButton> item(
        [[CredentialItemButton alloc]
              initWithFrame:rect
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

- (NSArray*)credentialButtons {
  return credentialButtons_;
}

- (NSTextView*)titleView {
  return titleView_;
}

@end
