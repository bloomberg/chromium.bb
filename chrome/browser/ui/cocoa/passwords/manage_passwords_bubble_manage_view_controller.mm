// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_manage_view_controller.h"

#include <cmath>

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/passwords/manage_password_item_view_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/controls/hyperlink_button_cell.h"
#include "ui/base/l10n/l10n_util.h"

using namespace password_manager::mac::ui;

@implementation PasswordItemListView
- (id)initWithModel:(ManagePasswordsBubbleModel*)model {
  if ((self = [super initWithFrame:NSZeroRect])) {
    base::scoped_nsobject<NSMutableArray> items([[NSMutableArray alloc] init]);

    // Create and lay out the items.
    const CGFloat curX = 0;
    CGFloat maxX = 0;
    CGFloat curY = 0;
    for (autofill::ConstPasswordFormMap::const_reverse_iterator i =
             model->best_matches().rbegin();
         i != model->best_matches().rend();
         ++i) {
      autofill::PasswordForm form = *i->second;
      password_manager::ui::PasswordItemPosition position =
          (&(*i) == &(*model->best_matches().begin()))
              ? password_manager::ui::FIRST_ITEM
              : password_manager::ui::SUBSEQUENT_ITEM;
      base::scoped_nsobject<ManagePasswordItemViewController> item(
          [[ManagePasswordItemViewController alloc] initWithModel:model
                                                     passwordForm:form
                                                         position:position]);
      [items addObject:item.get()];
      NSView* itemView = [item view];
      [self addSubview:itemView];

      // The items stack up on each other.
      [itemView setFrameOrigin:NSMakePoint(curX, curY)];
      maxX = NSMaxX([itemView frame]);
      curY = NSMaxY([itemView frame]);
    }
    [self setFrameSize:NSMakeSize(maxX, curY)];
    itemViews_.reset(items.release());
  }
  return self;
}
@end

@implementation PasswordItemListView (Testing)
- (NSArray*)itemViews {
  return itemViews_.get();
}
@end

@implementation NoPasswordsView
- (id)initWithWidth:(CGFloat)width {
  if ((self = [super initWithFrame:NSZeroRect])) {
    [self setEditable:NO];
    [self setSelectable:NO];
    [self setDrawsBackground:NO];
    [self setBezeled:NO];
    [self setStringValue:l10n_util::GetNSString(
        IDS_MANAGE_PASSWORDS_NO_PASSWORDS)];
    [self setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
    [[self cell] setWraps:YES];
    [self setFrameSize:NSMakeSize(width, MAXFLOAT)];
    [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:self];
  }
  return self;
}
@end

@interface ManagePasswordsBubbleManageViewController ()
- (void)onDoneClicked:(id)sender;
- (void)onManageClicked:(id)sender;
@end

@implementation ManagePasswordsBubbleManageViewController

- (id)initWithModel:(ManagePasswordsBubbleModel*)model
           delegate:(id<ManagePasswordsBubbleContentViewDelegate>)delegate {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    model_ = model;
    delegate_ = delegate;
  }
  return self;
}

- (void)loadView {
  self.view = [[[NSView alloc] initWithFrame:NSZeroRect] autorelease];

  // -----------------------------------
  // |  Title                          |
  // |  -----------------------------  | (1 px border)
  // |    username   password      x   |
  // |  -----------------------------  | (1 px border)
  // |    username   password      x   |
  // |  -----------------------------  | (1 px border)
  // |    username   password      x   |
  // |  -----------------------------  | (1 px border)
  // |  Manage                 [Done]  |
  // -----------------------------------

  // The bubble should be wide enough to fit the title row, the username and
  // password rows, and the buttons row on one line each, but not smaller than
  // kDesiredBubbleWidth.

  // Create the elements and add them to the view.
  NSTextField* titleLabel =
      [self addTitleLabel:base::SysUTF16ToNSString(model_->title())];

  // Content. If we have a list of passwords to store for the current site, we
  // display them to the user for management. Otherwise, we show a "No passwords
  // for this site" message.
  if (model_->best_matches().empty()) {
    const CGFloat noPasswordsWidth = std::max(
        kDesiredBubbleWidth - 2 * kFramePadding, NSWidth([titleLabel frame]));
    contentView_.reset(
        [[NoPasswordsView alloc] initWithWidth:noPasswordsWidth]);
  } else {
    contentView_.reset(
        [[PasswordItemListView alloc] initWithModel:model_]);
  }
  [self.view addSubview:contentView_];
  DCHECK_GE(NSWidth([contentView_ frame]), NSWidth([titleLabel frame]));

  // Done button.
  doneButton_.reset([[self addButton:l10n_util::GetNSString(IDS_DONE)
                              target:self
                              action:@selector(onDoneClicked:)] retain]);

  // Manage button.
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
  [self.view addSubview:manageButton_];

  // Layout the elements, starting at the bottom and moving up.

  // The Done button goes in the bottom-right corner.
  const CGFloat width = 2 * kFramePadding + NSWidth([contentView_ frame]);
  CGFloat curX = width - kFramePadding - NSWidth([doneButton_ frame]);
  CGFloat curY = kFramePadding;
  [doneButton_ setFrameOrigin:NSMakePoint(curX, curY)];

  // The Manage button goes in the bottom-left corner, centered vertically with
  // the Done button.
  curX = kFramePadding;
  const CGFloat diffY = std::ceil(
      (NSHeight([doneButton_ frame]) - NSHeight([manageButton_ frame])) / 2.0);
  [manageButton_ setFrameOrigin:NSMakePoint(curX, curY + diffY)];

  // The content goes above the button row.
  curX = kFramePadding;
  curY = NSMaxY([doneButton_ frame]) + kUnrelatedControlVerticalPadding;
  [contentView_ setFrameOrigin:NSMakePoint(curX, curY)];

  // The title goes above the content.
  curY = NSMaxY([contentView_ frame]) + kUnrelatedControlVerticalPadding;
  [titleLabel setFrameOrigin:NSMakePoint(curX, curY)];

  curX = NSMaxX([contentView_ frame]) + kFramePadding;
  curY = NSMaxY([titleLabel frame]) + kFramePadding;
  DCHECK_EQ(width, curX);
  [self.view setFrameSize:NSMakeSize(curX, curY)];
}

- (void)onDoneClicked:(id)sender {
  model_->OnDoneClicked();
  [delegate_ viewShouldDismiss];
}

- (void)onManageClicked:(id)sender {
  model_->OnManageLinkClicked();
  [delegate_ viewShouldDismiss];
}

@end

@implementation ManagePasswordsBubbleManageViewController (Testing)

- (NSButton*)doneButton {
  return doneButton_.get();
}

- (NSButton*)manageButton {
  return manageButton_.get();
}

- (NSView*)contentView {
  return contentView_.get();
}

@end
