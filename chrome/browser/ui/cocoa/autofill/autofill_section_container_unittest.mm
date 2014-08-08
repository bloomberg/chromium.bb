// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_section_container.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_models.h"
#include "chrome/browser/ui/autofill/mock_autofill_dialog_view_delegate.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_input_field.h"
#import "chrome/browser/ui/cocoa/autofill/layout_view.h"
#import "chrome/browser/ui/cocoa/menu_button.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/simple_menu_model.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"

using base::ASCIIToUTF16;

namespace {

class AutofillSectionContainerTest : public ui::CocoaTest {
 public:
  AutofillSectionContainerTest() : section_(autofill::SECTION_BILLING) {}
  virtual void SetUp() {
    CocoaTest::SetUp();
    ResetContainer();
  }

  void ResetContainer() {
    container_.reset(
        [[AutofillSectionContainer alloc]
            initWithDelegate:&delegate_
                    forSection:section_]);
    [[test_window() contentView] setSubviews:@[ [container_ view] ]];
  }

 protected:
  base::scoped_nsobject<AutofillSectionContainer> container_;
  testing::NiceMock<autofill::MockAutofillDialogViewDelegate> delegate_;
  autofill::DialogSection section_;
};

}  // namespace

TEST_VIEW(AutofillSectionContainerTest, [container_ view])

TEST_F(AutofillSectionContainerTest, HasSubviews) {
  bool hasLayoutView = false;
  bool hasTextField = false;
  bool hasSuggestButton = false;
  bool hasSuggestionView = false;

  ASSERT_EQ(4U, [[[container_ view] subviews] count]);
  for (NSView* view in [[container_ view] subviews]) {
    if ([view isKindOfClass:[NSTextField class]]) {
      hasTextField = true;
    } else if ([view isKindOfClass:[LayoutView class]]) {
      hasLayoutView = true;
    } else if ([view isKindOfClass:[MenuButton class]]) {
      hasSuggestButton = true;
    } else if ([view isKindOfClass:[NSView class]]) {
      hasSuggestionView = true;
    }
  }

  EXPECT_TRUE(hasSuggestButton);
  EXPECT_TRUE(hasLayoutView);
  EXPECT_TRUE(hasTextField);
  EXPECT_TRUE(hasSuggestionView);
}

TEST_F(AutofillSectionContainerTest, ModelsPopulateComboboxes) {
  using namespace autofill;
  using namespace testing;

  const DetailInput kTestInputs[] = {
    { DetailInput::SHORT, CREDIT_CARD_EXP_MONTH },
    { DetailInput::SHORT, CREDIT_CARD_EXP_4_DIGIT_YEAR },
  };

  DetailInputs inputs;
  inputs.push_back(kTestInputs[0]);
  inputs.push_back(kTestInputs[1]);

  autofill::MonthComboboxModel monthComboModel;
  autofill::YearComboboxModel yearComboModel;
  EXPECT_CALL(delegate_, RequestedFieldsForSection(section_))
      .WillOnce(ReturnRef(inputs));
  EXPECT_CALL(delegate_, ComboboxModelForAutofillType(CREDIT_CARD_EXP_MONTH))
      .WillRepeatedly(Return(&monthComboModel));
  EXPECT_CALL(
    delegate_, ComboboxModelForAutofillType(CREDIT_CARD_EXP_4_DIGIT_YEAR))
      .WillRepeatedly(Return(&yearComboModel));
  ResetContainer();

  NSPopUpButton* popup = base::mac::ObjCCastStrict<NSPopUpButton>(
      [container_ getField:CREDIT_CARD_EXP_MONTH]);
  EXPECT_EQ(13, [popup numberOfItems]);
  EXPECT_NSEQ(@"Month", [popup itemTitleAtIndex:0]);
  EXPECT_NSEQ(@"01", [popup itemTitleAtIndex:1]);
  EXPECT_NSEQ(@"02", [popup itemTitleAtIndex:2]);
  EXPECT_NSEQ(@"03", [popup itemTitleAtIndex:3]);
  EXPECT_NSEQ(@"12", [popup itemTitleAtIndex:12]);

  NSPopUpButton* yearPopup = base::mac::ObjCCastStrict<NSPopUpButton>(
      [container_ getField:CREDIT_CARD_EXP_4_DIGIT_YEAR]);
  EXPECT_EQ(11, [yearPopup numberOfItems]);
  EXPECT_NSEQ(@"Year", [yearPopup itemTitleAtIndex:0]);
};

