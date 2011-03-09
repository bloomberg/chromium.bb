// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_IMPORTER_IMPORT_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_IMPORTER_IMPORT_DIALOG_VIEW_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/ref_counted.h"
#include "base/string16.h"
#include "chrome/browser/importer/importer_host.h"
#include "chrome/browser/importer/importer_observer.h"
#include "ui/base/models/combobox_model.h"
#include "views/controls/button/native_button.h"
#include "views/controls/combobox/combobox.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"

namespace views {
class Checkbox;
class Label;
class Window;
}

class ImporterList;
class Profile;

// ImportDialogView draws the dialog that allows the user to select what to
// import from other browsers.
class ImportDialogView : public views::View,
                         public views::DialogDelegate,
                         public views::ButtonListener,
                         public ui::ComboboxModel,
                         public views::Combobox::Listener,
                         public ImporterList::Observer,
                         public ImporterObserver {
 public:
  // |initial_state| is a bitmask of importer::ImportItem.
  // Each checkbox for the bits in |initial_state| is checked.
  ImportDialogView(Profile* profile, uint16 initial_state);
  virtual ~ImportDialogView();

 protected:
  // views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;

  // views::DialogDelegate:
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const OVERRIDE;
  virtual bool IsDialogButtonEnabled(
      MessageBoxFlags::DialogButton button) const OVERRIDE;
  virtual bool IsModal() const OVERRIDE;
  virtual std::wstring GetWindowTitle() const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // ui::ComboboxModel:
  virtual int GetItemCount();
  virtual string16 GetItemAt(int index) OVERRIDE;

  // views::Combobox::Listener
  virtual void ItemChanged(views::Combobox* combobox,
                           int prev_index,
                           int new_index) OVERRIDE;

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

  // ImporterList::Observer:
  virtual void SourceProfilesLoaded() OVERRIDE;

  // ImporterObserver:
  virtual void ImportCompleted() OVERRIDE;
  virtual void ImportCanceled() OVERRIDE;

  views::Label* import_from_label_;
  views::Combobox* profile_combobox_;
  views::Label* import_items_label_;
  views::Checkbox* history_checkbox_;
  views::Checkbox* favorites_checkbox_;
  views::Checkbox* passwords_checkbox_;
  views::Checkbox* search_engines_checkbox_;

  // Our current profile.
  Profile* profile_;

  // Utility class that does the actual import.
  scoped_refptr<ImporterHost> importer_host_;

  // Enumerates the source profiles.
  scoped_refptr<ImporterList> importer_list_;

  // Stores the state of the checked items associated with the position of the
  // selected item in the combo-box.
  std::vector<uint16> checkbox_items_;

  // Initial state of the |checkbox_items_|.
  uint16 initial_state_;

  DISALLOW_COPY_AND_ASSIGN(ImportDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_IMPORTER_IMPORT_DIALOG_VIEW_H_
