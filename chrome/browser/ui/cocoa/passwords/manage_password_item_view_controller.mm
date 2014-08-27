// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/manage_password_item_view_controller.h"

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_content_view_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/controls/hyperlink_button_cell.h"
#import "ui/base/cocoa/hover_image_button.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/native_theme/common_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/layout/layout_constants.h"

using namespace password_manager::mac::ui;

namespace {

const CGFloat kBorderWidth = 1;
const SkColor kHoverColor = SkColorSetARGBInline(0xFF, 0xEB, 0xEB, 0xEB);

NSColor* HoverColor() {
  return gfx::SkColorToCalibratedNSColor(kHoverColor);
}

NSFont* LabelFont() {
  return [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
}

NSSize LabelSize(int resourceID) {
  return [l10n_util::GetNSString(resourceID)
      sizeWithAttributes:@{NSFontAttributeName : LabelFont()}];
}

CGFloat FirstFieldWidth() {
  const CGFloat undoExplanationWidth =
      LabelSize(IDS_MANAGE_PASSWORDS_DELETED).width;
  const CGFloat kUsernameWidth =
      ManagePasswordsBubbleModel::UsernameFieldWidth();
  const CGFloat width = std::max(kUsernameWidth, undoExplanationWidth);
  return width;
}

CGFloat SecondFieldWidth() {
  const CGFloat undoLinkWidth =
      LabelSize(IDS_MANAGE_PASSWORDS_UNDO).width;
  const CGFloat kPasswordWidth =
      ManagePasswordsBubbleModel::PasswordFieldWidth();
  const CGFloat width = std::max(kPasswordWidth, undoLinkWidth);
  return width;
}

CGFloat ItemWidth() {
  const CGFloat width =
      kFramePadding +
      FirstFieldWidth() +
      views::kItemLabelSpacing +
      SecondFieldWidth() +
      views::kItemLabelSpacing +
      chrome_style::GetCloseButtonSize() +
      kFramePadding;
  return width;
}

void InitLabel(NSTextField* textField, const base::string16& text) {
  [textField setStringValue:base::SysUTF16ToNSString(text)];
  [textField setEditable:NO];
  [textField setSelectable:NO];
  [textField setDrawsBackground:NO];
  [textField setBezeled:NO];
  [textField setFont:LabelFont()];
  [textField sizeToFit];
}

NSTextField* Label(const base::string16& text) {
  base::scoped_nsobject<NSTextField> textField(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  InitLabel(textField, text);
  return textField.autorelease();
}

NSTextField* UsernameLabel(const base::string16& text) {
  NSTextField* textField = Label(text);
  [textField
      setFrameSize:NSMakeSize(FirstFieldWidth(), NSHeight([textField frame]))];
  return textField;
}

NSSecureTextField* PasswordLabel(const base::string16& text) {
  base::scoped_nsobject<NSSecureTextField> textField(
      [[NSSecureTextField alloc] initWithFrame:NSZeroRect]);
  InitLabel(textField, text);
  [textField
      setFrameSize:NSMakeSize(SecondFieldWidth(), NSHeight([textField frame]))];
  return textField.autorelease();
}

}  // namespace

@implementation ManagePasswordItemUndoView
- (id)initWithTarget:(id)target action:(SEL)action {
  if ((self = [super init])) {
    // The button should look like a link.
    undoButton_.reset([[NSButton alloc] initWithFrame:NSZeroRect]);
    base::scoped_nsobject<HyperlinkButtonCell> cell([[HyperlinkButtonCell alloc]
        initTextCell:l10n_util::GetNSString(IDS_MANAGE_PASSWORDS_UNDO)]);
    [cell setControlSize:NSSmallControlSize];
    [cell setShouldUnderline:NO];
    [cell setUnderlineOnHover:NO];
    [cell setTextColor:gfx::SkColorToCalibratedNSColor(
        chrome_style::GetLinkColor())];
    [undoButton_ setCell:cell.get()];
    [undoButton_ sizeToFit];
    [undoButton_ setTarget:target];
    [undoButton_ setAction:action];

    const CGFloat width = ItemWidth();
    CGFloat curX = kFramePadding;
    CGFloat curY = views::kRelatedControlVerticalSpacing;

    // Add the explanation text.
    NSTextField* label =
        Label(l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_DELETED));
    [label setFrameOrigin:NSMakePoint(curX, curY)];
    [self addSubview:label];

    // The undo button should be right-aligned.
    curX = width - kFramePadding - NSWidth([undoButton_ frame]);
    [undoButton_ setFrameOrigin:NSMakePoint(curX, curY)];
    [self addSubview:undoButton_ ];

    // Move to the top-right of the delete button.
    curX = NSMaxX([undoButton_ frame]) + kFramePadding;
    curY = NSMaxY([undoButton_ frame]) + views::kRelatedControlVerticalSpacing;

    // Update the frame.
    DCHECK_EQ(width, curX);
    [self setFrameSize:NSMakeSize(curX, curY)];
  }
  return self;
}
@end

@implementation ManagePasswordItemUndoView (Testing)
- (NSButton*)undoButton {
  return undoButton_.get();
}
@end

@implementation ManagePasswordItemManageView
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
    [deleteButton_ setTarget:target];
    [deleteButton_ setAction:action];

