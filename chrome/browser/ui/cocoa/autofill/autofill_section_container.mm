// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_section_container.h"

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_textfield.h"
#import "chrome/browser/ui/cocoa/autofill/layout_view.h"
#include "chrome/browser/ui/cocoa/autofill/simple_grid_layout.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/models/combobox_model.h"

namespace {

// Constants used for layouting controls. These variables are copied from
// "ui/views/layout/layout_constants.h".

// Horizontal spacing between controls that are logically related.
const int kRelatedControlHorizontalSpacing = 8;

// Vertical spacing between controls that are logically related.
const int kRelatedControlVerticalSpacing = 8;

}

@interface AutofillSectionContainer (Internal)

// Create properly styled label for section. Autoreleased.
- (NSTextField*)makeDetailSectionLabel:(NSString*)labelText;

// Create NSView containing inputs & labelling. Autoreleased.
- (NSView*)makeSectionView:(NSString*)labelText
             withControls:(LayoutView*)controls;

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

- (void)loadView {
  inputs_.reset([[self makeInputControls] retain]);
  string16 labelText = controller_->LabelForSection(section_);
  [self setView:[self makeSectionView:base::SysUTF16ToNSString(labelText)
                         withControls:inputs_]];
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

- (NSView*)makeSectionView:(NSString*)labelText
             withControls:(LayoutView*)controls {
  // TODO(estade): pull out these constants, and figure out better values
  // for them. Note: These are duplicated from Views code.
  const int kLabelWidth = 180;
  const int kPadding = 30;
  const int kDetailsWidth = 300;
  const size_t kDetailSectionInset = 10;

  scoped_nsobject<NSTextField> label(
      [[self makeDetailSectionLabel:labelText] retain]);

  CGFloat controlHeight = [controls preferredHeightForWidth:kDetailsWidth];
  NSRect frame = NSZeroRect;
  frame.size.width = kLabelWidth + kPadding + kDetailsWidth;
  frame.size.height = std::max(NSHeight([label frame]), controlHeight) +
                      2 * kDetailSectionInset;
  scoped_nsobject<NSView> section_container(
      [[NSView alloc] initWithFrame:frame]);

  NSPoint labelOrigin = NSMakePoint(
      kLabelWidth - NSWidth([label frame]),
      NSHeight(frame) - NSHeight([label frame]) - kDetailSectionInset);
  [label setFrameOrigin:labelOrigin];
  [label setAutoresizingMask:(NSViewMinYMargin | NSViewMinYMargin)];

  NSRect dummyFrame;
  NSRect controlFrame = [controls frame];
  NSDivideRect(NSInsetRect(frame, 0, kDetailSectionInset),
               &controlFrame, &dummyFrame, kDetailsWidth, NSMaxXEdge);
  controlFrame.size.height = controlHeight;
  [controls setFrame:controlFrame];
  [controls setAutoresizingMask:(NSViewMaxXMargin | NSViewMinYMargin)];

  [section_container setSubviews:@[label, controls]];
  return section_container.autorelease();
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