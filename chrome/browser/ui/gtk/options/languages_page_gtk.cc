// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/options/languages_page_gtk.h"

#include <set>
#include <string>
#include <vector>

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/language_combobox_model.h"
#include "chrome/browser/language_order_table_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/spellcheck_common.h"
#include "grit/generated_resources.h"
#include "ui/base/gtk/gtk_signal.h"

namespace {

const int kWrapWidth = 475;

GtkWidget* NewComboboxFromModel(ui::ComboboxModel* model) {
  GtkWidget* combobox = gtk_combo_box_new_text();
  int count = model->GetItemCount();
  for (int i = 0; i < count; ++i)
    gtk_combo_box_append_text(GTK_COMBO_BOX(combobox),
                              UTF16ToUTF8(model->GetItemAt(i)).c_str());
  return combobox;
}

////////////////////////////////////////////////////////////////////////////////
// AddLanguageDialog

class AddLanguageDialog {
 public:
  AddLanguageDialog(Profile* profile, LanguagesPageGtk* delegate);
  virtual ~AddLanguageDialog() {}

 private:
  // Callback for dialog buttons.
  CHROMEGTK_CALLBACK_1(AddLanguageDialog, void, OnResponse, int);

  // Callback for window destruction.
  CHROMEGTK_CALLBACK_0(AddLanguageDialog, void, OnWindowDestroy);

  // The dialog window.
  GtkWidget* dialog_;

  // The language chooser combobox.
  GtkWidget* combobox_;
  scoped_ptr<LanguageComboboxModel> accept_language_combobox_model_;

  // Used for call back to LanguagePageGtk that language has been selected.
  LanguagesPageGtk* language_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AddLanguageDialog);
};

AddLanguageDialog::AddLanguageDialog(Profile* profile,
                                     LanguagesPageGtk* delegate)
    : language_delegate_(delegate) {
  GtkWindow* parent = GTK_WINDOW(
      gtk_widget_get_toplevel(delegate->get_page_widget()));

  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(
          IDS_FONT_LANGUAGE_SETTING_LANGUAGES_TAB_TITLE).c_str(),
      parent,
      static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      GTK_STOCK_CANCEL,
      GTK_RESPONSE_CANCEL,
      GTK_STOCK_ADD,
      GTK_RESPONSE_OK,
      NULL);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog_), GTK_RESPONSE_OK);
  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);

  const std::string app_locale = g_browser_process->GetApplicationLocale();
  std::vector<std::string> locale_codes;
  l10n_util::GetAcceptLanguagesForLocale(app_locale, &locale_codes);
  accept_language_combobox_model_.reset(
      new LanguageComboboxModel(profile, locale_codes));
  combobox_ = NewComboboxFromModel(accept_language_combobox_model_.get());
  gtk_combo_box_set_active(GTK_COMBO_BOX(combobox_), 0);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog_)->vbox), combobox_);

  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponseThunk), this);
  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnWindowDestroyThunk), this);

  gtk_util::ShowDialog(dialog_);
}

void AddLanguageDialog::OnResponse(GtkWidget* dialog, int response_id) {
  if (response_id == GTK_RESPONSE_OK) {
    int selected = gtk_combo_box_get_active(GTK_COMBO_BOX(combobox_));
    language_delegate_->OnAddLanguage(
        accept_language_combobox_model_->GetLocaleFromIndex(selected));
  }
  gtk_widget_destroy(dialog_);
}

