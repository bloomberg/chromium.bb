// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_IMPORTER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_IMPORTER_VIEW_H_
#pragma once

#include "app/combobox_model.h"
#include "base/string16.h"
#include "chrome/browser/importer/importer.h"
#include "views/controls/button/native_button.h"
#include "views/controls/combobox/combobox.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"

namespace views {
class Checkbox;
class Label;
class Window;
}

class Profile;

// ImporterView draws the dialog that allows the user to select what to
// import from other browsers.
// Note: The UI team hasn't defined yet how the import UI will look like.
//       So now use dialog as a placeholder.
class ImporterView : public views::View,
                     public views::DialogDelegate,
                     public views::ButtonListener,
                     public ComboboxModel,
                     public views::Combobox::Listener,
                     public ImportObserver {
 public:
  // Creates a new ImporterView. |initial_state| is a bitmask of ImportItems.
  // Each checkbox for the bits in |initial_state| is checked.
  ImporterView(Profile* profile, int initial_state);
  virtual ~ImporterView();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

  // Overridden from views::DialogDelegate:
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;
  virtual bool IsDialogButtonEnabled(
      MessageBoxFlags::DialogButton button) const;
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual bool Accept();
  virtual views::View* GetContentsView();

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overridden from ComboboxModel:
  virtual int GetItemCount();
  virtual string16 GetItemAt(int index);

  // Overridden from ChromeViews::Combobox::Listener:
  virtual void ItemChanged(views::Combobox* combobox,
                           int prev_index,
                           int new_index);

  // Overridden from ImportObserver:
  virtual void ImportCanceled();
  virtual void ImportComplete();

 private:
  // Initializes the controls on the dialog.
  void SetupControl();

  // Creates and initializes a new check-box.
  views::Checkbox* InitCheckbox(const std::wstring& text, bool checked);

  // Create a bitmap from the checkboxes of the view.
  uint16 GetCheckedItems();

  // Enables/Disables all the checked items for the given state.
  void SetCheckedItemsState(uint16 items);

  // Sets all checked items in the given state.
  void SetCheckedItems(uint16 items);

  views::Label* import_from_label_;
  views::Combobox* profile_combobox_;
  views::Label* import_items_label_;
  views::Checkbox* history_checkbox_;
  views::Checkbox* favorites_checkbox_;
  views::Checkbox* passwords_checkbox_;
  views::Checkbox* search_engines_checkbox_;

  scoped_refptr<ImporterHost> importer_host_;

  // Stores the state of the checked items associated with the position of the
  // selected item in the combo-box.
  std::vector<uint16> checkbox_items_;

  // Initial state of the |checkbox_items_|.
  uint16 initial_state_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ImporterView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_IMPORTER_VIEW_H_
