// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/account_chooser_view_controller.h"

#include <cmath>

#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/passwords/account_avatar_fetcher_manager.h"
#import "chrome/browser/ui/cocoa/passwords/passwords_bubble_utils.h"
#include "chrome/browser/ui/passwords/password_dialog_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "ui/base/cocoa/controls/hyperlink_text_view.h"
#include "ui/base/l10n/l10n_util.h"

@implementation CredentialItemCell {
  base::scoped_nsobject<CredentialItemView> view_;
}
- (id)initWithView:(CredentialItemView*)view {
  if ((self = [super init]))
    view_.reset([view retain]);
  return self;
}
- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  [controlView addSubview:view_];
  [view_ setFrame:cellFrame];
}
- (id)copyWithZone:(NSZone*)zone {
  return [[CredentialItemCell alloc] initWithView:view_];
}
- (CredentialItemView*)view {
  return view_.get();
}
@end

@interface AccountChooserViewController()
- (void)onCancelClicked:(id)sender;
+ (NSArray*)credentialItemsFromBridge:(PasswordPromptBridgeInterface*)bridge
                            delegate:(id<CredentialItemDelegate>)delegate;
@end

@implementation AccountChooserViewController
@synthesize bridge = bridge_;

- (instancetype)initWithBridge:(PasswordPromptBridgeInterface*)bridge {
  base::scoped_nsobject<AccountAvatarFetcherManager> avatarManager(
      [[AccountAvatarFetcherManager alloc]
           initWithRequestContext:bridge->GetRequestContext()]);
  return [self initWithBridge:bridge avatarManager:avatarManager];
}

- (void)dealloc {
  [credentialsView_ setDelegate:nil];
  [credentialsView_ setDataSource:nil];
  [super dealloc];
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
  titleView_ = TitleLabelWithLink(title_text.first, title_text.second, self);
  // Force the text to wrap to fit in the bubble size.
  [titleView_ setVerticallyResizable:YES];
  const CGFloat width = kDesiredBubbleWidth - 2*kFramePadding;
  [titleView_ setFrameSize:NSMakeSize(width, MAXFLOAT)];
  [titleView_ sizeToFit];
  [view addSubview:titleView_];

  // Credentials list.
  credentialItems_.reset(
      [[[self class] credentialItemsFromBridge:bridge_ delegate:self] retain]);
  base::scoped_nsobject<NSTableView> credentialsView(
      [[NSTableView alloc] initWithFrame:NSZeroRect]);
  [credentialsView
      setSelectionHighlightStyle:NSTableViewSelectionHighlightStyleNone];
  NSTableColumn* column =
      [[[NSTableColumn alloc] initWithIdentifier:@""] autorelease];
  [credentialsView addTableColumn:column];
  [credentialsView setDelegate:self];
  [credentialsView setDataSource:self];
  [credentialsView setFocusRingType:NSFocusRingTypeNone];
  credentialsView_ = credentialsView;
  [view addSubview:credentialsView_];

  // "Cancel" button.
  cancelButton_ = DialogButton(l10n_util::GetNSString(
                          IDS_CREDENTIAL_MANAGEMENT_ACCOUNT_CHOOSER_NO_THANKS));
  [cancelButton_ setTarget:self];
  [cancelButton_ setAction:@selector(onCancelClicked:)];
  [view addSubview:cancelButton_];

  // Lay out the views.
  [cancelButton_ setFrameOrigin:NSMakePoint(
      kFramePadding + width - NSWidth([cancelButton_ frame]),
      kFramePadding)];

  // The credentials TableView expands to fill available space.
  [column setMaxWidth:width];
  [credentialsView
      setFrameSize:NSMakeSize(width, NSHeight([credentialsView_ frame]))];
  [credentialsView_
      setFrameOrigin:NSMakePoint(kFramePadding,
                                 NSMaxY([cancelButton_ frame]) +
                                     kUnrelatedControlVerticalPadding)];

  [titleView_ setFrameOrigin:NSMakePoint(
      kFramePadding,
      NSMaxY([credentialsView_ frame]) + kUnrelatedControlVerticalPadding)];

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

- (void)fetchAvatar:(const GURL&)avatarURL forView:(CredentialItemView*)view {
  [avatarManager_ fetchAvatar:avatarURL forView:view];
}

+ (NSArray*)credentialItemsFromBridge:(PasswordPromptBridgeInterface*)bridge
                             delegate:(id<CredentialItemDelegate>)delegate {
  base::scoped_nsobject<NSMutableArray> items([[NSMutableArray alloc] init]);
  PasswordDialogController* controller = bridge->GetDialogController();
  for (const auto& form : controller->GetLocalForms()) {
    base::scoped_nsobject<CredentialItemView> item([[CredentialItemView alloc]
        initWithPasswordForm:*form
              credentialType:password_manager::CredentialType::
                                 CREDENTIAL_TYPE_PASSWORD
                       style:password_manager_mac::CredentialItemStyle::
                                 ACCOUNT_CHOOSER
                    delegate:delegate]);
    [item setAutoresizingMask:NSViewWidthSizable];
    [items addObject:item];
  }
  for (const auto& form : controller->GetFederationsForms()) {
    base::scoped_nsobject<CredentialItemView> item([[CredentialItemView alloc]
        initWithPasswordForm:*form
              credentialType:password_manager::CredentialType::
                                 CREDENTIAL_TYPE_FEDERATED
                       style:password_manager_mac::CredentialItemStyle::
                                 ACCOUNT_CHOOSER
                    delegate:delegate]);
    [item setAutoresizingMask:NSViewWidthSizable];
    [items addObject:item];
  }
  return items.autorelease();
}

#pragma mark NSTableViewDataSource

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView {
  return [credentialItems_ count];
}

#pragma mark NSTableViewDelegate

- (CGFloat)tableView:(NSTableView*)tableView heightOfRow:(NSInteger)row {
  return NSHeight([[credentialItems_.get() objectAtIndex:row] frame]);
}

- (NSCell*)tableView:(NSTableView*)tableView
    dataCellForTableColumn:(NSTableColumn*)tableColumn
                       row:(NSInteger)row {
  return [[[CredentialItemCell alloc]
      initWithView:[credentialItems_.get() objectAtIndex:row]] autorelease];
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification {
  CredentialItemView* item =
      [credentialItems_.get() objectAtIndex:[credentialsView_ selectedRow]];
  if (bridge_ && bridge_->GetDialogController()) {
    bridge_->GetDialogController()->OnChooseCredentials(item.passwordForm,
                                                        item.credentialType);
  }
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

- (NSTableView*)credentialsView {
  return credentialsView_;
}

- (NSTextView*)titleView {
  return titleView_;
}

@end
