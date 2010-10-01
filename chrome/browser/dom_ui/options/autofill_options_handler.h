// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_OPTIONS_AUTOFILL_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_OPTIONS_AUTOFILL_OPTIONS_HANDLER_H_

#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/dom_ui/options/options_ui.h"

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

  // Adds or updates an address, depending on the unique ID of the address. If
  // the unique ID is 0, a new address is added to the WebDatabase; otherwise,
  // the address with the matching ID is updated. Called from DOMUI.
  // |args| - an array containing the unique ID of the address followed by the
  // address data.
  void UpdateAddress(const ListValue* args);

  // Loads the data from an address and sends this data back to the DOMUI to
  // show in the address editor. Called from DOMUI.
  // |args| - an integer, the unique ID of the address to edit.
  void EditAddress(const ListValue* args);

  // Removes an address from the WebDatabase. Called from DOMUI.
  // |args| - an integer, the unique ID of the address to remove.
  void RemoveAddress(const ListValue* args);

  // Adds or updates a credit card, depending on the unique ID of the credit
  // card. If the unique ID is 0, a new credit card is added to the WebDatabase;
  // otherwise, the credit card with the matching ID is updated. Called from
  // DOMUI.
  // |args| - an array containing the unique ID of the credit card followed by
  // the credit card data.
  void UpdateCreditCard(const ListValue* args);

  // Loads the data from a credit card and sends this data back to the DOMUI to
  // show in the credit card editor. Called from DOMUI.
  // |args| - an integer, the unique ID of the credit card to edit.
  void EditCreditCard(const ListValue* args);

  // Removes a credit card from the WebDatabase. Called from DOMUI.
  // |args| - an integer, the unique ID of the credit card to remove.
  void RemoveCreditCard(const ListValue* args);

  // The personal data manager, used to load AutoFill profiles and credit cards.
  // Unowned pointer, may not be NULL.
  PersonalDataManager* personal_data_;

  DISALLOW_COPY_AND_ASSIGN(AutoFillOptionsHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_OPTIONS_AUTOFILL_OPTIONS_HANDLER_H_
