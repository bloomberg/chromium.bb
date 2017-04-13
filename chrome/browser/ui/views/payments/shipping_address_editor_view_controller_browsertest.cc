// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/validating_textfield.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/country_combobox_model.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/region_combobox_model.h"
#include "components/payments/content/payment_request_spec.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/null_storage.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#include "ui/views/controls/combobox/combobox.h"

namespace payments {

namespace {

const char kNameFull[] = "Bob Jones";
const char kHomeAddress[] = "42 Answers-All Avenue";
const char kHomeCity[] = "Question-City";
const char kHomeZip[] = "ziiiiiip";

}  // namespace

class PaymentRequestShippingAddressEditorTest
    : public PaymentRequestBrowserTestBase,
      public TestChromePaymentRequestDelegate::AddressInputProvider {
 protected:
  PaymentRequestShippingAddressEditorTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_dynamic_shipping_test.html") {}

  void EnableAddressInputOverride() { SetAddressInputOverride(this); }

  // TestChromePaymentRequestDelegate::AddressInputProvider.
  std::unique_ptr<const ::i18n::addressinput::Source> GetAddressInputSource()
      override {
    return base::MakeUnique<TestSource>(address_input_override_data_);
  }

  std::unique_ptr<::i18n::addressinput::Storage> GetAddressInputStorage()
      override {
    return base::MakeUnique<::i18n::addressinput::NullStorage>();
  }

  void SetRequiredFields() {
    SetEditorTextfieldValue(base::ASCIIToUTF16(kNameFull), autofill::NAME_FULL);
    SetEditorTextfieldValue(base::ASCIIToUTF16(kHomeAddress),
                            autofill::ADDRESS_HOME_STREET_ADDRESS);
    SetEditorTextfieldValue(base::ASCIIToUTF16(kHomeCity),
                            autofill::ADDRESS_HOME_CITY);
    SetEditorTextfieldValue(base::ASCIIToUTF16(kHomeZip),
                            autofill::ADDRESS_HOME_ZIP);
  }

  std::string GetSelectedCountryCode() {
    views::Combobox* country_combobox =
        static_cast<views::Combobox*>(dialog_view()->GetViewByID(
            static_cast<int>(autofill::ADDRESS_HOME_COUNTRY)));
    DCHECK(country_combobox);
    int selected_country_row = country_combobox->GetSelectedRow();
    autofill::CountryComboboxModel* model =
        static_cast<autofill::CountryComboboxModel*>(country_combobox->model());

    return model->countries()[selected_country_row]->country_code();
  }

  void SetDefaultCountryData() {
    AddCountryData(GetDataManager()->GetDefaultCountryCodeForNewAddress());
  }

  void AddCountryData(const std::string country_code) {
    std::string country_key("data/");
    country_key += country_code;
    std::string json("{\"");
    json += country_key + "\":{";
    json += "\"id\":\"";
    json += country_key + "\",";
    json += "\"key\":\"";
    json += country_code + "\",";
    json += "\"sub_keys\":\"QC~ON\"},";

    json += "\"";
    json += country_key + "/ON\":{";
    json += "\"id\":\"";
    json += country_key + "/ON\",";
    json += "\"key\":\"ON\",";
    json += "\"name\":\"Ontario\"},";

    json += "\"";
    json += country_key + "/QC\":{";
    json += "\"id\":\"";
    json += country_key + "/QC\",";
    json += "\"key\":\"QC\",";
    json += "\"name\":\"Quebec\"}}";
    address_input_override_data_[country_key] = json;
  }

  void AddCountryDataWithNoRegion(const std::string country_code) {
    std::string country_key("data/");
    country_key += country_code;
    std::string json("{\"");
    json += country_key + "\":{";
    json += "\"id\":\"";
    json += country_key + "\",";
    json += "\"key\":\"";
    json += country_code + "\"}}";
    address_input_override_data_[country_key] = json;
  }

  PersonalDataLoadedObserverMock personal_data_observer_;
  std::map<std::string, std::string> address_input_override_data_;

 private:
  class TestSource : public ::i18n::addressinput::Source {
   public:
    explicit TestSource(const std::map<std::string, std::string>& data)
        : data_(data) {}
    ~TestSource() override {}

    void Get(const std::string& key,
             const ::i18n::addressinput::Source::Callback& data_ready)
        const override {
      if (data_.find(key) == data_.end())
        data_ready(false, key, nullptr);
      else
        data_ready(true, key, new std::string(data_.at(key)));
    }

   private:
    const std::map<std::string, std::string> data_;
  };
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestShippingAddressEditorTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestShippingAddressEditorTest,
                       EnteringValidDataWithDefaultCountry) {
  InvokePaymentRequestUI();

  SetDefaultCountryData();

  EnableAddressInputOverride();

  OpenShippingAddressSectionScreen();

  OpenShippingAddressEditorScreen();

  std::string country_code(GetSelectedCountryCode());

  SetRequiredFields();

  ResetEventObserver(DialogEvent::BACK_NAVIGATION);

  // Verifying the data is in the DB.
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
  EXPECT_EQ(base::ASCIIToUTF16(country_code),
            profile->GetRawInfo(autofill::ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(base::ASCIIToUTF16(kNameFull),
            profile->GetRawInfo(autofill::NAME_FULL));
  EXPECT_EQ(base::ASCIIToUTF16(kHomeAddress),
            profile->GetRawInfo(autofill::ADDRESS_HOME_STREET_ADDRESS));
  EXPECT_EQ(base::ASCIIToUTF16(kHomeCity),
            profile->GetRawInfo(autofill::ADDRESS_HOME_CITY));
  EXPECT_EQ(base::ASCIIToUTF16(kHomeZip),
            profile->GetRawInfo(autofill::ADDRESS_HOME_ZIP));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestShippingAddressEditorTest,
                       SwitchingCountryUpdatesView) {
  InvokePaymentRequestUI();

  SetDefaultCountryData();

  EnableAddressInputOverride();

  OpenShippingAddressSectionScreen();

  OpenShippingAddressEditorScreen();

  views::Combobox* country_combobox =
      static_cast<views::Combobox*>(dialog_view()->GetViewByID(
          static_cast<int>(autofill::ADDRESS_HOME_COUNTRY)));
  ASSERT_NE(nullptr, country_combobox);
  ASSERT_EQ(0, country_combobox->GetSelectedRow());
  autofill::CountryComboboxModel* model =
      static_cast<autofill::CountryComboboxModel*>(country_combobox->model());
  size_t num_countries = model->countries().size();
  ASSERT_GT(num_countries, 10UL);
  bool without_region_data = true;
  for (size_t country_index = 10; country_index < num_countries;
       country_index += num_countries / 10) {
    // The editor updates asynchronously when the country changes.
    ResetEventObserver(DialogEvent::EDITOR_VIEW_UPDATED);
    country_combobox->SetSelectedRow(country_index);
    country_combobox->OnBlur();
    // Some entries don't have country data, e.g., separators.
    if (model->countries()[country_index]) {
      std::string code(model->countries()[country_index]->country_code());
      if (without_region_data)
        AddCountryDataWithNoRegion(code);
      else
        AddCountryData(code);
      without_region_data = !without_region_data;
    }
    // The view update will invalidate the country_combobox / model pointers.
    country_combobox = nullptr;
    model = nullptr;
    WaitForObservedEvent();

    // Make sure the country combo box was properly reset to the chosen country.
    country_combobox = static_cast<views::Combobox*>(dialog_view()->GetViewByID(
        static_cast<int>(autofill::ADDRESS_HOME_COUNTRY)));
    DCHECK(country_combobox);
    EXPECT_EQ(country_index,
              static_cast<size_t>(country_combobox->GetSelectedRow()));

    country_combobox = static_cast<views::Combobox*>(dialog_view()->GetViewByID(
        static_cast<int>(autofill::ADDRESS_HOME_COUNTRY)));
    DCHECK(country_combobox);
    model =
        static_cast<autofill::CountryComboboxModel*>(country_combobox->model());
    ASSERT_EQ(num_countries, model->countries().size());
  }
}

IN_PROC_BROWSER_TEST_F(PaymentRequestShippingAddressEditorTest,
                       FailToLoadRegionData) {
  InvokePaymentRequestUI();

  SetDefaultCountryData();

  EnableAddressInputOverride();

  OpenShippingAddressSectionScreen();

  OpenShippingAddressEditorScreen();

  // The editor updates asynchronously when the regions fail to load.
  ResetEventObserver(DialogEvent::EDITOR_VIEW_UPDATED);
  views::Combobox* country_combobox =
      static_cast<views::Combobox*>(dialog_view()->GetViewByID(
          static_cast<int>(autofill::ADDRESS_HOME_STATE)));
  ASSERT_NE(nullptr, country_combobox);
  autofill::RegionComboboxModel* model =
      static_cast<autofill::RegionComboboxModel*>(country_combobox->model());
  model->SetFailureModeForTests(true);

  // The view update will invalidate the country_combobox / model pointers.
  country_combobox = nullptr;
  model = nullptr;
  WaitForObservedEvent();

  // Now any textual value can be set as the state.
  SetEditorTextfieldValue(base::ASCIIToUTF16("any state"),
                          autofill::ADDRESS_HOME_STATE);
  SetRequiredFields();
  ResetEventObserver(DialogEvent::BACK_NAVIGATION);

  // Verifying the data is in the DB.
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
  EXPECT_EQ(base::ASCIIToUTF16("any state"),
            profile->GetRawInfo(autofill::ADDRESS_HOME_STATE));
}

}  // namespace payments