    const CGFloat width = ItemWidth();
    CGFloat curX = kFramePadding;
    CGFloat curY = views::kRelatedControlVerticalSpacing;

    // Add the username.
    usernameField_.reset([UsernameLabel(form.username_value) retain]);
    [usernameField_ setFrameOrigin:NSMakePoint(curX, curY)];
    [self addSubview:usernameField_];

    // Move to the right of the username and add the password.
    curX = NSMaxX([usernameField_ frame]) + views::kItemLabelSpacing;
    passwordField_.reset([PasswordLabel(form.password_value) retain]);
    [passwordField_ setFrameOrigin:NSMakePoint(curX, curY)];
    [self addSubview:passwordField_];

    // The delete button should be right-aligned.
    curX = width - kFramePadding - NSWidth([deleteButton_ frame]);
    [deleteButton_ setFrameOrigin:NSMakePoint(curX, curY)];
    [self addSubview:deleteButton_];

    // Move to the top-right of the delete button.
    curX = NSMaxX([deleteButton_ frame]) + kFramePadding;
    curY =
        NSMaxY([deleteButton_ frame]) + views::kRelatedControlVerticalSpacing;

    // Update the frame.
    DCHECK_EQ(width, curX);
    [self setFrameSize:NSMakeSize(curX, curY)];
  }
  return self;
}
@end

@implementation ManagePasswordItemManageView (Testing)
- (NSTextField*)usernameField {
  return usernameField_.get();
}
- (NSSecureTextField*)passwordField {
  return passwordField_.get();
}
- (NSButton*)deleteButton {
  return deleteButton_.get();
}
@end

@implementation ManagePasswordItemPendingView

- (id)initWithForm:(const autofill::PasswordForm&)form {
  if ((self = [super initWithFrame:NSZeroRect])) {
    CGFloat curX = kFramePadding;
    CGFloat curY = views::kRelatedControlVerticalSpacing;

    // Add the username.
    usernameField_.reset([UsernameLabel(form.username_value) retain]);
    [usernameField_ setFrameOrigin:NSMakePoint(curX, curY)];
    [self addSubview:usernameField_];

    // Move to the right of the username and add the password.
    curX = NSMaxX([usernameField_ frame]) + views::kItemLabelSpacing;
    passwordField_.reset([PasswordLabel(form.password_value) retain]);
    [passwordField_ setFrameOrigin:NSMakePoint(curX, curY)];
    [self addSubview:passwordField_];

    // Move to the top-right of the password.
    curY =
        NSMaxY([passwordField_ frame]) + views::kRelatedControlVerticalSpacing;

    // Update the frame.
    [self setFrameSize:NSMakeSize(ItemWidth(), curY)];
  }
  return self;
}

@end

@implementation ManagePasswordItemPendingView (Testing)

- (NSTextField*)usernameField {
  return usernameField_.get();
}

- (NSSecureTextField*)passwordField {
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

// Repaint the content.
- (void)layoutContent;
@end

@implementation ManagePasswordItemViewController

- (id)initWithModel:(ManagePasswordsBubbleModel*)model
       passwordForm:(const autofill::PasswordForm&)passwordForm
           position:(password_manager::ui::PasswordItemPosition)position {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    model_ = model;
    position_ = position;
    passwordForm_ = passwordForm;
    state_ = password_manager::ui::IsPendingState(model_->state())
        ? MANAGE_PASSWORD_ITEM_STATE_PENDING
        : MANAGE_PASSWORD_ITEM_STATE_MANAGE;
    [self updateContent];
  }
  return self;
}

