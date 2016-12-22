// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/manage_passwords_view_controller.h"

#include <cmath>

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/cocoa/chrome_style.h"
#import "chrome/browser/ui/cocoa/passwords/passwords_bubble_utils.h"
#import "chrome/browser/ui/cocoa/passwords/passwords_list_view_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/controls/hyperlink_button_cell.h"
#include "ui/base/l10n/l10n_util.h"

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

@interface ManagePasswordsViewController ()
- (void)onDoneClicked:(id)sender;
- (void)onManageClicked:(id)sender;
@end

@implementation ManagePasswordsViewController

- (void)loadView {
  base::scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:NSZeroRect]);

  // -----------------------------------
  // |  Title                          |
  // |                                 |
  // |    username   password      x   |
  // |    username   password      x   |
  // |    username   password      x   |
  // |                                 |
  // |  Manage                 [Done]  |
  // -----------------------------------

  // The bubble should be wide enough to fit the title row, the username and
  // password rows, and the buttons row on one line each, but not smaller than
  // kDesiredBubbleWidth.

  // Create the elements and add them to the view.
  NSTextField* titleLabel =
      [self addTitleLabel:base::SysUTF16ToNSString(self.model->title())
                   toView:view];

  // Content. If we have a list of passwords to store for the current site, we
  // display them to the user for management. Otherwise, we show a "No passwords
  // for this site" message.
  NSView* contentView = nil;
  if (self.model->local_credentials().empty()) {
    const CGFloat noPasswordsWidth = std::max(
        kDesiredBubbleWidth - 2 * kFramePadding, NSWidth([titleLabel frame]));
    noPasswordsView_.reset(
        [[NoPasswordsView alloc] initWithWidth:noPasswordsWidth]);
    contentView = noPasswordsView_.get();
  } else {
    passwordsListController_.reset([[PasswordsListViewController alloc]
        initWithModelAndForms:self.model
                        forms:&self.model->local_credentials()]);
    contentView = [passwordsListController_ view];
  }
  [view addSubview:contentView];

  // Wrap the title if necessary to match the width of the content view.
  [titleLabel setFrameSize:NSMakeSize(NSWidth([contentView frame]), 0)];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:titleLabel];

  // Done button.
  doneButton_.reset([[self addButton:l10n_util::GetNSString(IDS_DONE)
                              toView:view
                              target:self
                              action:@selector(onDoneClicked:)] retain]);

  // Manage button.
  manageButton_.reset([[NSButton alloc] initWithFrame:NSZeroRect]);
  base::scoped_nsobject<HyperlinkButtonCell> cell([[HyperlinkButtonCell alloc]
      initTextCell:base::SysUTF16ToNSString(self.model->manage_link())]);
  [cell setControlSize:NSSmallControlSize];
  [cell setTextColor:skia::SkColorToCalibratedNSColor(
      chrome_style::GetLinkColor())];
  [manageButton_ setCell:cell.get()];
  [manageButton_ sizeToFit];
  [manageButton_ setTarget:self];
  [manageButton_ setAction:@selector(onManageClicked:)];
  [view addSubview:manageButton_];

  // Layout the elements, starting at the bottom and moving up.

  // The Done button goes in the bottom-right corner.
  const CGFloat width = 2 * kFramePadding + NSWidth([contentView frame]);
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
  [contentView setFrameOrigin:NSMakePoint(curX, curY)];

  // The title goes above the content.
  curY = NSMaxY([contentView frame]) + kUnrelatedControlVerticalPadding;
  [titleLabel setFrameOrigin:NSMakePoint(curX, curY)];

  curX = NSMaxX([contentView frame]) + kFramePadding;
  curY = NSMaxY([titleLabel frame]) + kFramePadding;
  DCHECK_EQ(width, curX);
  [view setFrameSize:NSMakeSize(curX, curY)];

  [self setView:view];
}

- (void)onDoneClicked:(id)sender {
  if (self.model)
    self.model->OnDoneClicked();
  [self.delegate viewShouldDismiss];
}

- (void)onManageClicked:(id)sender {
  if (self.model)
    self.model->OnManageLinkClicked();
  [self.delegate viewShouldDismiss];
}

- (ManagePasswordsBubbleModel*)model {
  return [self.delegate model];
}

- (NSButton*)defaultButton {
  return doneButton_;
}

@end

@implementation ManagePasswordsViewController (Testing)

- (NSButton*)doneButton {
  return doneButton_.get();
}

- (NSButton*)manageButton {
  return manageButton_.get();
}

- (NoPasswordsView*)noPasswordsView {
  return noPasswordsView_.get();
}

- (PasswordsListViewController*)passwordsListController {
  return passwordsListController_.get();
}

@end
