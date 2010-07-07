// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_CONFIG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_CONFIG_VIEW_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "app/table_model.h"
#include "chrome/browser/chromeos/options/language_config_model.h"
#include "chrome/browser/views/options/options_page_view.h"
#include "views/controls/button/native_button.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/label.h"
#include "views/controls/table/table_view2.h"
#include "views/controls/table/table_view_observer.h"
#include "views/grid_layout.h"
#include "views/window/dialog_delegate.h"

namespace chromeos {

class InputMethodButton;
class InputMethodCheckbox;
class PreferredLanguageTableModel;

// A dialog box for configuring the languages.
class LanguageConfigView : public TableModel,
                           public views::ButtonListener,
                           public views::Combobox::Listener,
                           public views::DialogDelegate,
                           public views::TableViewObserver,
                           public OptionsPageView {
 public:
  explicit LanguageConfigView(Profile* profile);
  virtual ~LanguageConfigView();

  // views::ButtonListener overrides.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event);

  // views::DialogDelegate overrides.
  virtual bool IsModal() const { return true; }
  virtual views::View* GetContentsView() { return this; }
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;
  virtual int GetDialogButtons() const {
    return MessageBoxFlags::DIALOGBUTTON_OK;
  }
  virtual std::wstring GetWindowTitle() const;

  // views::View overrides:
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

  // views::TableViewObserver overrides:
  virtual void OnSelectionChanged();

  // TableModel overrides:
  // To workaround crbug.com/38266, implement TreeModel as part of
  // LanguageConfigView class, rather than a separate class.
  // TODO(satorux): Implement TableModel as a separate class once the bug
  // is fixed.
  virtual std::wstring GetText(int row, int column_id);
  virtual void SetObserver(TableModelObserver* observer);
  virtual int RowCount();

  // views::Combobox::Listener overrides:
  virtual void ItemChanged(views::Combobox* combobox,
                           int prev_index,
                           int new_index);

  // OptionsPageView overrides.
  virtual void InitControlLayout();

  // Shows the language config dialog in a new window.
  static void Show(Profile* profile, gfx::NativeWindow parent);

 private:
  // Initializes the input method config view.
  void InitInputMethodConfigViewMap();

  // Invoked when a language is added by the add combobox.
  void OnAddLanguage(const std::string& language_code);

  // Invoked when a language is removed by the remove button.
  void OnRemoveLanguage();

  // Resets the add language combobox to the initial "Add language" state.
  void ResetAddLanguageCombobox();

  // Creates the contents on the left, including the language table.
  views::View* CreateContentsOnLeft();

  // Creates the contents on the bottom, including the add language button.
  views::View* CreateContentsOnBottom();

  // Creates the per-language config view.
  views::View* CreatePerLanguageConfigView(const std::string& language_code);

  // Adds the UI language section in the per-language config view.
  void AddUiLanguageSection(const std::string& language_code,
                            views::GridLayout* layout);

  // Adds the input method section in the per-language config view.
  void AddInputMethodSection(const std::string& language_code,
                             views::GridLayout* layout);

  // Creates the input method config view based on the given |input_method_id|.
  // Returns NULL if the config view is not found.
  views::DialogDelegate* CreateInputMethodConfigureView(
      const std::string& input_method_id);

  // If there is only one input method left, disable the selected method.
  // This is done to prevent the user from disabling all input methods.
  void MaybeDisableLastCheckbox();

  // Enable all input method checkboxes.
  void EnableAllCheckboxes();

  // The model of the view.
  LanguageConfigModel model_;

  // The map of the input method id to a pointer to the function for
  // creating the input method configuration dialog.
  typedef views::DialogDelegate* (*CreateDialogDelegateFunction)(Profile*);
  typedef std::map<std::string,
                   CreateDialogDelegateFunction> InputMethodConfigViewMap;
  InputMethodConfigViewMap input_method_config_view_map_;

  // The checkboxes for activating input methods for a language.
  std::set<InputMethodCheckbox*> input_method_checkboxes_;

  views::View* root_container_;
  views::View* right_container_;
  views::NativeButton* remove_language_button_;
  views::TableView2* preferred_language_table_;
  scoped_ptr<AddLanguageComboboxModel> add_language_combobox_model_;
  views::Combobox* add_language_combobox_;

  DISALLOW_COPY_AND_ASSIGN(LanguageConfigView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_CONFIG_VIEW_H_