void AddLanguageDialog::OnWindowDestroy(GtkWidget* widget) {
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// LanguagesPageGtk

LanguagesPageGtk::LanguagesPageGtk(Profile* profile)
    : OptionsPageBase(profile),
      enable_autospellcorrect_checkbox_(NULL),
      initializing_(true) {
  Init();
}

LanguagesPageGtk::~LanguagesPageGtk() {
}

void LanguagesPageGtk::Init() {
  page_ = gtk_vbox_new(FALSE, gtk_util::kContentAreaSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(page_),
                                 gtk_util::kContentAreaBorder);

  // Languages order controls.
  GtkWidget* languages_vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(page_), languages_vbox,
                     TRUE, TRUE, 0);

  GtkWidget* languages_instructions_label = gtk_label_new(
      l10n_util::GetStringUTF8(
          IDS_FONT_LANGUAGE_SETTING_LANGUAGES_INSTRUCTIONS).c_str());
  gtk_misc_set_alignment(GTK_MISC(languages_instructions_label), 0, .5);
  gtk_util::SetLabelWidth(languages_instructions_label, kWrapWidth);
  gtk_box_pack_start(GTK_BOX(languages_vbox), languages_instructions_label,
                     FALSE, FALSE, 0);

  GtkWidget* languages_list_hbox = gtk_hbox_new(FALSE,
                                                gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(languages_vbox), languages_list_hbox,
                     TRUE, TRUE, 0);

  // Languages order tree.
  language_order_store_ = gtk_list_store_new(COL_COUNT,
                                             G_TYPE_STRING);
  language_order_tree_ = gtk_tree_view_new_with_model(
      GTK_TREE_MODEL(language_order_store_));
  g_object_unref(language_order_store_);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(language_order_tree_), FALSE);
  GtkTreeViewColumn* lang_column = gtk_tree_view_column_new_with_attributes(
      "",
      gtk_cell_renderer_text_new(),
      "text", COL_LANG,
      NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(language_order_tree_), lang_column);
  language_order_selection_ = gtk_tree_view_get_selection(
      GTK_TREE_VIEW(language_order_tree_));
  gtk_tree_selection_set_mode(language_order_selection_,
                              GTK_SELECTION_MULTIPLE);
  g_signal_connect(language_order_selection_, "changed",
                   G_CALLBACK(OnSelectionChangedThunk), this);
  GtkWidget* scroll_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll_window),
                                      GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(scroll_window), language_order_tree_);
  gtk_box_pack_start(GTK_BOX(languages_list_hbox), scroll_window,
                     TRUE, TRUE, 0);

  language_order_table_model_.reset(new LanguageOrderTableModel);
  language_order_table_adapter_.reset(
      new gtk_tree::TableAdapter(this, language_order_store_,
                                 language_order_table_model_.get()));

  // Languages order buttons.
  GtkWidget* languages_buttons_vbox = gtk_vbox_new(FALSE,
                                                   gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(languages_list_hbox), languages_buttons_vbox,
                     FALSE, FALSE, 0);

  add_button_ = gtk_button_new_with_label(l10n_util::GetStringUTF8(
      IDS_FONT_LANGUAGE_SETTING_LANGUAGES_SELECTOR_ADD_BUTTON_LABEL).c_str());
  g_signal_connect(add_button_, "clicked",
                   G_CALLBACK(OnAddButtonClickedThunk), this);
  gtk_box_pack_start(GTK_BOX(languages_buttons_vbox), add_button_,
                     FALSE, FALSE, 0);

  std::string remove_button_text  = l10n_util::GetStringUTF8(
      IDS_FONT_LANGUAGE_SETTING_LANGUAGES_SELECTOR_REMOVE_BUTTON_LABEL);
  remove_button_ = gtk_button_new_with_label(remove_button_text.c_str());
  g_signal_connect(remove_button_, "clicked",
                   G_CALLBACK(OnRemoveButtonClickedThunk), this);
  gtk_box_pack_start(GTK_BOX(languages_buttons_vbox), remove_button_,
                     FALSE, FALSE, 0);

  std::string move_up_button_text  = l10n_util::GetStringUTF8(
      IDS_FONT_LANGUAGE_SETTING_LANGUAGES_SELECTOR_MOVEUP_BUTTON_LABEL);
  move_up_button_ = gtk_button_new_with_label(move_up_button_text.c_str());
  g_signal_connect(move_up_button_, "clicked",
                   G_CALLBACK(OnMoveUpButtonClickedThunk), this);
  gtk_box_pack_start(GTK_BOX(languages_buttons_vbox), move_up_button_,
                     FALSE, FALSE, 0);

  std::string move_down_button_text  = l10n_util::GetStringUTF8(
      IDS_FONT_LANGUAGE_SETTING_LANGUAGES_SELECTOR_MOVEDOWN_BUTTON_LABEL);
  move_down_button_ = gtk_button_new_with_label(move_down_button_text.c_str());
  g_signal_connect(move_down_button_, "clicked",
                   G_CALLBACK(OnMoveDownButtonClickedThunk), this);
  gtk_box_pack_start(GTK_BOX(languages_buttons_vbox), move_down_button_,
                     FALSE, FALSE, 0);

  // Spell checker controls.
  GtkWidget* spellchecker_vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(page_), spellchecker_vbox,
                     FALSE, FALSE, 0);

  enable_spellchecking_checkbox_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_OPTIONS_ENABLE_SPELLCHECK).c_str());
  g_signal_connect(enable_spellchecking_checkbox_, "toggled",
                   G_CALLBACK(OnEnableSpellCheckingToggledThunk), this);
  gtk_box_pack_start(GTK_BOX(spellchecker_vbox), enable_spellchecking_checkbox_,
                     FALSE, FALSE, 0);

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kExperimentalSpellcheckerFeatures)) {
    enable_autospellcorrect_checkbox_ = gtk_check_button_new_with_label(
        l10n_util::GetStringUTF8(
            IDS_OPTIONS_ENABLE_AUTO_SPELL_CORRECTION).c_str());
    g_signal_connect(enable_autospellcorrect_checkbox_, "toggled",
                     G_CALLBACK(OnEnableAutoSpellCheckingToggledThunk), this);
    gtk_box_pack_start(GTK_BOX(spellchecker_vbox),
                       enable_autospellcorrect_checkbox_, FALSE, FALSE, 0);
  }

  std::vector<std::string> spell_check_languages;
  SpellCheckCommon::SpellCheckLanguages(&spell_check_languages);
  dictionary_language_model_.reset(new LanguageComboboxModel(profile(),
      spell_check_languages));
  dictionary_language_combobox_ = NewComboboxFromModel(
      dictionary_language_model_.get());
  g_signal_connect(dictionary_language_combobox_, "changed",
                   G_CALLBACK(OnDictionaryLanguageChangedThunk), this);
  GtkWidget* dictionary_language_control =
      gtk_util::CreateLabeledControlsGroup(NULL,
          l10n_util::GetStringUTF8(
              IDS_OPTIONS_CHROME_DICTIONARY_LANGUAGE).c_str(),
          dictionary_language_combobox_,
          NULL);
  gtk_box_pack_start(GTK_BOX(spellchecker_vbox), dictionary_language_control,
                     FALSE, FALSE, 0);

  // Initialize.
  accept_languages_.Init(prefs::kAcceptLanguages,
                         profile()->GetPrefs(), this);
  dictionary_language_.Init(prefs::kSpellCheckDictionary,
                            profile()->GetPrefs(), this);
  enable_spellcheck_.Init(prefs::kEnableSpellCheck,
                          profile()->GetPrefs(), this);
  enable_autospellcorrect_.Init(prefs::kEnableAutoSpellCorrect,
                                profile()->GetPrefs(), this);
  NotifyPrefChanged(NULL);
  EnableControls();
}

