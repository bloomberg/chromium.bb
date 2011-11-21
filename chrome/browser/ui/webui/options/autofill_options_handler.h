// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_AUTOFILL_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_AUTOFILL_OPTIONS_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/autofill/personal_data_manager_observer.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

class PersonalDataManager;

namespace base {
class DictionaryValue;
class ListValue;
}

class AutofillOptionsHandler : public OptionsPageUIHandler,
                               public PersonalDataManagerObserver {
 public:
  AutofillOptionsHandler();
  virtual ~AutofillOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void Initialize() OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // PersonalDataManagerObserver implementation.
  virtual void OnPersonalDataChanged() OVERRIDE;

 private:
  // Loads the strings for the address and credit card overlays.
  void SetAddressOverlayStrings(base::DictionaryValue* localized_strings);
  void SetCreditCardOverlayStrings(base::DictionaryValue* localized_strings);

  // Loads Autofill addresses and credit cards using the PersonalDataManager.
  void LoadAutofillData();

  // Removes an address from the PersonalDataManager.
  // |args| - A string, the GUID of the address to remove.
  void RemoveAddress(const base::ListValue* args);

  // Removes a credit card from the PersonalDataManager.
  // |args| - A string, the GUID of the credit card to remove.
  void RemoveCreditCard(const base::ListValue* args);

  // Requests profile data for a specific address. Calls into WebUI with the
  // loaded profile data to open the address editor.
  // |args| - A string, the GUID of the address to load.
  void LoadAddressEditor(const base::ListValue* args);

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

  // The personal data manager, used to load Autofill profiles and credit cards.
  // Unowned pointer, may not be NULL.
  PersonalDataManager* personal_data_;

  DISALLOW_COPY_AND_ASSIGN(AutofillOptionsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_AUTOFILL_OPTIONS_HANDLER_H_
