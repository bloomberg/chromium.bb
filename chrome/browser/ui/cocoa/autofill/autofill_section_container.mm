// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_section_container.h"

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_textfield.h"
#import "chrome/browser/ui/cocoa/autofill/layout_view.h"
#include "chrome/browser/ui/cocoa/autofill/simple_grid_layout.h"
#import "chrome/browser/ui/cocoa/image_button_cell.h"
#import "chrome/browser/ui/cocoa/menu_button.h"
#import "chrome/browser/ui/cocoa/menu_controller.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Constants used for layouting controls. These variables are copied from
// "ui/views/layout/layout_constants.h".

// Horizontal spacing between controls that are logically related.
const int kRelatedControlHorizontalSpacing = 8;

// Vertical spacing between controls that are logically related.
const int kRelatedControlVerticalSpacing = 8;

// TODO(estade): pull out these constants, and figure out better values
// for them. Note: These are duplicated from Views code.

// Fixed width for the section label.
const int kLabelWidth = 180;

// Padding between section label and section input.
const int kPadding = 30;

// Fixed width for the details section.
const int kDetailsWidth = 300;

// Top/bottom inset for contents of a detail section.
const size_t kDetailSectionInset = 10;

}

@interface AutofillSectionContainer (Internal)

// Create properly styled label for section. Autoreleased.
- (NSTextField*)makeDetailSectionLabel:(NSString*)labelText;

// Create a button offering input suggestions.
- (MenuButton*)makeSuggestionButton;

// Create a view with all inputs requested by |controller_|. Autoreleased.
- (LayoutView*)makeInputControls;

@end

@implementation AutofillSectionContainer

@synthesize section = section_;

- (id)initWithController:(autofill::AutofillDialogController*)controller
              forSection:(autofill::DialogSection)section {
  if (self = [super init]) {
    section_ = section;
    controller_ = controller;
  }
  return self;
}

- (void)getInputs:(autofill::DetailOutputMap*)output {
  for (id input in [inputs_ subviews]) {
    const autofill::DetailInput* detailInput =
        reinterpret_cast<autofill::DetailInput*>([input tag]);
    DCHECK(detailInput);
    NSString* value = [input isKindOfClass:[NSPopUpButton class]] ?
        [input titleOfSelectedItem] : [input stringValue];
    output->insert(
        std::make_pair(detailInput,base::SysNSStringToUTF16(value)));
  }
}

- (void)modelChanged {
  ui::MenuModel* suggestionModel = controller_->MenuModelForSection(section_);
  menuController_.reset([[MenuController alloc] initWithModel:suggestionModel
                                       useWithPopUpButtonCell:YES]);
  NSMenu* menu = [menuController_ menu];

  const BOOL hasSuggestions = [menu numberOfItems] > 0;
  [suggestButton_ setHidden:!hasSuggestions];

  [suggestButton_ setAttachedMenu:menu];
}

- (void)loadView {
  inputs_.reset([[self makeInputControls] retain]);
  string16 labelText = controller_->LabelForSection(section_);

  label_.reset([[self makeDetailSectionLabel:
                   base::SysUTF16ToNSString(labelText)] retain]);

  suggestButton_.reset([[self makeSuggestionButton] retain]);

  [self modelChanged];

  view_.reset([[NSView alloc] initWithFrame:NSZeroRect]);
  [self performLayout];
  [self setView:view_];
  [[self view] setSubviews:@[label_, inputs_, suggestButton_]];
}

- (NSSize)preferredSize {
  NSSize labelSize = [label_ frame].size;  // Assumes sizeToFit was called.
  CGFloat contentHeight = [inputs_ preferredHeightForWidth:kDetailsWidth];
  contentHeight = std::max(contentHeight, labelSize.height);
  contentHeight = std::max(contentHeight, NSHeight([suggestButton_ frame]));

  return NSMakeSize(kLabelWidth + kPadding + kDetailsWidth,
                    contentHeight + 2 * kDetailSectionInset);
}

- (void)performLayout {
  NSSize buttonSize = [suggestButton_ frame].size;  // Assume sizeToFit.
  NSSize labelSize = [label_ frame].size;  // Assumes sizeToFit was called.
  CGFloat controlHeight = [inputs_ preferredHeightForWidth:kDetailsWidth];

  NSRect viewFrame = NSZeroRect;
  viewFrame.size = [self preferredSize];

  NSRect contentFrame = NSInsetRect(viewFrame, 0, kDetailSectionInset);
  NSRect dummy;

  // Set up three content columns. kLabelWidth is first column width,
  // then padding, then have suggestButton and inputs share kDetailsWidth.
  NSRect column[3];
  NSDivideRect(contentFrame, &column[0], &dummy, kLabelWidth, NSMinXEdge);
  NSDivideRect(contentFrame, &column[1], &dummy, kDetailsWidth, NSMaxXEdge);
  NSDivideRect(column[1],
               &column[2], &column[1], buttonSize.width, NSMaxXEdge);

  // Center inputs by height in column 1.
  NSRect controlFrame = column[1];
  int centerOffset = (NSHeight(controlFrame) - controlHeight) / 2;
  controlFrame.origin.x += centerOffset;
  controlFrame.size.height = controlHeight;

  // Align label to right top in column 0.
  NSRect labelFrame;
  NSDivideRect(column[0], &labelFrame, &dummy, labelSize.height, NSMaxYEdge);
  NSDivideRect(labelFrame, &labelFrame, &dummy, labelSize.width, NSMaxXEdge);

  // suggest button is top left of column 2.
  NSRect buttonFrame = column[2];
  NSDivideRect(column[2], &buttonFrame, &dummy, buttonSize.height, NSMaxYEdge);

  [inputs_ setFrame:controlFrame];
  [label_ setFrame:labelFrame];
  [suggestButton_ setFrame:buttonFrame];
  [view_ setFrame:viewFrame];
}

