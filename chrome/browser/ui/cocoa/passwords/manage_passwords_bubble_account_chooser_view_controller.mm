// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_account_chooser_view_controller.h"

#include <cmath>

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/ui/cocoa/bubble_combobox.h"
#import "chrome/browser/ui/cocoa/passwords/account_avatar_fetcher_manager.h"
#include "chrome/browser/ui/passwords/account_chooser_more_combobox_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/content/common/credential_manager_types.h"
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

@interface ManagePasswordsBubbleAccountChooserViewController()
- (id)initWithModel:(ManagePasswordsBubbleModel*)model
      avatarManager:(AccountAvatarFetcherManager*)avatarManager
           delegate:(id<ManagePasswordsBubbleContentViewDelegate>)delegate;
- (void)onCancelClicked:(id)sender;
- (void)onLearnMoreClicked:(id)sender;
- (void)onSettingsClicked:(id)sender;
+ (NSArray*)credentialItemsForModel:(ManagePasswordsBubbleModel*)model
                           delegate:(id<CredentialItemDelegate>)delegate;
@property(nonatomic, readonly) NSButton* cancelButton;
@property(nonatomic, readonly) BubbleCombobox* moreButton;
@property(nonatomic, readonly) NSTableView* credentialsView;
@end

@implementation ManagePasswordsBubbleAccountChooserViewController

@synthesize cancelButton = cancelButton_;
@synthesize moreButton = moreButton_;
@synthesize credentialsView = credentialsView_;

- (id)initWithModel:(ManagePasswordsBubbleModel*)model
      avatarManager:(AccountAvatarFetcherManager*)avatarManager
           delegate:(id<ManagePasswordsBubbleContentViewDelegate>)delegate {
  DCHECK(model);
  if ((self = [super initWithNibName:nil bundle:nil])) {
    model_ = model;
    delegate_ = delegate;
    avatarManager_.reset([avatarManager retain]);
  }
  return self;
}

