// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_section_container.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_section_view.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_suggestion_container.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_textfield.h"
#import "chrome/browser/ui/cocoa/autofill/layout_view.h"
#include "chrome/browser/ui/cocoa/autofill/simple_grid_layout.h"
#import "chrome/browser/ui/cocoa/image_button_cell.h"
#import "chrome/browser/ui/cocoa/menu_button.h"
#include "grit/theme_resources.h"
#import "ui/base/cocoa/menu_controller.h"
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

// Break suggestion text into two lines. TODO(groby): Should be on controller.
void BreakSuggestionText(const string16& text,
                         string16* line1,
                         string16* line2) {
  // TODO(estade): does this localize well?
  string16 line_return(base::ASCIIToUTF16("\n"));
  size_t position = text.find(line_return);
  if (position == string16::npos) {
    *line1 = text;
    line2->clear();
  } else {
    *line1 = text.substr(0, position);
    *line2 = text.substr(position + line_return.length());
  }
}

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

  [self updateSuggestionState];

  // TODO(groby): "Save in Chrome" handling.

  // Always request re-layout on state change.
  id controller = [[view_ window] windowController];
  if ([controller respondsToSelector:@selector(requestRelayout)])
    [controller performSelector:@selector(requestRelayout)];
}

- (void)loadView {
  inputs_.reset([[self makeInputControls] retain]);
  string16 labelText = controller_->LabelForSection(section_);

  label_.reset([[self makeDetailSectionLabel:
                   base::SysUTF16ToNSString(labelText)] retain]);

  suggestButton_.reset([[self makeSuggestionButton] retain]);
  suggestContainer_.reset([[AutofillSuggestionContainer alloc] init]);

  [self modelChanged];
  view_.reset([[AutofillSectionView alloc] initWithFrame:NSZeroRect]);
  [self setView:view_];
  [[self view] setSubviews:
      @[label_, inputs_, [suggestContainer_ view], suggestButton_]];
}

- (NSSize)preferredSize {
  NSSize labelSize = [label_ frame].size;  // Assumes sizeToFit was called.
  CGFloat controlHeight = [inputs_ preferredHeightForWidth:kDetailsWidth];
  if ([inputs_ isHidden])
    controlHeight = [suggestContainer_ preferredSize].height;
  CGFloat contentHeight = std::max(controlHeight, labelSize.height);
  contentHeight = std::max(contentHeight, labelSize.height);
  contentHeight = std::max(contentHeight, NSHeight([suggestButton_ frame]));

  return NSMakeSize(kLabelWidth + kPadding + kDetailsWidth,
                    contentHeight + 2 * kDetailSectionInset);
}

- (void)performLayout {
  NSSize buttonSize = [suggestButton_ frame].size;  // Assume sizeToFit.
  NSSize labelSize = [label_ frame].size;  // Assumes sizeToFit was called.
  CGFloat controlHeight = [inputs_ preferredHeightForWidth:kDetailsWidth];
  if ([inputs_ isHidden])
    controlHeight = [suggestContainer_ preferredSize].height;

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

  [[suggestContainer_ view] setFrame:controlFrame];
  [suggestContainer_ performLayout];
  [inputs_ setFrame:controlFrame];
  [label_ setFrame:labelFrame];
  [suggestButton_ setFrame:buttonFrame];
  [view_ setFrameSize:viewFrame.size];
}

- (NSTextField*)makeDetailSectionLabel:(NSString*)labelText {
  base::scoped_nsobject<NSTextField> label([[NSTextField alloc] init]);
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
  base::scoped_nsobject<MenuButton> button([[MenuButton alloc] init]);

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

  base::scoped_nsobject<LayoutView> view([[LayoutView alloc] init]);
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
      base::scoped_nsobject<NSPopUpButton> popup(
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
      base::scoped_nsobject<AutofillTextField> field(
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

- (void)updateSuggestionState {
  const autofill::SuggestionState& suggestionState =
      controller_->SuggestionStateForSection(section_);
  bool showSuggestions = !suggestionState.text.empty();

  [[suggestContainer_ view] setHidden:!showSuggestions];
  [inputs_ setHidden:showSuggestions];

  string16 line1, line2;
  BreakSuggestionText(suggestionState.text, &line1, &line2);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::Font font = rb.GetFont(ui::ResourceBundle::BaseFont).DeriveFont(
      0, suggestionState.text_style);
  [suggestContainer_ setSuggestionText:base::SysUTF16ToNSString(line1)
                                 line2:base::SysUTF16ToNSString(line2)
                              withFont:font.GetNativeFont()];
  [suggestContainer_ setIcon:suggestionState.icon.AsNSImage()];
  if (!suggestionState.extra_text.empty()) {
    NSString* extraText =
        base::SysUTF16ToNSString(suggestionState.extra_text);
    NSImage* extraIcon = suggestionState.extra_icon.AsNSImage();
    [suggestContainer_ showTextfield:extraText withIcon:extraIcon];
  }
  [view_ setShouldHighlightOnHover:showSuggestions];
}

- (void)update {
  // TODO(groby): Will need to update input fields/support clobbering.
  // cf. AutofillDialogViews::UpdateSectionImpl.
  [self modelChanged];
}

- (void)editLinkClicked {
  controller_->EditClickedForSection(section_);
}

- (NSString*)editLinkTitle {
  return base::SysUTF16ToNSString(controller_->EditSuggestionText());
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
