// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_OPTIONS_AUTOFILL_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_OPTIONS_AUTOFILL_OPTIONS_HANDLER_H_

#include <string>

#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/dom_ui/options/options_ui.h"

class DictionaryValue;
class ListValue;

class AutoFillOptionsHandler : public OptionsPageUIHandler,
                               public PersonalDataManager::Observer {
 public:
  AutoFillOptionsHandler();
  virtual ~AutoFillOptionsHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);
  virtual void Initialize();
  virtual void RegisterMessages();

  // PersonalDataManager::Observer implementation.
  virtual void OnPersonalDataLoaded();
  virtual void OnPersonalDataChanged();

 private:
  // Loads the strings for the address and credit card overlays.
  void SetAddressOverlayStrings(DictionaryValue* localized_strings);
  void SetCreditCardOverlayStrings(DictionaryValue* localized_strings);

  // Loads AutoFill addresses and credit cards using the PersonalDataManager.
  void LoadAutoFillData();

  // Removes either an address or a credit card, depending on the type of the
  // profile.
  // |args| - A string, the GUID of the profile to remove.
  void RemoveAutoFillProfile(const ListValue* args);

  // Requests profile data for a specific profile. Calls into DOMUI with the
  // loaded profile data to open the appropriate editor, depending on the type
  // of the profile.
  // |args| - A string, the GUID of the profile to load.
  void LoadProfileEditor(const ListValue* args);

  // Adds or updates an address, depending on the GUID of the profile. If the
  // GUID is empty, a new address is added to the WebDatabase; otherwise, the
  // address with the matching GUID is updated. Called from DOMUI.
  // |args| - an array containing the GUID of the address followed by the
  // address data.
  void SetAddress(const ListValue* args);

  // Adds or updates a credit card, depending on the GUID of the profile. If the
  // GUID is empty, a new credit card is added to the WebDatabase; otherwise,
  // the credit card with the matching GUID is updated. Called from DOMUI.
  // |args| - an array containing the GUID of the credit card followed by the
  // credit card data.
  void SetCreditCard(const ListValue* args);

  // Loads the data from an address and sends this data back to the DOMUI to
  // show in the address editor. |guid| is the GUID of the profile to load.
  void EditAddress(const std::string& guid);

  // Loads the data from a credit card and sends this data back to the DOMUI to
  // show in the credit card editor. |guid| is the GUID of the profile to load.
  void EditCreditCard(const std::string& guid);

  // The personal data manager, used to load AutoFill profiles and credit cards.
  // Unowned pointer, may not be NULL.
  PersonalDataManager* personal_data_;

  DISALLOW_COPY_AND_ASSIGN(AutoFillOptionsHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_OPTIONS_AUTOFILL_OPTIONS_HANDLER_H_
