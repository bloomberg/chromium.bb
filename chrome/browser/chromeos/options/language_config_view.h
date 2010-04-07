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
#include "views/window/dialog_delegate.h"

class Profile;

namespace chromeos {

class InputMethodButton;
class InputMethodCheckbox;
class PreferredLanguageTableModel;
// A dialog box for showing a password textfield.
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

  // Normalizes the language code and returns the normalized version.
  // The function concverts a two-letter language code to its
  // corresponding three-letter code like "ja" => "jpn". Otherwise,
  // returns the given language code as-is.
  static std::string NormalizeLanguageCode(
      const std::string& language_code);

  // Returns true if the given input method id is for a keyboard layout.
  static bool IsKeyboardLayout(const std::string& input_method_id);

 private:
  // Initializes the input method config view.
  void InitInputMethodConfigViewMap();

  // Initialize id_to_{code,display_name}_map_ member variables.
  void InitLanguageIdMaps();

  // Creates the contents on the left, including the language table.
  views::View* CreateContentsOnLeft();

  // Creates the per-language config view.
  views::View* CreatePerLanguageConfigView(const std::string& language_code);

  // Deactivates the input languages for the given language code.
  void DeactivateInputLanguagesFor(const std::string& language_code);

  // Creates the input method config view based on the given |language_id|.
  // Returns NULL if the config view is not found.
  views::DialogDelegate* CreateInputMethodConfigureView(
      const std::string& language_id);

  // Activates or deactivates an IME whose ID is |language_id|.
  void SetLanguageActivated(const std::string& language_id, bool activated);

  // Returns true if an IME of |language_id| is activated.
  bool LanguageIsActivated(const std::string& language_id);

  // Gets the list of active IME IDs like "pinyin" and "m17n:ar:kbd".
  void GetActiveLanguageIDs(std::vector<std::string>* out_language_ids);

  // Gets the list of supported IME IDs like "pinyin" and "m17n:ar:kbd".
  void GetSupportedLanguageIDs(
      std::vector<std::string>* out_language_ids) const;

  // Converts a language ID to a language code of the IME. Returns "" when
  // |language_id| is unknown.
  // Example: "hangul" => "ko"
  std::string GetLanguageCodeFromID(const std::string& language_id) const;

  // Converts a language ID to a display name of the IME. Returns "" when
  // |language_id| is unknown.
  // Examples: "pinyin" => "Pinyin"
  //           "m17n:ar:kbd" => "kbd (m17n)"
  std::string GetDisplayNameFromID(const std::string& language_id) const;

  // Callback for |preload_engines_| pref updates. Initializes the preferred
  // language codes based on the updated pref value.
  void NotifyPrefChanged();

  // The codes of the preferred languages.
  std::vector<std::string> preferred_language_codes_;
  // The map of the input language id to a pointer to the function for
  // creating the input method configuration dialog.
  typedef views::DialogDelegate* (*CreateDialogDelegateFunction)(Profile*);
  typedef std::map<std::string,
                   CreateDialogDelegateFunction> InputMethodConfigViewMap;
  InputMethodConfigViewMap input_method_config_view_map_;

  // The buttons for configuring input methods for a language.
  std::set<InputMethodButton*> input_method_buttons_;
  // The checkboxes for activating input methods for a language.
  std::set<InputMethodCheckbox*> input_method_checkboxes_;

  views::View* root_container_;
  views::View* right_container_;
  views::NativeButton* add_language_button_;
  views::NativeButton* remove_language_button_;
  views::TableView2* preferred_language_table_;

  StringPrefMember preload_engines_;
  std::map<std::string, std::string> id_to_language_code_map_;
  std::map<std::string, std::string> id_to_display_name_map_;

  DISALLOW_COPY_AND_ASSIGN(LanguageConfigView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_CONFIG_VIEW_H_
