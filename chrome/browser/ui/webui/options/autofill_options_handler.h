// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_AUTOFILL_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_AUTOFILL_OPTIONS_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"

namespace autofill {
class AutofillProfile;
class PersonalDataManager;
}  // namespace autofill

namespace base {
class DictionaryValue;
class ListValue;
}

namespace options {

class AutofillOptionsHandler : public OptionsPageUIHandler,
                               public autofill::PersonalDataManagerObserver {
 public:
  AutofillOptionsHandler();
  ~AutofillOptionsHandler() override;

  // OptionsPageUIHandler implementation.
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;
  void InitializeHandler() override;
  void InitializePage() override;
  void RegisterMessages() override;

  // PersonalDataManagerObserver implementation.
  void OnPersonalDataChanged() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(AutofillOptionsHandlerTest, AddressToDictionary);

  // Loads the strings for the address and credit card overlays.
  void SetAddressOverlayStrings(base::DictionaryValue* localized_strings);
  void SetCreditCardOverlayStrings(base::DictionaryValue* localized_strings);

  // Loads Autofill addresses and credit cards using the PersonalDataManager.
  void LoadAutofillData();

  // Removes data from the PersonalDataManager.
  // |args| - A string, the GUID of the address or credit card to remove.
  void RemoveData(const base::ListValue* args);

  // Requests profile data for a specific address. Calls into WebUI with the
  // loaded profile data to open the address editor.
  // |args| - A string, the GUID of the address to load.
  void LoadAddressEditor(const base::ListValue* args);

  // Requests input form layout information for a specific country code. Calls
  // into WebUI with the layout information.
  // |args| - A string, the country code to load.
  void LoadAddressEditorComponents(const base::ListValue* args);

  // Requests profile data for a specific credit card. Calls into WebUI with the
  // loaded profile data to open the credit card editor.
  // |args| - A string, the GUID of the credit card to load.
  void LoadCreditCardEditor(const base::ListValue* args);

  // Adds or updates an address, depending on the GUID of the profile. If the
  // GUID is empty, a new address is added to the WebDatabase; otherwise, the
  // address with the matching GUID is updated. Called from WebUI.
  // |args| - an array containing the GUID of the address followed by the
  // address data.
  void SetAddress(const base::ListValue* args);

  // Adds or updates a credit card, depending on the GUID of the profile. If the
  // GUID is empty, a new credit card is added to the WebDatabase; otherwise,
  // the credit card with the matching GUID is updated. Called from WebUI.
  // |args| - an array containing the GUID of the credit card followed by the
  // credit card data.
  void SetCreditCard(const base::ListValue* args);

  // Validates a list of phone numbers.  The resulting validated list of
  // numbers is then sent back to the WebUI.
  // |args| - an array containing the index of the modified or added number, the
  // array of numbers, and the country code string set on the profile.
  void ValidatePhoneNumbers(const base::ListValue* args);

  // Resets the masked state on the unmasked Wallet card described by the GUID
  // in args[0].
  void RemaskServerCard(const base::ListValue* args);

  // Returns true if |personal_data_| is non-null and loaded.
  bool IsPersonalDataLoaded() const;

  // Fills in |address| with the data format that the options js expects.
  static void AutofillProfileToDictionary(
      const autofill::AutofillProfile& profile,
      base::DictionaryValue* address);

  // The personal data manager, used to load Autofill profiles and credit cards.
  // Unowned pointer, may not be NULL.
  autofill::PersonalDataManager* personal_data_;

  DISALLOW_COPY_AND_ASSIGN(AutofillOptionsHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_AUTOFILL_OPTIONS_HANDLER_H_
