// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace payments {

namespace {

const char kNameFull[] = "Kirby Puckett";
const char kPhoneNumber[] = "6515558946";
const char kPhoneNumberInvalid[] = "123";
const char kEmailAddress[] = "kirby@example.com";
const char kEmailAddressInvalid[] = "kirby";

std::string GetLocale() {
  return g_browser_process->GetApplicationLocale();
}

}  // namespace

class PaymentRequestContactInfoEditorTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestContactInfoEditorTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_contact_details_test.html") {}

  PersonalDataLoadedObserverMock personal_data_observer_;
};

IN_PROC_BROWSER_TEST_F(PaymentRequestContactInfoEditorTest, HappyPath) {
  InvokePaymentRequestUI();
  OpenContactInfoScreen();
  OpenContactInfoEditorScreen();

  SetEditorTextfieldValue(base::ASCIIToUTF16(kNameFull), autofill::NAME_FULL);
  SetEditorTextfieldValue(base::ASCIIToUTF16(kPhoneNumber),
                          autofill::PHONE_HOME_WHOLE_NUMBER);
  SetEditorTextfieldValue(base::ASCIIToUTF16(kEmailAddress),
                          autofill::EMAIL_ADDRESS);

  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  // Wait until the web database has been updated and the notification sent.
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&data_loop));
  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);
  data_loop.Run();

  ASSERT_EQ(1UL, personal_data_manager->GetProfiles().size());
  autofill::AutofillProfile* profile = personal_data_manager->GetProfiles()[0];
  DCHECK(profile);

  EXPECT_EQ(base::ASCIIToUTF16(kNameFull),
            profile->GetInfo(autofill::AutofillType(autofill::NAME_FULL),
                             GetLocale()));
  EXPECT_EQ(base::ASCIIToUTF16(kPhoneNumber),
            profile->GetInfo(
                autofill::AutofillType(autofill::PHONE_HOME_WHOLE_NUMBER),
                GetLocale()));
  EXPECT_EQ(base::ASCIIToUTF16(kEmailAddress),
            profile->GetInfo(autofill::AutofillType(autofill::EMAIL_ADDRESS),
                             GetLocale()));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestContactInfoEditorTest, Validation) {
  InvokePaymentRequestUI();
  OpenContactInfoScreen();
  OpenContactInfoEditorScreen();

  // Insert invalid values into fields which have rules more complex than
  // just emptiness, and an empty string into simple required fields.
  SetEditorTextfieldValue(base::string16(), autofill::NAME_FULL);
  SetEditorTextfieldValue(base::ASCIIToUTF16(kPhoneNumberInvalid),
                          autofill::PHONE_HOME_WHOLE_NUMBER);
  SetEditorTextfieldValue(base::ASCIIToUTF16(kEmailAddressInvalid),
                          autofill::EMAIL_ADDRESS);

  EXPECT_TRUE(IsEditorTextfieldInvalid(autofill::NAME_FULL));
  EXPECT_TRUE(IsEditorTextfieldInvalid(autofill::PHONE_HOME_WHOLE_NUMBER));
  EXPECT_TRUE(IsEditorTextfieldInvalid(autofill::EMAIL_ADDRESS));

  // Correct the problems.
  SetEditorTextfieldValue(base::ASCIIToUTF16(kNameFull), autofill::NAME_FULL);
  SetEditorTextfieldValue(base::ASCIIToUTF16(kPhoneNumber),
                          autofill::PHONE_HOME_WHOLE_NUMBER);
  SetEditorTextfieldValue(base::ASCIIToUTF16(kEmailAddress),
                          autofill::EMAIL_ADDRESS);

  EXPECT_FALSE(IsEditorTextfieldInvalid(autofill::NAME_FULL));
  EXPECT_FALSE(IsEditorTextfieldInvalid(autofill::PHONE_HOME_WHOLE_NUMBER));
  EXPECT_FALSE(IsEditorTextfieldInvalid(autofill::EMAIL_ADDRESS));

  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  // Wait until the web database has been updated and the notification sent.
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&data_loop));
  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);
  data_loop.Run();

  ASSERT_EQ(1UL, personal_data_manager->GetProfiles().size());
  autofill::AutofillProfile* profile = personal_data_manager->GetProfiles()[0];
  DCHECK(profile);

  EXPECT_EQ(base::ASCIIToUTF16(kNameFull),
            profile->GetInfo(autofill::AutofillType(autofill::NAME_FULL),
                             GetLocale()));
  EXPECT_EQ(base::ASCIIToUTF16(kPhoneNumber),
            profile->GetInfo(
                autofill::AutofillType(autofill::PHONE_HOME_WHOLE_NUMBER),
                GetLocale()));
  EXPECT_EQ(base::ASCIIToUTF16(kEmailAddress),
            profile->GetInfo(autofill::AutofillType(autofill::EMAIL_ADDRESS),
                             GetLocale()));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestContactInfoEditorTest, ModifyExisting) {
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  autofill::AutofillProfile incomplete_profile;
  incomplete_profile.SetInfo(autofill::AutofillType(autofill::NAME_FULL),
                             base::ASCIIToUTF16(kNameFull), GetLocale());
  AddAutofillProfile(incomplete_profile);

  InvokePaymentRequestUI();
  OpenContactInfoScreen();

  views::View* list_view = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::CONTACT_INFO_SHEET_LIST_VIEW));
  DCHECK(list_view);
  ClickOnDialogViewAndWait(list_view->child_at(0));

  // Do not set name: This should have been populated when opening the screen.
  EXPECT_EQ(base::ASCIIToUTF16(kNameFull),
            GetEditorTextfieldValue(autofill::NAME_FULL));
  SetEditorTextfieldValue(base::ASCIIToUTF16(kPhoneNumber),
                          autofill::PHONE_HOME_WHOLE_NUMBER);
  SetEditorTextfieldValue(base::ASCIIToUTF16(kEmailAddress),
                          autofill::EMAIL_ADDRESS);

  // Wait until the web database has been updated and the notification sent.
  base::RunLoop save_data_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&save_data_loop));
  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);
  save_data_loop.Run();

  ASSERT_EQ(1UL, personal_data_manager->GetProfiles().size());
  autofill::AutofillProfile* profile = personal_data_manager->GetProfiles()[0];
  DCHECK(profile);

  EXPECT_EQ(base::ASCIIToUTF16(kNameFull),
            profile->GetInfo(autofill::AutofillType(autofill::NAME_FULL),
                             GetLocale()));
  EXPECT_EQ(base::ASCIIToUTF16(kPhoneNumber),
            profile->GetInfo(
                autofill::AutofillType(autofill::PHONE_HOME_WHOLE_NUMBER),
                GetLocale()));
  EXPECT_EQ(base::ASCIIToUTF16(kEmailAddress),
            profile->GetInfo(autofill::AutofillType(autofill::EMAIL_ADDRESS),
                             GetLocale()));
}

}  // namespace payments
