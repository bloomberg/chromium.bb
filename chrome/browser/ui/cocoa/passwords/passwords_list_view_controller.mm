// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/passwords_list_view_controller.h"

#include <utility>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/cocoa/chrome_style.h"
#import "chrome/browser/ui/cocoa/passwords/base_passwords_content_view_controller.h"
#import "chrome/browser/ui/cocoa/passwords/password_item_views.h"
#import "chrome/browser/ui/cocoa/passwords/passwords_bubble_utils.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/controls/hyperlink_button_cell.h"
#import "ui/base/cocoa/hover_image_button.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/resources/grit/ui_resources.h"
#include "url/origin.h"

namespace {

CGFloat ManagePasswordItemWidth() {
  const CGFloat undoExplanationWidth =
      LabelSize(IDS_MANAGE_PASSWORDS_DELETED).width;
  const CGFloat undoLinkWidth = LabelSize(IDS_MANAGE_PASSWORDS_UNDO).width;
  return std::max(kDesiredRowWidth,
                  undoExplanationWidth + kItemLabelSpacing + undoLinkWidth);
}

NSTextField* Label(const base::string16& text) {
  base::scoped_nsobject<NSTextField> textField(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  InitLabel(textField, text);
  return textField.autorelease();
}

NSTextField* UsernameLabel(const base::string16& text) {
  return Label(text);
}

NSTextField* FederationLabel(const base::string16& text) {
  return Label(text);
}

}  // namespace

@implementation UndoPasswordItemView
- (id)initWithTarget:(id)target action:(SEL)action {
  if ((self = [super init])) {
    // The button should look like a link.
    undoButton_.reset([[NSButton alloc] initWithFrame:NSZeroRect]);
    base::scoped_nsobject<HyperlinkButtonCell> cell([[HyperlinkButtonCell alloc]
        initTextCell:l10n_util::GetNSString(IDS_MANAGE_PASSWORDS_UNDO)]);
    [cell setControlSize:NSSmallControlSize];
    [cell setTextColor:skia::SkColorToCalibratedNSColor(
                           chrome_style::GetLinkColor())];
    [undoButton_ setCell:cell.get()];
    [undoButton_ sizeToFit];
    [undoButton_ setTarget:target];
    [undoButton_ setAction:action];
    [self addSubview:undoButton_];

    // Add the explanation text.
    label_.reset([Label(l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_DELETED))
        retain]);
    [self addSubview:label_];
  }
  return self;
}

#pragma mark PasswordItemTwoColumnView

- (void)layoutWithFirstColumn:(CGFloat)firstWidth
                 secondColumn:(CGFloat)secondWidth {
  const CGFloat width = ManagePasswordItemWidth();
  CGFloat curX = 0;
  CGFloat curY = kRelatedControlVerticalSpacing;
  [label_ setFrameOrigin:NSMakePoint(curX, curY)];
  // The undo button should be right-aligned.
  curX = width - NSWidth([undoButton_ frame]);
  [undoButton_ setFrameOrigin:NSMakePoint(curX, curY)];
  // Move to the top-right of the delete button.
  curX = NSMaxX([undoButton_ frame]);
  curY = NSMaxY([undoButton_ frame]) + kRelatedControlVerticalSpacing;

  // Update the frame.
  [self setFrameSize:NSMakeSize(curX, curY)];
}

- (CGFloat)firstColumnWidth {
  // This view doesn't have columns aligned with username/password.
  return 0;
}

- (CGFloat)secondColumnWidth {
  // This view doesn't have columns aligned with username/password.
  return 0;
}
@end

@implementation UndoPasswordItemView (Testing)
- (NSButton*)undoButton {
  return undoButton_.get();
}
@end

