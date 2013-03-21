// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_MODELS_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_MODELS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/string16.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/simple_menu_model.h"

class PrefService;

namespace autofill {

class SuggestionsMenuModel;

class SuggestionsMenuModelDelegate {
 public:
  virtual ~SuggestionsMenuModelDelegate();

  // Called when a menu item has been activated.
  virtual void SuggestionItemSelected(const SuggestionsMenuModel& model) = 0;
};

// A model for the dropdowns that allow the user to select from different
// sets of known data. It wraps a SimpleMenuModel, providing a mapping between
// index and item GUID.
class SuggestionsMenuModel : public ui::SimpleMenuModel,
                             public ui::SimpleMenuModel::Delegate {
 public:
  explicit SuggestionsMenuModel(SuggestionsMenuModelDelegate* delegate);
  virtual ~SuggestionsMenuModel();

  // Adds an item and its identifying key to the model. Keys needn't be unique.
  void AddKeyedItem(const std::string& key, const string16& display_label);

  // As above, but also accepts an image which will be displayed alongside the
  // text.
  void AddKeyedItemWithIcon(const std::string& key,
                            const string16& display_label,
                            const gfx::Image& icon);

  // Adds a label with a sublabel and its identifying key to the model.
  // Keys needn't be unique.
  void AddKeyedItemWithSublabel(const std::string& key,
                                const string16& display_label,
                                const string16& display_sublabel);

  // As above, but also accepts an image which will be displayed alongside the
  // text.
  void AddKeyedItemWithSublabelAndIcon(const std::string& key,
                                       const string16& display_label,
                                       const string16& display_sublabel,
                                       const gfx::Image& icon);

  // Resets the model to empty.
  void Reset();

  // Returns the ID key for the item at |index|.
  std::string GetItemKeyAt(int index) const;

  // Returns the ID key for the item at |checked_item_|, or an empty string if
  // there are no items.
  std::string GetItemKeyForCheckedItem() const;

  int checked_item() { return checked_item_; }

  // ui::SimpleMenuModel::Delegate implementation.
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

 private:
  // The items this model represents, in presentation order. The first
  // string is the "key" which identifies the item. The second is the
  // display string for the item.
  std::vector<std::pair<std::string, string16> > items_;

  SuggestionsMenuModelDelegate* delegate_;

  // The command id (and index) of the item which is currently checked.
  int checked_item_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsMenuModel);
};

// A delegate interface to allow the AccountChooserModel to inform its owner
// of changes.
class AccountChooserModelDelegate {
 public:
  virtual ~AccountChooserModelDelegate();

  // Called when the active account has changed.
  virtual void AccountChoiceChanged() = 0;
};

// A menu model for the account chooser. This allows users to switch between
// using Wallet and local Autofill. TODO(estade): this should support multiple
// Wallet accounts.
class AccountChooserModel : public ui::SimpleMenuModel,
                            public ui::SimpleMenuModel::Delegate {
 public:
  AccountChooserModel(AccountChooserModelDelegate* delegate,
                      PrefService* prefs);
  virtual ~AccountChooserModel();

  // ui::SimpleMenuModel::Delegate implementation.
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

  // Should be called when the Wallet server returns an error.
  void SetHadWalletError();

  bool had_wallet_error() const { return had_wallet_error_; }

  bool WalletIsSelected() const;

  int checked_item() const { return checked_item_; }

 private:
  void PrefChanged(const std::string& pref);

  // Sets |checked_item_| from the relevant pref.
  void UpdateCheckmarkFromPref();

  // Command IDs of the items in this menu. For now, we only support a single
  // account, so there's only one wallet item.
  static const int kWalletItemId;
  static const int kAutofillItemId;

  AccountChooserModelDelegate* account_delegate_;

  PrefService* prefs_;

  // The command id of the currently active item.
  int checked_item_;

  // Whether there has been a Wallet error while the owning dialog has been
  // open.
  bool had_wallet_error_;

  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(AccountChooserModel);
};

// A model for possible months in the Gregorian calendar.
class MonthComboboxModel : public ui::ComboboxModel {
 public:
  MonthComboboxModel();
  virtual ~MonthComboboxModel();

  static string16 FormatMonth(int index);

  // ui::Combobox implementation:
  virtual int GetItemCount() const OVERRIDE;
  virtual string16 GetItemAt(int index) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(MonthComboboxModel);
};

// A model for years between now and a decade hence.
class YearComboboxModel : public ui::ComboboxModel {
 public:
  YearComboboxModel();
  virtual ~YearComboboxModel();

  // ui::Combobox implementation:
  virtual int GetItemCount() const OVERRIDE;
  virtual string16 GetItemAt(int index) OVERRIDE;

 private:
  // The current year (e.g., 2012).
  int this_year_;

  DISALLOW_COPY_AND_ASSIGN(YearComboboxModel);
};

}  // autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_MODELS_H_