- (id)initWithModel:(ManagePasswordsBubbleModel*)model
           delegate:(id<ManagePasswordsBubbleContentViewDelegate>)delegate {
  base::scoped_nsobject<AccountAvatarFetcherManager> avatarManager(
      [[AccountAvatarFetcherManager alloc]
          initWithRequestContext:model->GetProfile()->GetRequestContext()]);
  return
      [self initWithModel:model avatarManager:avatarManager delegate:delegate];
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
  // |            [ More v] [ Cancel ]  |
  // ------------------------------------

  // Create the views.
  // Title.
  NSTextField* title =
      [self addTitleLabel:base::SysUTF16ToNSString(model_->title())
                   toView:view];
  [title setAlignment:base::i18n::IsRTL() ? NSRightTextAlignment
                                          : NSLeftTextAlignment];

  // Credentials list.
  credentialItems_.reset(
      [[[self class] credentialItemsForModel:model_ delegate:self] retain]);
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
  cancelButton_ =
      [self addButton:l10n_util::GetNSString(
                          IDS_CREDENTIAL_MANAGEMENT_ACCOUNT_CHOOSER_NO_THANKS)
               toView:view
               target:self
               action:@selector(onCancelClicked:)];

  // "More" button.
  AccountChooserMoreComboboxModel comboboxModel;
  moreButton_ = [[BubbleCombobox alloc] initWithFrame:NSZeroRect
                                            pullsDown:YES
                                                model:&comboboxModel];
  [moreButton_ sizeToFit];
  NSMenuItem* learnMoreItem = [moreButton_
      itemAtIndex:AccountChooserMoreComboboxModel::INDEX_LEARN_MORE];
  [learnMoreItem setTarget:self];
  [learnMoreItem setAction:@selector(onLearnMoreClicked:)];
  NSMenuItem* settingsItem =
      [moreButton_ itemAtIndex:AccountChooserMoreComboboxModel::INDEX_SETTINGS];
  [settingsItem setTarget:self];
  [settingsItem setAction:@selector(onSettingsClicked:)];
  [view addSubview:moreButton_];

  // Lay out the views.
  const CGFloat width =
      std::max(NSWidth([title frame]), NSWidth([credentialsView_ frame]));
  [cancelButton_
      setFrameOrigin:NSMakePoint(
                         base::i18n::IsRTL()
                             ? password_manager::mac::ui::kFramePadding
                             : password_manager::mac::ui::kFramePadding +
                                   width - NSWidth([cancelButton_ frame]),
                         password_manager::mac::ui::kFramePadding)];
  [moreButton_
      setFrameOrigin:NSMakePoint(
                         base::i18n::IsRTL()
                             ? NSMaxX([cancelButton_ frame]) +
                                   password_manager::mac::ui::
                                       kRelatedControlHorizontalPadding
                             : NSMinX([cancelButton_ frame]) -
                                   password_manager::mac::ui::
                                       kRelatedControlHorizontalPadding -
                                   NSWidth([moreButton_ frame]),
                         std::ceil(password_manager::mac::ui::kFramePadding +
                                   (NSHeight([cancelButton_ frame]) -
                                    NSHeight([moreButton_ frame])) /
                                       2.0f))];

  // The credentials TableView expands to fill available space.
  [column setMaxWidth:width];
  [credentialsView_
      setFrameOrigin:NSMakePoint(password_manager::mac::ui::kFramePadding,
                                 NSMaxY([cancelButton_ frame]) +
                                     password_manager::mac::ui::
                                         kUnrelatedControlVerticalPadding)];

  [title setFrameOrigin:NSMakePoint(
                            base::i18n::IsRTL()
                                ? password_manager::mac::ui::kFramePadding +
                                      width - NSWidth([title frame])
                                : password_manager::mac::ui::kFramePadding,
                            NSMaxY([credentialsView_ frame]) +
                                password_manager::mac::ui::
                                    kUnrelatedControlVerticalPadding)];

  // Compute the frame to hold all the views.
  const CGFloat frameWidth =
      width + 2 * password_manager::mac::ui::kFramePadding;
  const CGFloat frameHeight =
      NSMaxY([title frame]) + password_manager::mac::ui::kFramePadding;
  [view setFrame:NSMakeRect(0, 0, frameWidth, frameHeight)];

  [self setView:view];
}

- (void)onCancelClicked:(id)sender {
  model_->OnNopeClicked();
  [delegate_ viewShouldDismiss];
}

- (void)onLearnMoreClicked:(id)sender {
  // TODO(dconnelly): Open some help center article that's not written yet.
  [delegate_ viewShouldDismiss];
}

- (void)onSettingsClicked:(id)sender {
  model_->OnManageLinkClicked();
  [delegate_ viewShouldDismiss];
}

- (void)fetchAvatar:(const GURL&)avatarURL forView:(CredentialItemView*)view {
  [avatarManager_ fetchAvatar:avatarURL forView:view];
}

+ (NSArray*)credentialItemsForModel:(ManagePasswordsBubbleModel*)model
                           delegate:(id<CredentialItemDelegate>)delegate {
  base::scoped_nsobject<NSMutableArray> items([[NSMutableArray alloc] init]);
  for (auto form : model->local_credentials()) {
    base::scoped_nsobject<CredentialItemView> item([[CredentialItemView alloc]
        initWithPasswordForm:*form
              credentialType:password_manager::CredentialType::
                                 CREDENTIAL_TYPE_LOCAL
                    delegate:delegate]);
    [items addObject:item];
  }
  for (auto form : model->federated_credentials()) {
    base::scoped_nsobject<CredentialItemView> item([[CredentialItemView alloc]
        initWithPasswordForm:*form
              credentialType:password_manager::CredentialType::
                                 CREDENTIAL_TYPE_FEDERATED
                    delegate:delegate]);
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
  model_->OnChooseCredentials(item.passwordForm, item.credentialType);
  [delegate_ viewShouldDismiss];
}

@end
