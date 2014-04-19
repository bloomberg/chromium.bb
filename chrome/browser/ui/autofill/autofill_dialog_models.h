// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_MODELS_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_MODELS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/simple_menu_model.h"

namespace autofill {

class SuggestionsMenuModel;

class SuggestionsMenuModelDelegate {
 public:
  virtual ~SuggestionsMenuModelDelegate();

  // Called when a menu item has been activated.
  virtual void SuggestionItemSelected(SuggestionsMenuModel* model,
                                      size_t index) = 0;
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
  void AddKeyedItem(const std::string& key,
                    const base::string16& display_label);

  // As above, but also accepts an image which will be displayed alongside the
  // text.
  void AddKeyedItemWithIcon(const std::string& key,
                            const base::string16& display_label,
                            const gfx::Image& icon);

  // Adds a label with a minor text and its identifying key to the model.
  // Keys needn't be unique.
  void AddKeyedItemWithMinorText(const std::string& key,
                                const base::string16& display_label,
                                const base::string16& display_minor_text);

  // As above, but also accepts an image which will be displayed alongside the
  // text.
  void AddKeyedItemWithMinorTextAndIcon(
      const std::string& key,
      const base::string16& display_label,
      const base::string16& display_minor_text,
      const gfx::Image& icon);

  // Resets the model to empty.
  void Reset();

  // Returns the ID key for the item at |index|.
  std::string GetItemKeyAt(int index) const;

  // Returns the ID key for the item at |checked_item_|, or an empty string if
  // there are no items.
  std::string GetItemKeyForCheckedItem() const;

  // Sets which item is checked.
  void SetCheckedItem(const std::string& item_key);
  void SetCheckedIndex(size_t index);

  int checked_item() const { return checked_item_; }

  // Enable/disable an item by key.
  void SetEnabled(const std::string& item_key, bool enabled);

  // ui::SimpleMenuModel::Delegate implementation.
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

 private:
  // Represents an item in this model.
  struct Item {
    std::string key;  //  The key of the item.
    bool enabled;  // Whether the item is selectable.
  };
  // The items this model represents in presentation order.
  // Note: the index in this vector is the |command_id| of the item.
  std::vector<Item> items_;

  // Returns the command id (and index) of the item by the |key|.
  size_t GetItemIndex(const std::string& item_key);

  SuggestionsMenuModelDelegate* delegate_;

  // The command id (and index) of the item which is currently checked. Only one
  // item is checked at a time.
  int checked_item_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsMenuModel);
};

// A model for possible months in the Gregorian calendar.
class MonthComboboxModel : public ui::ComboboxModel {
 public:
  MonthComboboxModel();
  virtual ~MonthComboboxModel();

  static base::string16 FormatMonth(int index);

  // ui::Combobox implementation:
  virtual int GetItemCount() const OVERRIDE;
  virtual base::string16 GetItemAt(int index) OVERRIDE;

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
  virtual base::string16 GetItemAt(int index) OVERRIDE;

 private:
  // The current year (e.g., 2012).
  int this_year_;

  DISALLOW_COPY_AND_ASSIGN(YearComboboxModel);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_MODELS_H_