@implementation ManagePasswordItemView
- (id)initWithForm:(const autofill::PasswordForm&)form
            target:(id)target
            action:(SEL)action {
  if ((self = [super init])) {
    deleteButton_.reset([[HoverImageButton alloc] initWithFrame:NSZeroRect]);
    [deleteButton_ setFrameSize:NSMakeSize(chrome_style::GetCloseButtonSize(),
                                           chrome_style::GetCloseButtonSize())];
    [deleteButton_ setBordered:NO];
    [[deleteButton_ cell] setHighlightsBy:NSNoCellMask];
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    [deleteButton_
        setDefaultImage:bundle.GetImageNamed(IDR_CLOSE_2).ToNSImage()];
    [deleteButton_
        setHoverImage:bundle.GetImageNamed(IDR_CLOSE_2_H).ToNSImage()];
    [deleteButton_
        setPressedImage:bundle.GetImageNamed(IDR_CLOSE_2_P).ToNSImage()];
    NSString* deleteTitle = l10n_util::GetNSString(IDS_MANAGE_PASSWORDS_DELETE);
    [deleteButton_ setAccessibilityTitle:deleteTitle];
    [deleteButton_ setTarget:target];
    [deleteButton_ setAction:action];
    [self addSubview:deleteButton_];

    // Add the username.
    usernameField_.reset([UsernameLabel(GetDisplayUsername(form)) retain]);
    [self addSubview:usernameField_];

    if (form.federation_origin.unique()) {
      passwordField_.reset([PasswordLabel(form.password_value) retain]);
    } else {
      base::string16 text = l10n_util::GetStringFUTF16(
          IDS_PASSWORDS_VIA_FEDERATION,
          base::UTF8ToUTF16(form.federation_origin.host()));
      passwordField_.reset([FederationLabel(text) retain]);
    }
    [self addSubview:passwordField_];
  }
  return self;
}

#pragma mark PasswordItemTwoColumnView

- (void)layoutWithFirstColumn:(CGFloat)firstWidth
                 secondColumn:(CGFloat)secondWidth {
  const CGFloat width = ManagePasswordItemWidth();
  std::pair<CGFloat, CGFloat> sizes = GetResizedColumns(
      width - NSWidth([deleteButton_ frame]) - kRelatedControlVerticalSpacing,
      std::make_pair(firstWidth, secondWidth));
  [usernameField_
      setFrameSize:NSMakeSize(sizes.first, NSHeight([usernameField_ frame]))];
  [passwordField_
      setFrameSize:NSMakeSize(sizes.second, NSHeight([passwordField_ frame]))];
  CGFloat curX = 0;
  CGFloat curY = kRelatedControlVerticalSpacing;
  [usernameField_ setFrameOrigin:NSMakePoint(curX, curY)];
  // Move to the right of the username and add the password.
  curX = NSMaxX([usernameField_ frame]) + kItemLabelSpacing;
  [passwordField_ setFrameOrigin:NSMakePoint(curX, curY)];
  // The delete button should be right-aligned.
  curX = width - NSWidth([deleteButton_ frame]);
  [deleteButton_ setFrameOrigin:NSMakePoint(curX, curY)];
  // Move to the top-right of the delete button.
  curX = NSMaxX([deleteButton_ frame]);
  curY = NSMaxY([deleteButton_ frame]) + kRelatedControlVerticalSpacing;

  // Update the frame.
  [self setFrameSize:NSMakeSize(curX, curY)];
}

- (CGFloat)firstColumnWidth {
  return NSWidth([usernameField_ frame]);
}

- (CGFloat)secondColumnWidth {
  return NSWidth([passwordField_ frame]);
}
@end

@implementation ManagePasswordItemView (Testing)
- (NSTextField*)usernameField {
  return usernameField_.get();
}
- (NSTextField*)passwordField {
  return passwordField_.get();
}
- (NSButton*)deleteButton {
  return deleteButton_.get();
}
@end

@implementation PendingPasswordItemView

- (id)initWithForm:(const autofill::PasswordForm&)form {
  if ((self = [super initWithFrame:NSZeroRect])) {
    // Add the username.
    usernameField_.reset([UsernameLabel(GetDisplayUsername(form)) retain]);
    [self addSubview:usernameField_];

    if (form.federation_origin.unique()) {
      passwordField_.reset([PasswordLabel(form.password_value) retain]);
    } else {
      base::string16 text = l10n_util::GetStringFUTF16(
          IDS_PASSWORDS_VIA_FEDERATION,
          base::UTF8ToUTF16(form.federation_origin.host()));
      passwordField_.reset([FederationLabel(text) retain]);
    }
    [self addSubview:passwordField_];
  }
  return self;
}