void LanguagesPageGtk::SetColumnValues(int row, GtkTreeIter* iter) {
  string16 lang = language_order_table_model_->GetText(row, 0);
  gtk_list_store_set(language_order_store_, iter,
                     COL_LANG, UTF16ToUTF8(lang).c_str(),
                     -1);
}

void LanguagesPageGtk::OnAnyModelUpdate() {
  if (!initializing_)
    accept_languages_.SetValue(language_order_table_model_->GetLanguageList());
  EnableControls();
}

void LanguagesPageGtk::EnableControls() {
  int num_selected = gtk_tree_selection_count_selected_rows(
      language_order_selection_);
  int row_count = gtk_tree_model_iter_n_children(
      GTK_TREE_MODEL(language_order_store_), NULL);
  gtk_widget_set_sensitive(move_up_button_,
                           num_selected == 1 && FirstSelectedRowNum() > 0);
  gtk_widget_set_sensitive(move_down_button_,
                           num_selected == 1 &&
                           FirstSelectedRowNum() < row_count - 1);
  gtk_widget_set_sensitive(remove_button_, num_selected > 0);
}

int LanguagesPageGtk::FirstSelectedRowNum() {
  int row_num = -1;
  GList* list = gtk_tree_selection_get_selected_rows(language_order_selection_,
                                                     NULL);
  if (list) {
    row_num = gtk_tree::GetRowNumForPath(static_cast<GtkTreePath*>(list->data));
    g_list_foreach(list, (GFunc)gtk_tree_path_free, NULL);
    g_list_free(list);
  }
  return row_num;
}

