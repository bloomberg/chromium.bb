// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The languages page of the Languages & languages options dialog, which
// contains accept-languages and spellchecker language options.
//
// Note that we intentionally do not implement the application locale setting,
// as it does not make sense on Linux, where locale is set through the LANG and
// LC_* environment variables.

#ifndef CHROME_BROWSER_GTK_OPTIONS_LANGUAGES_PAGE_GTK_H_
#define CHROME_BROWSER_GTK_OPTIONS_LANGUAGES_PAGE_GTK_H_

#include <gtk/gtk.h>

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/options_page_base.h"
#include "chrome/common/gtk_tree.h"
#include "chrome/common/pref_member.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class LanguageComboboxModel;
class LanguageOrderTableModel;

class LanguagesPageGtk
    : public OptionsPageBase,
      public gtk_tree::TableAdapter::Delegate {
 public:
  explicit LanguagesPageGtk(Profile* profile);
  virtual ~LanguagesPageGtk();

  GtkWidget* get_page_widget() const {
    return page_;
  }

  // gtk_tree::TableAdapter::Delegate implementation.
  virtual void OnAnyModelUpdate();
  virtual void SetColumnValues(int row, GtkTreeIter* iter);

  // Callback from AddLanguageDialog.
  void OnAddLanguage(const std::string& new_language);

 private:
  // Column ids for |language_order_store_|.
  enum {
    COL_LANG,
    COL_COUNT,
  };

  void Init();

  // Enable buttons based on selection state.
  void EnableControls();

  // Get the row number of the first selected row or -1 if no row is selected.
  int FirstSelectedRowNum();

  // Overridden from OptionsPageBase.
  virtual void NotifyPrefChanged(const std::wstring* pref_name);

  // Callbacks for accept languages widgets.
  static void OnSelectionChanged(GtkTreeSelection *selection,
                                 LanguagesPageGtk* languages_page);
  static void OnAddButtonClicked(GtkButton* button,
                                 LanguagesPageGtk* languages_page);
  static void OnRemoveButtonClicked(GtkButton* button,
                                    LanguagesPageGtk* languages_page);
  static void OnMoveUpButtonClicked(GtkButton* button,
                                    LanguagesPageGtk* languages_page);
  static void OnMoveDownButtonClicked(GtkButton* button,
                                      LanguagesPageGtk* languages_page);

  // Callbacks for spellchecker option widgets.
  static void OnEnableSpellCheckingToggled(GtkToggleButton* toggle_button,
                                           LanguagesPageGtk* languages_page);
  static void OnEnableAutoSpellCheckingToggled(
      GtkToggleButton* toggle_button, LanguagesPageGtk* languages_page);
  static void OnDictionaryLanguageChangedThunk(
      GtkComboBox* combo_box, LanguagesPageGtk* languages_page);
  void OnDictionaryLanguageChanged();

  // The accept languages widgets.
  GtkListStore* language_order_store_;
  GtkWidget* language_order_tree_;
  GtkTreeSelection* language_order_selection_;
  GtkWidget* move_up_button_;
  GtkWidget* move_down_button_;
  GtkWidget* add_button_;
  GtkWidget* remove_button_;

  // The spell checking widgets.
  GtkWidget* dictionary_language_combobox_;
  GtkWidget* enable_autospellcorrect_checkbox_;
  GtkWidget* enable_spellchecking_checkbox_;

  // The widget containing the options for this page.
  GtkWidget* page_;

  // The model for |language_order_store_|.
  scoped_ptr<LanguageOrderTableModel> language_order_table_model_;
  scoped_ptr<gtk_tree::TableAdapter> language_order_table_adapter_;

  // Accept languages pref.
  StringPrefMember accept_languages_;

  // The spellchecker "dictionary language" pref and model.
  StringPrefMember dictionary_language_;
  scoped_ptr<LanguageComboboxModel> dictionary_language_model_;

  // If a language was auto-added to accept_languages_ due to being selected as
  // the dictionary language, it is saved in this string, so that it can be
  // removed if the dictionary language is changed again.
  std::string spellcheck_language_added_;

  // SpellChecker enable pref.
  BooleanPrefMember enable_spellcheck_;

  // Auto spell correction pref.
  BooleanPrefMember enable_autospellcorrect_;

  // Flag to ignore gtk callbacks while we are loading prefs, to avoid
  // then turning around and saving them again.
  bool initializing_;

  friend class LanguagesPageGtkTest;
  FRIEND_TEST(LanguagesPageGtkTest, RemoveAcceptLang);
  FRIEND_TEST(LanguagesPageGtkTest, RemoveMultipleAcceptLang);
  FRIEND_TEST(LanguagesPageGtkTest, MoveAcceptLang);
  FRIEND_TEST(LanguagesPageGtkTest, AddAcceptLang);
  FRIEND_TEST(LanguagesPageGtkTest, EnableSpellChecking);
  FRIEND_TEST(LanguagesPageGtkTest, DictionaryLanguage);

  DISALLOW_COPY_AND_ASSIGN(LanguagesPageGtk);
};

#endif  // CHROME_BROWSER_GTK_OPTIONS_LANGUAGES_PAGE_GTK_H_
