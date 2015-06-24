// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/manage_credential_item_view_controller.h"

#include <algorithm>

#include "base/i18n/rtl.h"
#include "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/passwords/credential_item_view.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "grit/components_strings.h"
#include "grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/controls/hyperlink_button_cell.h"
#import "ui/base/cocoa/hover_image_button.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/resources/grit/ui_resources.h"

namespace {

// Horizontal space betweeen subviews of a credential item.
const CGFloat kItemSubviewsHorizontalSpacing = 20.0f;

// Lays out |leftSubview| and |rightSubview| within |item| so that they stick
// to the left and right sides of |item|, respectively, and resizes |item| to
// hold them. Flipped in RTL.
void LayOutItem(NSView* item, NSView* leftSubview, NSView* rightSubview) {
  const CGFloat width = NSWidth([leftSubview frame]) +
                        kItemSubviewsHorizontalSpacing +
                        NSWidth([rightSubview frame]);
  const CGFloat height =
      std::max(NSHeight([leftSubview frame]), NSHeight([rightSubview frame]));
  [item setFrameSize:NSMakeSize(width, height)];
  [leftSubview
      setFrameOrigin:NSMakePoint(base::i18n::IsRTL()
                                     ? width - NSWidth([leftSubview frame])
                                     : 0,
                                 0)];
  [rightSubview
      setFrameOrigin:NSMakePoint(
                         base::i18n::IsRTL()
                             ? 0
                             : width - NSWidth([rightSubview frame]),
                         (height - NSHeight([rightSubview frame])) / 2.0f)];
  [leftSubview setAutoresizingMask:NSViewWidthSizable |
                                   (base::i18n::IsRTL() ? NSViewMinXMargin
                                                        : NSViewMaxXMargin)];
  [rightSubview setAutoresizingMask:(base::i18n::IsRTL() ? NSViewMaxXMargin
                                                         : NSViewMinXMargin)];
}
}  // namespace

@implementation ManageCredentialItemView : NSView

- (id)initWithPasswordForm:(const autofill::PasswordForm&)passwordForm
                  delegate:(id<CredentialItemDelegate>)delegate
                    target:(id)target
                    action:(SEL)action {
  if ((self = [super initWithFrame:NSZeroRect])) {

    // ----------------------------------------------
    // |      | John Q. Facebooker            |     |
    // | icon | john@somewhere.com            |  X  |
    // |      |                               |     |
    // ----------------------------------------------

    // Create the views.
    credentialItem_.reset([[CredentialItemView alloc]
        initWithPasswordForm:passwordForm
              credentialType:password_manager::CredentialType::
                                 CREDENTIAL_TYPE_PASSWORD
                       style:password_manager_mac::CredentialItemStyle::
                                 ACCOUNT_CHOOSER
                    delegate:delegate]);
    [self addSubview:credentialItem_];

    deleteButton_.reset([[HoverImageButton alloc]
        initWithFrame:NSMakeRect(0, 0, chrome_style::GetCloseButtonSize(),
                                 chrome_style::GetCloseButtonSize())]);
    [deleteButton_ setBordered:NO];
    [[deleteButton_ cell] setHighlightsBy:NSNoCellMask];
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    [deleteButton_
        setDefaultImage:bundle.GetNativeImageNamed(IDR_CLOSE_2).ToNSImage()];
    [deleteButton_
        setHoverImage:bundle.GetNativeImageNamed(IDR_CLOSE_2_H).ToNSImage()];
    [deleteButton_
        setPressedImage:bundle.GetNativeImageNamed(IDR_CLOSE_2_P).ToNSImage()];
    [deleteButton_ setTarget:target];
    [deleteButton_ setAction:action];
    [self addSubview:deleteButton_];

    LayOutItem(self, credentialItem_, deleteButton_);
  }
  return self;
}

@end

@implementation DeletedCredentialItemView : NSView
- (id)initWithTarget:(id)target action:(SEL)action {
  if ((self = [super initWithFrame:NSZeroRect])) {

    // ----------------------------------------------
    // | Such-and-such has been deleted.       [Undo]
    // ----------------------------------------------

    // Create the views.
    base::scoped_nsobject<NSTextField> explanationLabel(
        [[NSTextField alloc] initWithFrame:NSZeroRect]);
    [explanationLabel
        setStringValue:l10n_util::GetNSString(IDS_MANAGE_PASSWORDS_DELETED)];
    NSFont* font = ResourceBundle::GetSharedInstance()
                       .GetFontList(ResourceBundle::SmallFont)
                       .GetPrimaryFont()
                       .GetNativeFont();
    [explanationLabel setFont:font];
    [explanationLabel setEditable:NO];
    [explanationLabel setSelectable:NO];
    [explanationLabel setDrawsBackground:NO];
    [explanationLabel setBezeled:NO];
    [explanationLabel sizeToFit];
    [self addSubview:explanationLabel];

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
    [self addSubview:undoButton_];

    LayOutItem(self, explanationLabel, undoButton_);
  }
  return self;
}

@end

@interface ManageCredentialItemViewController()
- (void)deleteCredential:(id)sender;
- (void)undoDelete:(id)sender;
@end

@implementation ManageCredentialItemViewController
- (id)initWithPasswordForm:(const autofill::PasswordForm&)passwordForm
                     model:(ManagePasswordsBubbleModel*)model
                  delegate:(id<CredentialItemDelegate>)delegate {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    passwordForm_ = passwordForm;
    model_ = model;
    delegate_ = delegate;
    [self changeToManageView];
  }
  return self;
}

- (void)performLayout {
  [self view].subviews = @[ contentView_ ];
  [[self view] setFrameSize:[contentView_ frame].size];
  [contentView_ setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
}

- (void)loadView {
  [self setView:[[[NSView alloc] initWithFrame:NSZeroRect] autorelease]];
  [self performLayout];
}

- (void)deleteCredential:(id)sender {
  model_->OnPasswordAction(passwordForm_,
                           ManagePasswordsBubbleModel::REMOVE_PASSWORD);
  [self changeToDeletedView];
  [self performLayout];
}

- (void)undoDelete:(id)sender {
  model_->OnPasswordAction(passwordForm_,
                           ManagePasswordsBubbleModel::ADD_PASSWORD);
  [self changeToManageView];
  [self performLayout];
}

- (void)changeToManageView {
  contentView_.reset([[ManageCredentialItemView alloc]
      initWithPasswordForm:passwordForm_
                  delegate:delegate_
                    target:self
                    action:@selector(deleteCredential:)]);
}

- (void)changeToDeletedView {
  contentView_.reset([[DeletedCredentialItemView alloc]
      initWithTarget:self
              action:@selector(undoDelete:)]);
}
@end