#pragma mark PasswordItemTwoColumnView

- (void)layoutWithFirstColumn:(CGFloat)firstWidth
                 secondColumn:(CGFloat)secondWidth {
  std::pair<CGFloat, CGFloat> sizes = GetResizedColumns(
      kDesiredRowWidth, std::make_pair(firstWidth, secondWidth));
  [usernameField_
      setFrameSize:NSMakeSize(sizes.first, NSHeight([usernameField_ frame]))];
  [passwordField_
      setFrameSize:NSMakeSize(sizes.second, NSHeight([passwordField_ frame]))];
  CGFloat curX = 0;
  CGFloat curY = kRelatedControlVerticalSpacing;
  [usernameField_ setFrameOrigin:NSMakePoint(curX, curY)];
  // Move to the right of the username and add the password.
  curX = NSMaxX([usernameField_ frame]) + kItemLabelSpacing;
  [passwordField_ setFrameOrigin:NSMakePoint(curX, curY)];
  // Move to the top-right of the password.
  curX = NSMaxX([passwordField_ frame]);
  curY = NSMaxY([passwordField_ frame]) + kRelatedControlVerticalSpacing;

  // Update the frame.
  [self setFrameSize:NSMakeSize(curX, curY)];
}

- (CGFloat)firstColumnWidth {
  return NSWidth([usernameField_ frame]);
}

- (CGFloat)secondColumnWidth {
  return NSWidth([passwordField_ frame]);
}
@end

@implementation PendingPasswordItemView (Testing)

- (NSTextField*)usernameField {
  return usernameField_.get();
}

- (NSTextField*)passwordField {
  return passwordField_.get();
}

@end

@interface ManagePasswordItemViewController ()
- (void)onDeleteClicked:(id)sender;
- (void)onUndoClicked:(id)sender;

// Find the next content view and repaint.
- (void)refresh;

// Find the next content view.
- (void)updateContent;
@end

@implementation ManagePasswordItemViewController

- (id)initWithDelegate:(id<PasswordItemDelegate>)delegate
          passwordForm:(const autofill::PasswordForm*)passwordForm {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    delegate_ = delegate;
    passwordForm_ = passwordForm;
    if ([delegate_ model]->state() ==
            password_manager::ui::PENDING_PASSWORD_STATE ||
        [delegate_ model]->state() ==
            password_manager::ui::PENDING_PASSWORD_UPDATE_STATE)
      state_ = MANAGE_PASSWORD_ITEM_STATE_PENDING;
    else
      state_ = MANAGE_PASSWORD_ITEM_STATE_MANAGE;
    [self updateContent];
  }
  return self;
}

- (void)onDeleteClicked:(id)sender {
  DCHECK_EQ(MANAGE_PASSWORD_ITEM_STATE_MANAGE, state_);
  state_ = MANAGE_PASSWORD_ITEM_STATE_DELETED;
  [self refresh];
  [delegate_ model]->OnPasswordAction(
      *passwordForm_, ManagePasswordsBubbleModel::REMOVE_PASSWORD);
}

- (void)onUndoClicked:(id)sender {
  DCHECK_EQ(MANAGE_PASSWORD_ITEM_STATE_DELETED, state_);
  state_ = MANAGE_PASSWORD_ITEM_STATE_MANAGE;
  [self refresh];
  [delegate_ model]->OnPasswordAction(*passwordForm_,
                                      ManagePasswordsBubbleModel::ADD_PASSWORD);
}

- (void)refresh {
  [self updateContent];
  [self layoutWithFirstColumn:[delegate_ firstColumnMaxWidth]
                 secondColumn:[delegate_ secondColumnMaxWidth]];
}

- (void)updateContent {
  switch (state_) {
    case MANAGE_PASSWORD_ITEM_STATE_PENDING:
      contentView_.reset(
          [[PendingPasswordItemView alloc] initWithForm:*passwordForm_]);
      return;
    case MANAGE_PASSWORD_ITEM_STATE_MANAGE:
      contentView_.reset([[ManagePasswordItemView alloc]
          initWithForm:*passwordForm_
                target:self
                action:@selector(onDeleteClicked:)]);
      return;
    case MANAGE_PASSWORD_ITEM_STATE_DELETED:
      contentView_.reset([[UndoPasswordItemView alloc]
          initWithTarget:self
                  action:@selector(onUndoClicked:)]);
      return;
  };
}

