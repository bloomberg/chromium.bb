// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_manage_credentials_view_controller.h"

#include <cmath>

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/passwords/account_avatar_fetcher_manager.h"
#import "chrome/browser/ui/cocoa/passwords/manage_credential_item_view_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/controls/hyperlink_button_cell.h"
#include "ui/base/l10n/l10n_util.h"

@implementation ManageCredentialItemCell {
  base::scoped_nsobject<ManageCredentialItemViewController> item_;
}
- (id)initWithItem:(ManageCredentialItemViewController*)item {
  if ((self = [super init]))
    item_.reset([item retain]);
  return self;
}
- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  [controlView addSubview:[item_ view]];
  [[item_ view] setFrame:cellFrame];
}
- (id)copyWithZone:(NSZone*)zone {
  return [[ManageCredentialItemCell alloc] initWithItem:item_];
}
- (ManageCredentialItemViewController*)item {
  return item_.get();
}
@end

@interface ManagePasswordsBubbleManageCredentialsViewController()
- (id)initWithModel:(ManagePasswordsBubbleModel*)model
      avatarManager:(AccountAvatarFetcherManager*)avatarManager
           delegate:(id<ManagePasswordsBubbleContentViewDelegate>)delegate;
- (void)onDoneClicked:(id)sender;
- (void)onManageClicked:(id)sender;
+ (NSArray*)credentialItemsForModel:(ManagePasswordsBubbleModel*)model
                           delegate:(id<CredentialItemDelegate>)delegate;
@property(nonatomic, readonly) NSButton* doneButton;
@property(nonatomic, readonly) NSButton* manageButton;
@property(nonatomic, readonly) NSTableView* credentialsView;
@end

@implementation ManagePasswordsBubbleManageCredentialsViewController

- (id)initWithModel:(ManagePasswordsBubbleModel*)model
      avatarManager:(AccountAvatarFetcherManager*)avatarManager
           delegate:(id<ManagePasswordsBubbleContentViewDelegate>)delegate {
  DCHECK(model);
  if (([super initWithDelegate:delegate])) {
    model_ = model;
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

- (NSButton*)doneButton {
  return doneButton_.get();
}

- (NSButton*)manageButton {
  return manageButton_.get();
}

- (NSTableView*)credentialsView {
  return credentialsView_.get();
}

- (void)loadView {
  base::scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:NSZeroRect]);

  // ------------------------------------
  // |                                  |
  // | Manage credentials               |
  // |                                  |
  // | ----                             |
  // | |  |  manage credential view   x |
  // | ----                             |
  // | |  |  manage credential view   x |
  // | ----                             |
  // |                                  |
  // | [ Manage ]           [ Done ]    |
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

  // "Manage" link.
  manageButton_.reset([[NSButton alloc] initWithFrame:NSZeroRect]);
  base::scoped_nsobject<HyperlinkButtonCell> cell([[HyperlinkButtonCell alloc]
      initTextCell:base::SysUTF16ToNSString(model_->manage_link())]);
  [cell setControlSize:NSSmallControlSize];
  [cell setShouldUnderline:NO];
  [cell setUnderlineOnHover:NO];
  [cell setTextColor:gfx::SkColorToCalibratedNSColor(
                         chrome_style::GetLinkColor())];
  [manageButton_ setCell:cell.get()];
  [manageButton_ sizeToFit];
  [manageButton_ setTarget:self];
  [manageButton_ setAction:@selector(onManageClicked:)];
  [view addSubview:manageButton_];

  // "Done" button.
  doneButton_.reset([[self addButton:l10n_util::GetNSString(IDS_DONE)
                              toView:view
                              target:self
                              action:@selector(onDoneClicked:)] retain]);

  // Lay out the views.
  const CGFloat width =
      std::max(password_manager::mac::ui::kDesiredBubbleWidth -
                   2 * password_manager::mac::ui::kFramePadding,
               NSWidth([credentialsView_ frame]));
  [doneButton_
      setFrameOrigin:NSMakePoint(
                         base::i18n::IsRTL()
                             ? password_manager::mac::ui::kFramePadding
                             : password_manager::mac::ui::kFramePadding +
                                   width - NSWidth([doneButton_ frame]),
                         password_manager::mac::ui::kFramePadding)];
  [manageButton_
      setFrameOrigin:NSMakePoint(
                         base::i18n::IsRTL()
                             ? password_manager::mac::ui::kFramePadding +
                                   width - NSWidth([manageButton_ frame])
                             : password_manager::mac::ui::kFramePadding,
                         password_manager::mac::ui::kFramePadding +
                             (NSHeight([doneButton_ frame]) -
                              NSHeight([manageButton_ frame])) /
                                 2.0f)];

  [column setMaxWidth:width];
  [credentialsView_
      setFrameOrigin:NSMakePoint(password_manager::mac::ui::kFramePadding,
                                 NSMaxY([doneButton_ frame]) +
                                     password_manager::mac::ui::
                                         kUnrelatedControlVerticalPadding)];

  [title setFrameOrigin:NSMakePoint(password_manager::mac::ui::kFramePadding,
                                    NSMaxY([credentialsView_ frame]) +
                                        password_manager::mac::ui::
                                            kUnrelatedControlVerticalPadding)];
  [title setFrameSize:NSMakeSize(width, NSHeight([title frame]))];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:title];

  // Compute the frame to hold all the views.
  const CGFloat frameWidth =
      width + 2 * password_manager::mac::ui::kFramePadding;
  const CGFloat frameHeight =
      NSMaxY([title frame]) + password_manager::mac::ui::kFramePadding;
  [view setFrame:NSMakeRect(0, 0, frameWidth, frameHeight)];

  [self setView:view];
}

- (void)onDoneClicked:(id)sender {
  model_->OnDoneClicked();
  [delegate_ viewShouldDismiss];
}

- (void)onManageClicked:(id)sender {
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
    base::scoped_nsobject<ManageCredentialItemViewController> item(
        [[ManageCredentialItemViewController alloc]
            initWithPasswordForm:*form
                           model:model
                        delegate:delegate]);
    [[item view] setAutoresizingMask:NSViewWidthSizable];
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
  return NSHeight([[[credentialItems_ objectAtIndex:row] view] frame]);
}

- (NSCell*)tableView:(NSTableView*)tableView
    dataCellForTableColumn:(NSTableColumn*)tableColumn
                       row:(NSInteger)row {
  return [[[ManageCredentialItemCell alloc]
      initWithItem:[credentialItems_ objectAtIndex:row]] autorelease];
}

@end