TEST_F(AutofillSectionContainerTest, OutputMatchesDefinition) {
  using namespace autofill;
  using namespace testing;

  const DetailInput kTestInputs[] = {
    { DetailInput::LONG, EMAIL_ADDRESS },
    { DetailInput::SHORT, CREDIT_CARD_EXP_MONTH },
  };
  autofill::MonthComboboxModel comboModel;
  DetailInputs inputs;
  inputs.push_back(kTestInputs[0]);
  inputs.push_back(kTestInputs[1]);

  EXPECT_CALL(delegate_, RequestedFieldsForSection(section_))
      .WillOnce(ReturnRef(inputs));
  EXPECT_CALL(delegate_, ComboboxModelForAutofillType(EMAIL_ADDRESS))
      .WillRepeatedly(ReturnNull());
  EXPECT_CALL(delegate_, ComboboxModelForAutofillType(CREDIT_CARD_EXP_MONTH))
      .WillRepeatedly(Return(&comboModel));

  ResetContainer();

  NSPopUpButton* popup = base::mac::ObjCCastStrict<NSPopUpButton>(
      [container_ getField:CREDIT_CARD_EXP_MONTH]);
  [popup selectItemWithTitle:@"02"];
  [[container_ getField:EMAIL_ADDRESS] setStringValue:@"magic@example.org"];

  autofill::FieldValueMap output;
  [container_ getInputs:&output];

  ASSERT_EQ(inputs.size(), output.size());
  EXPECT_EQ(ASCIIToUTF16("magic@example.org"), output[inputs[0].type]);
  EXPECT_EQ(ASCIIToUTF16("02"), output[inputs[1].type]);
}

TEST_F(AutofillSectionContainerTest, SuggestionsPopulatedByController) {
  ui::SimpleMenuModel model(NULL);
  model.AddItem(10, ASCIIToUTF16("a"));
  model.AddItem(11, ASCIIToUTF16("b"));

  using namespace autofill;
  using namespace testing;

  EXPECT_CALL(delegate_, MenuModelForSection(section_))
      .WillOnce(Return(&model));

  ResetContainer();
  MenuButton* button = nil;
  for (id item in [[container_ view] subviews]) {
    if ([item isKindOfClass:[MenuButton class]]) {
      button = item;
      break;
    }
  }

  NSMenu* menu = [button attachedMenu];
  // Expect _three_ items - popup menus need an empty first item.
  ASSERT_EQ(3, [menu numberOfItems]);

  EXPECT_NSEQ(@"a", [[menu itemAtIndex:1] title]);
  EXPECT_NSEQ(@"b", [[menu itemAtIndex:2] title]);
}

TEST_F(AutofillSectionContainerTest, FieldsAreInitiallyValid) {
  using namespace autofill;
  using namespace testing;

  const DetailInput kTestInputs[] = {
    { DetailInput::LONG, EMAIL_ADDRESS },
    { DetailInput::SHORT, CREDIT_CARD_EXP_MONTH },
  };

  MonthComboboxModel comboModel;
  DetailInputs inputs;
  inputs.push_back(kTestInputs[0]);
  inputs.push_back(kTestInputs[1]);

  EXPECT_CALL(delegate_, RequestedFieldsForSection(section_))
      .WillOnce(ReturnRef(inputs));
  EXPECT_CALL(delegate_, ComboboxModelForAutofillType(EMAIL_ADDRESS))
      .WillRepeatedly(ReturnNull());
  EXPECT_CALL(delegate_, ComboboxModelForAutofillType(CREDIT_CARD_EXP_MONTH))
      .WillRepeatedly(Return(&comboModel));

  ResetContainer();
  NSControl<AutofillInputField>* field = [container_ getField:EMAIL_ADDRESS];
  EXPECT_FALSE([field invalid]);
  field = [container_ getField:CREDIT_CARD_EXP_MONTH];
  EXPECT_FALSE([field invalid]);
}

TEST_F(AutofillSectionContainerTest, ControllerInformsValidity) {
  using namespace autofill;
  using namespace testing;

  const DetailInput kTestInputs[] = {
    { DetailInput::LONG, EMAIL_ADDRESS },
    { DetailInput::SHORT, CREDIT_CARD_EXP_MONTH },
  };

  MonthComboboxModel comboModel;
  DetailInputs inputs;
  inputs.push_back(kTestInputs[0]);
  inputs.push_back(kTestInputs[1]);

  ValidityMessages validity, validity2;

  validity.Set(EMAIL_ADDRESS,
      ValidityMessage(ASCIIToUTF16("Some error message"), false));
  validity2.Set(CREDIT_CARD_EXP_MONTH,
      ValidityMessage(ASCIIToUTF16("Some other error message"), false));
  EXPECT_CALL(delegate_, RequestedFieldsForSection(section_))
      .WillOnce(ReturnRef(inputs));
  EXPECT_CALL(delegate_, ComboboxModelForAutofillType(EMAIL_ADDRESS))
      .WillRepeatedly(ReturnNull());
  EXPECT_CALL(delegate_, ComboboxModelForAutofillType(CREDIT_CARD_EXP_MONTH))
      .WillRepeatedly(Return(&comboModel));

  ResetContainer();
  autofill::FieldValueMap output;
  [container_ getInputs:&output];
  EXPECT_CALL(delegate_, InputsAreValid(section_, output))
      .WillOnce(Return(validity))
      .WillOnce(Return(validity2));

  [container_ validateFor:VALIDATE_FINAL];
  NSControl<AutofillInputField>* field = [container_ getField:EMAIL_ADDRESS];
  EXPECT_TRUE([field invalid]);
  field = [container_ getField:CREDIT_CARD_EXP_MONTH];
  EXPECT_FALSE([field invalid]);

  [container_ validateFor:VALIDATE_FINAL];
  field = [container_ getField:EMAIL_ADDRESS];
  EXPECT_FALSE([field invalid]);
  field = [container_ getField:CREDIT_CARD_EXP_MONTH];
  EXPECT_TRUE([field invalid]);
}