- (void)loadView {
  self.view = [[[NSView alloc] initWithFrame:NSZeroRect] autorelease];
}

#pragma mark PasswordItemTwoColumnView

- (void)layoutWithFirstColumn:(CGFloat)firstWidth
                 secondColumn:(CGFloat)secondWidth {
  [contentView_ layoutWithFirstColumn:firstWidth secondColumn:secondWidth];
  // Update the view size according to the content view size.
  const NSSize contentSize = [contentView_ frame].size;
  [self.view setFrameSize:contentSize];

  // Add the content.
  [self.view setSubviews:@[ contentView_ ]];
}

- (CGFloat)firstColumnWidth {
  return [contentView_ firstColumnWidth];
}

- (CGFloat)secondColumnWidth {
  return [contentView_ secondColumnWidth];
}

@end

@implementation ManagePasswordItemViewController (Testing)

- (ManagePasswordItemState)state {
  return state_;
}

- (NSView*)contentView {
  return contentView_.get();
}

@end

@implementation PasswordsListViewController

@synthesize firstColumnMaxWidth = firstColumnMaxWidth_;
@synthesize secondColumnMaxWidth = secondColumnMaxWidth_;

- (id)initWithModelAndForms:(ManagePasswordsBubbleModel*)model
                      forms:(const PasswordFormsVector*)password_forms {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    base::scoped_nsobject<NSMutableArray> items(
        [[NSMutableArray arrayWithCapacity:password_forms->size()] retain]);
    model_ = model;
    // Create the controllers.
    for (const auto& form : *password_forms) {
      base::scoped_nsobject<ManagePasswordItemViewController> item(
          [[ManagePasswordItemViewController alloc] initWithDelegate:self
                                                        passwordForm:&form]);
      [items addObject:item.get()];
    }
    itemViews_.reset(items.release());
  }
  return self;
}

- (id)initWithModelAndForm:(ManagePasswordsBubbleModel*)model
                      form:(const autofill::PasswordForm*)form {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    base::scoped_nsobject<NSMutableArray> items(
        [[NSMutableArray arrayWithCapacity:1] retain]);
    model_ = model;
    base::scoped_nsobject<ManagePasswordItemViewController> item(
        [[ManagePasswordItemViewController alloc] initWithDelegate:self
                                                      passwordForm:form]);
    [items addObject:item.get()];
    itemViews_.reset(items.release());
  }
  return self;
}

- (void)loadView {
  base::scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:NSZeroRect]);

  // Create the subviews.
  for (id object in itemViews_.get()) {
    ManagePasswordItemViewController* passwordController =
        base::mac::ObjCCast<ManagePasswordItemViewController>(object);
    NSView* itemView = [passwordController view];
    [view addSubview:itemView];
    firstColumnMaxWidth_ =
        std::max(firstColumnMaxWidth_, [passwordController firstColumnWidth]);
    secondColumnMaxWidth_ =
        std::max(secondColumnMaxWidth_, [passwordController secondColumnWidth]);
  }
  // Lay out the items.
  NSPoint curPos = {};
  CGFloat maxX = 0;
  for (id object in [itemViews_ reverseObjectEnumerator]) {
    ManagePasswordItemViewController* passwordController =
        base::mac::ObjCCast<ManagePasswordItemViewController>(object);
    [passwordController layoutWithFirstColumn:firstColumnMaxWidth_
                                 secondColumn:secondColumnMaxWidth_];
    NSView* itemView = [passwordController view];
    // The items stack up on each other.
    [itemView setFrameOrigin:curPos];
    maxX = NSMaxX([itemView frame]);
    curPos.y = NSMaxY([itemView frame]);
  }
  [view setFrameSize:NSMakeSize(maxX, curPos.y)];
  [self setView:view];
}

- (ManagePasswordsBubbleModel*)model {
  return model_;
}

@end

@implementation PasswordsListViewController (Testing)

- (NSArray*)itemViews {
  return itemViews_.get();
}

@end