void LanguagesPageGtk::NotifyPrefChanged(const std::string* pref_name) {
  initializing_ = true;
  if (!pref_name || *pref_name == prefs::kAcceptLanguages) {
    language_order_table_model_->SetAcceptLanguagesString(
        accept_languages_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kSpellCheckDictionary) {
    int index = dictionary_language_model_->GetSelectedLanguageIndex(
        prefs::kSpellCheckDictionary);

    // If not found, fall back from "language-region" to "language".
    if (index < 0) {
      const std::string& lang_region = dictionary_language_.GetValue();
      dictionary_language_.SetValue(
          SpellCheckCommon::GetLanguageFromLanguageRegion(lang_region));
      index = dictionary_language_model_->GetSelectedLanguageIndex(
          prefs::kSpellCheckDictionary);
    }

    gtk_combo_box_set_active(GTK_COMBO_BOX(dictionary_language_combobox_),
                             index);
  }
  if (!pref_name || *pref_name == prefs::kEnableSpellCheck) {
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(enable_spellchecking_checkbox_),
        enable_spellcheck_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kEnableAutoSpellCorrect) {
    if (enable_autospellcorrect_checkbox_) {
      gtk_toggle_button_set_active(
          GTK_TOGGLE_BUTTON(enable_autospellcorrect_checkbox_),
          enable_autospellcorrect_.GetValue());
    }
  }
  initializing_ = false;
}

void LanguagesPageGtk::OnAddLanguage(const std::string& new_language) {
  if (language_order_table_model_->Add(new_language))
    gtk_tree::SelectAndFocusRowNum(language_order_table_model_->RowCount() - 1,
                                   GTK_TREE_VIEW(language_order_tree_));
}

void LanguagesPageGtk::OnSelectionChanged(GtkTreeSelection* selection) {
  EnableControls();
}

void LanguagesPageGtk::OnAddButtonClicked(GtkWidget* button) {
  new AddLanguageDialog(profile(), this);
}

void LanguagesPageGtk::OnRemoveButtonClicked(GtkWidget* button) {
  std::set<int> selected_rows;
  gtk_tree::GetSelectedIndices(language_order_selection_,
                               &selected_rows);

  int selected_row = 0;
  for (std::set<int>::reverse_iterator selected = selected_rows.rbegin();
       selected != selected_rows.rend(); ++selected) {
    language_order_table_model_->Remove(*selected);
    selected_row = *selected;
  }
  int row_count = language_order_table_model_->RowCount();
  if (row_count <= 0)
    return;
  if (selected_row >= row_count)
    selected_row = row_count - 1;
  gtk_tree::SelectAndFocusRowNum(selected_row,
      GTK_TREE_VIEW(language_order_tree_));
}

void LanguagesPageGtk::OnMoveUpButtonClicked(GtkWidget* button) {
  int item_selected = FirstSelectedRowNum();
  language_order_table_model_->MoveUp(item_selected);
  gtk_tree::SelectAndFocusRowNum(
      item_selected - 1, GTK_TREE_VIEW(language_order_tree_));
}

void LanguagesPageGtk::OnMoveDownButtonClicked(GtkWidget* button) {
  int item_selected = FirstSelectedRowNum();
  language_order_table_model_->MoveDown(item_selected);
  gtk_tree::SelectAndFocusRowNum(
      item_selected + 1, GTK_TREE_VIEW(language_order_tree_));
}

void LanguagesPageGtk::OnEnableSpellCheckingToggled(GtkWidget* toggle_button) {
  if (initializing_)
    return;
  enable_spellcheck_.SetValue(
      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_button)));
}

void LanguagesPageGtk::OnEnableAutoSpellCheckingToggled(
    GtkWidget* toggle_button) {
  if (initializing_)
    return;
  enable_autospellcorrect_.SetValue(
      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_button)));
}

void LanguagesPageGtk::OnDictionaryLanguageChanged(GtkWidget* widget) {
  if (initializing_)
    return;
  int new_index = gtk_combo_box_get_active(
      GTK_COMBO_BOX(dictionary_language_combobox_));

  if (new_index < 0 ||
      new_index >= dictionary_language_model_->GetItemCount()) {
    NOTREACHED();
    return;
  }

  // Remove the previously added spell check language to the accept list.
  if (!spellcheck_language_added_.empty()) {
    int old_index = language_order_table_model_->GetIndex(
        spellcheck_language_added_);
    if (old_index > -1)
      language_order_table_model_->Remove(old_index);
  }

  // Add this new spell check language only if it is not already in the
  // accept language list.
  std::string language =
      dictionary_language_model_->GetLocaleFromIndex(new_index);
  int index = language_order_table_model_->GetIndex(language);
  if (index == -1) {
    // Add the new language.
    OnAddLanguage(language);
    spellcheck_language_added_ = language;
  } else {
    spellcheck_language_added_ = "";
  }

  UserMetricsRecordAction(UserMetricsAction("Options_DictionaryLanguage"),
                          profile()->GetPrefs());
  dictionary_language_.SetValue(language);
}
