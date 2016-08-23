// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_assistant.h"

#include <memory>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace autofill {
namespace {

class MockAutofillManager : public AutofillManager {
 public:
  MockAutofillManager(TestAutofillDriver* driver, TestAutofillClient* client)
      // Force to use the constructor designated for unit test, but we don't
      // really need personal_data in this test so we pass a NULL pointer.
      : AutofillManager(driver, client, NULL) {}
  virtual ~MockAutofillManager() {}

  MOCK_METHOD5(FillCreditCardForm,
               void(int query_id,
                    const FormData& form,
                    const FormFieldData& field,
                    const CreditCard& credit_card,
                    const base::string16& cvc));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillManager);
};

}  // namespace

class AutofillAssistantTest : public testing::Test {
 protected:
  AutofillAssistantTest()
      : message_loop_(),
        autofill_client_(),
        autofill_driver_(),
        autofill_manager_(&autofill_driver_, &autofill_client_),
        autofill_assistant_(&autofill_manager_) {}

  void EnableAutofillCreditCardAssist() {
    scoped_feature_list_.InitAndEnableFeature(kAutofillCreditCardAssist);
    autofill_client_.set_is_context_secure(true);
  }

  // Returns an initialized FormStructure with credit card form data. To be
  // owned by the caller.
  std::unique_ptr<FormStructure> CreateValidCreditCardForm() {
    std::unique_ptr<FormStructure> form_structure;
    FormData form;

    FormFieldData field;
    field.form_control_type = "text";

    field.label = base::ASCIIToUTF16("Name on Card");
    field.name = base::ASCIIToUTF16("name_on_card");
    form.fields.push_back(field);

    field.label = base::ASCIIToUTF16("Card Number");
    field.name = base::ASCIIToUTF16("card_number");
    form.fields.push_back(field);

    field.label = base::ASCIIToUTF16("Exp Month");
    field.name = base::ASCIIToUTF16("ccmonth");
    form.fields.push_back(field);

    field.label = base::ASCIIToUTF16("Exp Year");
    field.name = base::ASCIIToUTF16("ccyear");
    form.fields.push_back(field);

    field.label = base::ASCIIToUTF16("Verification");
    field.name = base::ASCIIToUTF16("verification");
    form.fields.push_back(field);

    form_structure.reset(new FormStructure(form));
    form_structure->DetermineHeuristicTypes();

    return form_structure;
  }

  base::MessageLoop message_loop_;
  TestAutofillClient autofill_client_;
  testing::NiceMock<TestAutofillDriver> autofill_driver_;
  MockAutofillManager autofill_manager_;
  AutofillAssistant autofill_assistant_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

MATCHER_P(CreditCardMatches, guid, "") {
  return arg.guid() == guid;
}

// If the feature is turned off, CanShowCreditCardAssist() always returns
// false.
TEST_F(AutofillAssistantTest, CanShowCreditCardAssist_FeatureOff) {
  std::unique_ptr<FormStructure> form_structure = CreateValidCreditCardForm();

  std::vector<FormStructure*> form_structures{form_structure.get()};
  EXPECT_FALSE(autofill_assistant_.CanShowCreditCardAssist(form_structures));
}

// Tests that with the feature enabled and proper input,
// CanShowCreditCardAssist() behaves as expected.
TEST_F(AutofillAssistantTest, CanShowCreditCardAssist_FeatureOn) {
  EnableAutofillCreditCardAssist();
  std::unique_ptr<FormStructure> form_structure = CreateValidCreditCardForm();

  std::vector<FormStructure*> form_structures;
  EXPECT_FALSE(autofill_assistant_.CanShowCreditCardAssist(form_structures));

  // With valid input, the function extracts the credit card form properly.
  form_structures.push_back(form_structure.get());
  EXPECT_TRUE(autofill_assistant_.CanShowCreditCardAssist(form_structures));
}

// Tests that with the feature enabled and proper input,
// CanShowCreditCardAssist() behaves as expected for secure vs insecure
// contexts.
TEST_F(AutofillAssistantTest, CanShowCreditCardAssist_FeatureOn_NotSecure) {
  EnableAutofillCreditCardAssist();
  std::unique_ptr<FormStructure> form_structure = CreateValidCreditCardForm();
  std::vector<FormStructure*> form_structures;
  form_structures.push_back(form_structure.get());

  // Cannot be shown if the context is not secure.
  autofill_client_.set_is_context_secure(false);
  EXPECT_FALSE(autofill_assistant_.CanShowCreditCardAssist(form_structures));

  // Can be shown if the context is secure.
  autofill_client_.set_is_context_secure(true);
  EXPECT_TRUE(autofill_assistant_.CanShowCreditCardAssist(form_structures));
}

TEST_F(AutofillAssistantTest, ShowAssistForCreditCard_ValidCard) {
  EnableAutofillCreditCardAssist();
  std::unique_ptr<FormStructure> form_structure = CreateValidCreditCardForm();

  // Will extract the credit card form data.
  std::vector<FormStructure*> form_structures{form_structure.get()};
  EXPECT_TRUE(autofill_assistant_.CanShowCreditCardAssist(form_structures));

  // Create a valid card for the assist.
  CreditCard card;
  test::SetCreditCardInfo(&card, "John Doe", "4111111111111111", "05", "2999");

  // FillCreditCardForm ends up being called after user has accepted the
  // prompt.
  EXPECT_CALL(
      autofill_manager_,
      FillCreditCardForm(kNoQueryId, _, _, CreditCardMatches(card.guid()),
                         /* empty cvc */ base::string16()));

  autofill_assistant_.ShowAssistForCreditCard(card);
}

}  // namespace autofill