- (void)onDeleteClicked:(id)sender {
  DCHECK_EQ(MANAGE_PASSWORD_ITEM_STATE_MANAGE, state_);
  state_ = MANAGE_PASSWORD_ITEM_STATE_DELETED;
  [self refresh];
  model_->OnPasswordAction(passwordForm_,
                           ManagePasswordsBubbleModel::REMOVE_PASSWORD);
}

- (void)onUndoClicked:(id)sender {
  DCHECK_EQ(MANAGE_PASSWORD_ITEM_STATE_DELETED, state_);
  state_ = MANAGE_PASSWORD_ITEM_STATE_MANAGE;
  [self refresh];
  model_->OnPasswordAction(passwordForm_,
                           ManagePasswordsBubbleModel::ADD_PASSWORD);
}

- (void)refresh {
  [self updateContent];
  [self layoutContent];
}

- (void)updateContent {
  switch (state_) {
    default:
      NOTREACHED();
    case MANAGE_PASSWORD_ITEM_STATE_PENDING:
      contentView_.reset(
          [[ManagePasswordItemPendingView alloc] initWithForm:passwordForm_]);
      return;
    case MANAGE_PASSWORD_ITEM_STATE_MANAGE:
      contentView_.reset([[ManagePasswordItemManageView alloc]
          initWithForm:passwordForm_
                target:self
                action:@selector(onDeleteClicked:)]);
      return;
    case MANAGE_PASSWORD_ITEM_STATE_DELETED:
      contentView_.reset([[ManagePasswordItemUndoView alloc]
          initWithTarget:self
                  action:@selector(onUndoClicked:)]);
      return;
  };
}

- (void)layoutContent {
  // Update the view size according to the content view size.
  const NSSize contentSize = [contentView_ frame].size;
  [self.view setFrameSize:contentSize];

  // Add the content.
  [self.view setSubviews:@[ contentView_ ]];

  // Add the borders, which go along the entire view.
  SkColor borderSkColor;
  ui::CommonThemeGetSystemColor(
      ui::NativeTheme::kColorId_EnabledMenuButtonBorderColor, &borderSkColor);
  CGColorRef borderColor = gfx::CGColorCreateFromSkColor(borderSkColor);

  // Mac views don't have backing layers by default.
  base::scoped_nsobject<CALayer> rootLayer([[CALayer alloc] init]);
  [rootLayer setFrame:NSRectToCGRect(self.view.frame)];
  [self.view setLayer:rootLayer];
  [self.view setWantsLayer:YES];

  // The top border is only present for the first item.
  if (position_ == password_manager::ui::FIRST_ITEM) {
    base::scoped_nsobject<CALayer> topBorder([[CALayer alloc] init]);
    [topBorder setBackgroundColor:borderColor];
    [topBorder setFrame:CGRectMake(0,
                                   contentSize.height - kBorderWidth,
                                   contentSize.width,
                                   kBorderWidth)];
    [self.view.layer addSublayer:topBorder];
  }

  // The bottom border is always present.
  base::scoped_nsobject<CALayer> bottomBorder([[CALayer alloc] init]);
  [bottomBorder setBackgroundColor:borderColor];
  [bottomBorder setFrame:CGRectMake(0, 0, contentSize.width, kBorderWidth)];
  [self.view.layer addSublayer:bottomBorder];
}

- (void)loadView {
  self.view = [[[NSView alloc] initWithFrame:NSZeroRect] autorelease];
  [self layoutContent];
}

@end

@implementation ManagePasswordItemViewController (Testing)

- (ManagePasswordItemState)state {
  return state_;
}

- (NSView*)contentView {
  return contentView_.get();
}

- (autofill::PasswordForm)passwordForm {
  return passwordForm_;
}

@end

@implementation ManagePasswordItemClickableView

- (void)drawRect:(NSRect)dirtyRect {
  [super drawRect:dirtyRect];
  if (hovering_) {
    [HoverColor() setFill];
    NSRectFill(dirtyRect);
  }
}

- (void)mouseEntered:(NSEvent*)event {
  hovering_ = YES;
  [self setNeedsDisplay:YES];
}

- (void)mouseExited:(NSEvent*)event {
  hovering_ = NO;
  [self setNeedsDisplay:YES];
}

- (void)updateTrackingAreas {
  [super updateTrackingAreas];
  if (trackingArea_.get())
    [self removeTrackingArea:trackingArea_.get()];
  NSTrackingAreaOptions options =
      NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow;
  trackingArea_.reset([[CrTrackingArea alloc] initWithRect:[self bounds]
                                                   options:options
                                                     owner:self
                                                  userInfo:nil]);
  [self addTrackingArea:trackingArea_.get()];
}

@end
