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
#include "chrome/browser/language_combobox_model.h"
#include "chrome/browser/pref_member.h"
#include "chrome/browser/views/options/options_page_view.h"
#include "third_party/cros/chromeos_input_method.h"
#include "views/controls/button/native_button.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/label.h"
#include "views/controls/table/table_view2.h"
#include "views/controls/table/table_view_observer.h"
#include "views/grid_layout.h"
#include "views/window/dialog_delegate.h"

class Profile;

namespace chromeos {

class InputMethodButton;
class InputMethodCheckbox;
class PreferredLanguageTableModel;

// The combobox model is used for adding languages in the language config
// view.
class AddLanguageComboboxModel : public LanguageComboboxModel {
 public:
  AddLanguageComboboxModel(Profile* profile,
                           const std::vector<std::string>& locale_codes);
  // LanguageComboboxModel overrides.
  virtual int GetItemCount();
  virtual std::wstring GetItemAt(int index);

  // Converts the given index (index of the items in the combobox) to the
  // index of the internal language list. The returned index can be used
  // for GetLocaleFromIndex() and GetLanguageNameAt().
  int GetLanguageIndex(int index) const;

  // Marks the given language code to be ignored. Ignored languages won't
  // be shown in the combobox. It would be simpler if we could remove and
  // add language codes from the model, but ComboboxModel does not allow
  // items to be added/removed. Thus we use |ignore_set_| instead.
  void SetIgnored(const std::string& language_code, bool ignored);

 private:
  std::set<std::string> ignore_set_;
  DISALLOW_COPY_AND_ASSIGN(AddLanguageComboboxModel);
};

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

  // Invoked when a language is added by the add combobox.
  void OnAddLanguage(const std::string& language_code);

  // Invoked when a language is removed by the remove button.
  void OnRemoveLanguage();

  // Resets the add language combobox to the initial "Add language" state.
  void ResetAddLanguageCombobox();

  // OptionsPageView overrides.
  virtual void InitControlLayout();

  // NotificationObserver overrides.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // views::Combobox::Listener overrides:
  virtual void ItemChanged(views::Combobox* combobox,
                           int prev_index,
                           int new_index);

  // Rewrites the language name and returns the modified version if
  // necessary. Otherwise, returns the given language name as is.
  // In particular, this rewrites the special language name used for input
  // methods that don't fall under any other languages.
  static std::wstring MaybeRewriteLanguageName(
      const std::wstring& language_name);

  // Converts a language code to a language display name, using the
  // current application locale. MaybeRewriteLanguageName() is called
  // internally.
  // Examples: "fr"    => "French"
  //           "en-US" => "English (United States)"
  static std::wstring GetLanguageDisplayNameFromCode(
      const std::string& language_code);

  // Sorts the given language codes by their corresponding language names,
  // using the unicode string comparator.
  static void SortLanguageCodesByNames(
      std::vector<std::string>* language_codes);

  // Reorders the given input method ids for the language code. For
  // example, if |language_codes| is "fr" and |input_method_ids| contains
  // ["xkb:be::fra", and "xkb:fr::fra"], the list is reordered to
  // ["xkb:fr::fra", and "xkb:be::fra"], so that French keyboard layout
  // comes before Belgian keyboard layout.
  static void ReorderInputMethodIdsForLanguageCode(
      const std::string& language_code,
      std::vector<std::string>* input_method_ids);

  // Shows the language config dialog in a new window.
  static void Show(Profile* profile, gfx::NativeWindow parent);

 private:
  // Initializes the input method config view.
  void InitInputMethodConfigViewMap();

  // Initializes id_to_{code,display_name}_map_ maps,
  // as well as supported_{language_codes,input_method_ids}_ vectors.
  void InitInputMethodIdMapsAndVectors();

  // Adds the given language code and input method pair to the internal maps.
  void AddInputMethodToMaps(const std::string& language_code,
                            const InputMethodDescriptor& input_method);

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

  // Deactivates the input methods for the given language code.
  void DeactivateInputMethodsFor(const std::string& language_code);

  // Creates the input method config view based on the given |input_method_id|.
  // Returns NULL if the config view is not found.
  views::DialogDelegate* CreateInputMethodConfigureView(
      const std::string& input_method_id);

  // Activates or deactivates an IME whose ID is |input_method_id|.
  void SetInputMethodActivated(const std::string& input_method_id,
                               bool activated);

  // Returns true if an IME of |input_method_id| is activated.
  bool InputMethodIsActivated(const std::string& input_method_id);

  // Gets the list of active IME IDs like "pinyin" and "m17n:ar:kbd".
  void GetActiveInputMethodIds(std::vector<std::string>* out_input_method_ids);

  // If there is only one input method left, disable the selected method.
  // This is done to prevent the user from disabling all input methods.
  void MaybeDisableLastCheckbox();

  // Enable all input method checkboxes.
  void EnableAllCheckboxes();

  // Converts an input method ID to a language code of the IME. Returns "Eng"
  // when |input_method_id| is unknown.
  // Example: "hangul" => "ko"
  std::string GetLanguageCodeFromInputMethodId(
      const std::string& input_method_id) const;

  // Converts an input method ID to a display name of the IME. Returns
  // "USA" (US keyboard) when |input_method_id| is unknown.
  // Examples: "pinyin" => "Pinyin"
  //           "m17n:ar:kbd" => "kbd (m17n)"
  std::string GetInputMethodDisplayNameFromId(
      const std::string& input_method_id) const;

  // Gets the list of input method ids associated with the given language
  // code.  The original contents of |input_method_ids| will be lost.
  void GetInputMethodIdsFromLanguageCode(
      const std::string& language_code,
      std::vector<std::string>* input_method_ids) const;

  // Callback for |preload_engines_| pref updates. Initializes the preferred
  // language codes based on the updated pref value.
  void NotifyPrefChanged();

  // The codes of the preferred languages.
  std::vector<std::string> preferred_language_codes_;
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

  StringPrefMember preload_engines_;
  std::map<std::string, std::string> id_to_language_code_map_;
  std::map<std::string, std::string> id_to_display_name_map_;
  // List of supported language codes like "en" and "ja".
  std::vector<std::string> supported_language_codes_;
  // List of supported IME IDs like "pinyin" and "m17n:ar:kbd".
  std::vector<std::string> supported_input_method_ids_;
  // Map from language code to associated input method IDs.
  typedef std::multimap<std::string, std::string> LanguageCodeToIdsMap;
  LanguageCodeToIdsMap language_code_to_ids_map_;

  DISALLOW_COPY_AND_ASSIGN(LanguageConfigView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_CONFIG_VIEW_H_
