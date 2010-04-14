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
#include "chrome/browser/pref_member.h"
#include "chrome/browser/views/options/options_page_view.h"
#include "views/controls/button/native_button.h"
#include "views/controls/label.h"
#include "views/controls/table/table_view2.h"
#include "views/controls/table/table_view_observer.h"
#include "views/grid_layout.h"
#include "views/window/dialog_delegate.h"

class Profile;

namespace chromeos {

class InputMethodButton;
class InputMethodRadioButton;
class PreferredLanguageTableModel;
// A dialog box for configuring the languages.
class LanguageConfigView : public TableModel,
                           public views::ButtonListener,
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

  // Invoked when a language is added from the add button.
  void OnAddLanguage(const std::string& language_code);

  // OptionsPageView overrides.
  virtual void InitControlLayout();

  // NotificationObserver overrides.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Gets the list of supported language codes like "en" and "ja".
  void GetSupportedLanguageCodes(
      std::vector<std::string>* out_language_codes) const;

  // Rewrites the language name and returns the modified version if
  // necessary. Otherwise, returns the given language name as is.
  // In particular, this rewrites the special language name used for input
  // methods that don't fall under any other languages.
  static std::wstring MaybeRewriteLanguageName(
      const std::wstring& language_name);

  // Shows the language config dialog in a new window.
  static void Show(Profile* profile);

 private:
  // Initializes the input method config view.
  void InitInputMethodConfigViewMap();

  // Initializes id_to_{code,display_name}_map_ member variables.
  void InitInputMethodIdMaps();

  // Initializes the input method radio buttons.
  void InitInputMethodRadioButtons();

  // Creates the contents on the left, including the language table.
  views::View* CreateContentsOnLeft();

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

  // Gets the list of supported IME IDs like "pinyin" and "m17n:ar:kbd".
  void GetSupportedInputMethodIds(
      std::vector<std::string>* out_input_method_ids) const;

  // Converts an input method ID to a language code of the IME. Returns "Eng"
  // when |input_method_id| is unknown.
  // Example: "hangul" => "ko"
  std::string GetLanguageCodeFromId(const std::string& input_method_id) const;

  // Converts an input method ID to a display name of the IME. Returns "English"
  // when |input_method_id| is unknown.
  // Examples: "pinyin" => "Pinyin"
  //           "m17n:ar:kbd" => "kbd (m17n)"
  std::string GetDisplayNameFromId(const std::string& input_method_id) const;

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

  // The radio buttons for activating input methods for a language.
  // TODO(satorux): Remove this once we get rid of the hack in
  // InitInputMethodRadioButtons().
  std::set<InputMethodRadioButton*> input_method_radio_buttons_;

  views::View* root_container_;
  views::View* right_container_;
  views::NativeButton* remove_language_button_;
  views::TableView2* preferred_language_table_;

  StringPrefMember preload_engines_;
  std::map<std::string, std::string> id_to_language_code_map_;
  std::map<std::string, std::string> id_to_display_name_map_;

  DISALLOW_COPY_AND_ASSIGN(LanguageConfigView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_CONFIG_VIEW_H_