- (NSTextField*)makeDetailSectionLabel:(NSString*)labelText {
  scoped_nsobject<NSTextField> label([[NSTextField alloc] init]);
  [label setFont:
      [[NSFontManager sharedFontManager] convertFont:[label font]
                                         toHaveTrait:NSBoldFontMask]];
  [label setStringValue:labelText];
  [label sizeToFit];
  [label setEditable:NO];
  [label setBordered:NO];
  [label sizeToFit];
  return label.autorelease();
}

- (MenuButton*)makeSuggestionButton {
  scoped_nsobject<MenuButton> button([[MenuButton alloc] init]);

  [button setOpenMenuOnClick:YES];
  [button setBordered:NO];
  [button setShowsBorderOnlyWhileMouseInside:YES];

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  NSImage* image =
      rb.GetNativeImageNamed(IDR_AUTOFILL_DIALOG_MENU_BUTTON).ToNSImage();
  [[button cell] setImage:image
             forButtonState:image_button_cell::kDefaultState];
  image = rb.GetNativeImageNamed(IDR_AUTOFILL_DIALOG_MENU_BUTTON_H).
      ToNSImage();
  [[button cell] setImage:image
             forButtonState:image_button_cell::kHoverState];
  image = rb.GetNativeImageNamed(IDR_AUTOFILL_DIALOG_MENU_BUTTON_P).
      ToNSImage();
  [[button cell] setImage:image
             forButtonState:image_button_cell::kPressedState];
  image = rb.GetNativeImageNamed(IDR_AUTOFILL_DIALOG_MENU_BUTTON_D).
      ToNSImage();
  [[button cell] setImage:image
             forButtonState:image_button_cell::kDisabledState];

  [button sizeToFit];
  return button.autorelease();
}

// TODO(estade): we should be using Chrome-style constrained window padding
// values.
- (LayoutView*)makeInputControls {
  const autofill::DetailInputs& inputs =
      controller_->RequestedFieldsForSection(section_);

  scoped_nsobject<LayoutView> view([[LayoutView alloc] init]);
  [view setLayoutManager:
      scoped_ptr<SimpleGridLayout>(new SimpleGridLayout(view))];
  SimpleGridLayout* layout = [view layoutManager];

  for (autofill::DetailInputs::const_iterator it = inputs.begin();
       it != inputs.end(); ++it) {
    const autofill::DetailInput& input = *it;
    int kColumnSetId = input.row_id;
    ColumnSet* column_set = layout->GetColumnSet(kColumnSetId);
    if (!column_set) {
      // Create a new column set and row.
      column_set = layout->AddColumnSet(kColumnSetId);
      if (it != inputs.begin())
        layout->AddPaddingRow(kRelatedControlVerticalSpacing);
      layout->StartRow(0, kColumnSetId);
    } else {
      // Add a new column to existing row.
      column_set->AddPaddingColumn(kRelatedControlHorizontalSpacing);
      // Must explicitly skip the padding column since we've already started
      // adding views.
      layout->SkipColumns(1);
    }

    column_set->AddColumn(input.expand_weight ? input.expand_weight : 1.0f);

    ui::ComboboxModel* input_model =
        controller_->ComboboxModelForAutofillType(input.type);
    if (input_model) {
      scoped_nsobject<NSPopUpButton> popup(
          [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:YES]);
      for (int i = 0; i < input_model->GetItemCount(); ++i) {
        [popup addItemWithTitle:
            base::SysUTF16ToNSString(input_model->GetItemAt(i))];
      }
      [popup selectItemWithTitle:base::SysUTF16ToNSString(input.initial_value)];
      [popup sizeToFit];
      [popup setTag:reinterpret_cast<NSInteger>(&input)];
      layout->AddView(popup);
    } else {
      scoped_nsobject<AutofillTextField> field(
          [[AutofillTextField alloc] init]);
      [[field cell] setPlaceholderString:
          l10n_util::GetNSStringWithFixup(input.placeholder_text_rid)];
      [[field cell] setIcon:
          controller_->IconForField(
              input.type, input.initial_value).AsNSImage()];
      [[field cell] setInvalid:YES];
      [field sizeToFit];
      [field setTag:reinterpret_cast<NSInteger>(&input)];
      layout->AddView(field);
    }
  }

  return view.autorelease();
}

@end

@implementation AutofillSectionContainer (ForTesting)

- (NSControl*)getField:(autofill::AutofillFieldType)type {
  for (NSControl* control in [inputs_ subviews]) {
    const autofill::DetailInput* detailInput =
        reinterpret_cast<autofill::DetailInput*>([control tag]);
    DCHECK(detailInput);
    if (detailInput->type == type)
      return control;
  }
  return nil;
}

@end