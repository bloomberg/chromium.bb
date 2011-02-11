// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <shlobj.h>
#include <vsstyle.h>
#include <vssym32.h>

#include "chrome/browser/ui/views/options/languages_page_view.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/language_combobox_model.h"
#include "chrome/browser/language_order_table_model.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/shell_dialogs.h"
#include "chrome/browser/ui/views/restart_message_box.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/spellcheck_common.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/native_theme_win.h"
#include "unicode/uloc.h"
#include "views/controls/button/radio_button.h"
#include "views/controls/tabbed_pane/tabbed_pane.h"
#include "views/controls/table/table_view.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

///////////////////////////////////////////////////////////////////////////////
// AddLanguageWindowView
//
// This opens another window from where a new accept language can be selected.
//
class AddLanguageWindowView : public views::View,
                              public views::Combobox::Listener,
                              public views::DialogDelegate {
 public:
  AddLanguageWindowView(LanguagesPageView* language_delegate, Profile* profile);
  views::Window* container() const { return container_; }
  void set_container(views::Window* container) {
    container_ = container;
  }

  // views::DialogDelegate methods.
  virtual bool Accept();
  virtual std::wstring GetWindowTitle() const;

  // views::WindowDelegate method.
  virtual bool IsModal() const { return true; }
  virtual views::View* GetContentsView() { return this; }

  // views::Combobox::Listener implementation:
  virtual void ItemChanged(views::Combobox* combobox,
                           int prev_index,
                           int new_index);

  // views::View overrides.
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

 protected:
  virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                    views::View* child);

 private:
  void Init();

  // The Options dialog window.
  views::Window* container_;

  // Used for Call back to LanguagePageView that language has been selected.
  LanguagesPageView* language_delegate_;
  std::string accept_language_selected_;

  // Combobox and its corresponding model.
  scoped_ptr<LanguageComboboxModel> accept_language_combobox_model_;
  views::Combobox* accept_language_combobox_;

  // The Profile associated with this window.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(AddLanguageWindowView);
};

static const int kDialogPadding = 7;
static int kDefaultWindowWidthChars = 60;
static int kDefaultWindowHeightLines = 3;

AddLanguageWindowView::AddLanguageWindowView(
    LanguagesPageView* language_delegate,
    Profile* profile)
    : profile_(profile->GetOriginalProfile()),
      language_delegate_(language_delegate),
      accept_language_combobox_(NULL) {
  Init();

  // Initialize accept_language_selected_ to the first index in drop down.
  accept_language_selected_ = accept_language_combobox_model_->
      GetLocaleFromIndex(0);
}

std::wstring AddLanguageWindowView::GetWindowTitle() const {
  return UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_FONT_LANGUAGE_SETTING_LANGUAGES_TAB_TITLE));
}

bool AddLanguageWindowView::Accept() {
  if (language_delegate_) {
    language_delegate_->OnAddLanguage(accept_language_selected_);
  }
  return true;
}

void AddLanguageWindowView::ItemChanged(views::Combobox* combobox,
                                  int prev_index,
                                  int new_index) {
  accept_language_selected_ = accept_language_combobox_model_->
      GetLocaleFromIndex(new_index);
}

void AddLanguageWindowView::Layout() {
  gfx::Size sz = accept_language_combobox_->GetPreferredSize();
  accept_language_combobox_->SetBounds(kDialogPadding, kDialogPadding,
                                       width() - 2*kDialogPadding,
                                       sz.height());
}

gfx::Size AddLanguageWindowView::GetPreferredSize() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const gfx::Font& font = rb.GetFont(ResourceBundle::BaseFont);
  return gfx::Size(font.GetAverageCharacterWidth() * kDefaultWindowWidthChars,
                   font.GetHeight() * kDefaultWindowHeightLines);
}

void AddLanguageWindowView::ViewHierarchyChanged(bool is_add,
                                                 views::View* parent,
                                                 views::View* child) {
  // Can't init before we're inserted into a Widget, because we require
  // a HWND to parent native child controls to.
  if (is_add && child == this)
    Init();
}

void AddLanguageWindowView::Init() {
  // Determine Locale Codes.
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  std::vector<std::string> locale_codes;
  l10n_util::GetAcceptLanguagesForLocale(app_locale, &locale_codes);

  accept_language_combobox_model_.reset(new LanguageComboboxModel(
    profile_, locale_codes));
  accept_language_combobox_ = new views::Combobox(
      accept_language_combobox_model_.get());
  accept_language_combobox_->SetSelectedItem(0);
  accept_language_combobox_->set_listener(this);
  AddChildView(accept_language_combobox_);
}

LanguagesPageView::LanguagesPageView(Profile* profile)
    : languages_instructions_(NULL),
      languages_contents_(NULL),
      language_order_table_(NULL),
      add_button_(NULL),
      remove_button_(NULL),
      move_up_button_(NULL),
      move_down_button_(NULL),
      button_stack_(NULL),
      language_info_label_(NULL),
      ui_language_label_(NULL),
      change_ui_language_combobox_(NULL),
      change_dictionary_language_combobox_(NULL),
      enable_spellchecking_checkbox_(NULL),
      enable_autospellcorrect_checkbox_(NULL),
      dictionary_language_label_(NULL),
      OptionsPageView(profile),
      language_table_edited_(false),
      language_warning_shown_(false),
      enable_spellcheck_checkbox_clicked_(false),
      enable_autospellcorrect_checkbox_clicked_(false),
      spellcheck_language_index_selected_(-1),
      ui_language_index_selected_(-1),
      starting_ui_language_index_(-1) {
  accept_languages_.Init(prefs::kAcceptLanguages,
      profile->GetPrefs(), NULL);
  enable_spellcheck_.Init(prefs::kEnableSpellCheck,
      profile->GetPrefs(), NULL);
  enable_autospellcorrect_.Init(prefs::kEnableAutoSpellCorrect,
      profile->GetPrefs(), NULL);
}

LanguagesPageView::~LanguagesPageView() {
  if (language_order_table_)
    language_order_table_->SetModel(NULL);
}

void LanguagesPageView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == move_up_button_) {
    OnMoveUpLanguage();
    language_table_edited_ = true;
  } else if (sender == move_down_button_) {
    OnMoveDownLanguage();
    language_table_edited_ = true;
  } else if (sender == remove_button_) {
    OnRemoveLanguage();
    language_table_edited_ = true;
  } else if (sender == add_button_) {
    views::Window::CreateChromeWindow(
        GetWindow()->GetNativeWindow(),
        gfx::Rect(),
        new AddLanguageWindowView(this, profile()))->Show();
    language_table_edited_ = true;
  } else if (sender == enable_spellchecking_checkbox_) {
    enable_spellcheck_checkbox_clicked_ = true;
  } else if (sender == enable_autospellcorrect_checkbox_) {
    enable_autospellcorrect_checkbox_clicked_ = true;
  }
}

void LanguagesPageView::OnAddLanguage(const std::string& new_language) {
  if (language_order_table_model_->Add(new_language)) {
    language_order_table_->Select(language_order_table_model_->RowCount() - 1);
    OnSelectionChanged();
  }
}

void LanguagesPageView::InitControlLayout() {
  // Define the buttons.
  add_button_ = new views::NativeButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_FONT_LANGUAGE_SETTING_LANGUAGES_SELECTOR_ADD_BUTTON_LABEL)));
  remove_button_ = new views::NativeButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_FONT_LANGUAGE_SETTING_LANGUAGES_SELECTOR_REMOVE_BUTTON_LABEL)));
  remove_button_->SetEnabled(false);
  move_up_button_ = new views::NativeButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_FONT_LANGUAGE_SETTING_LANGUAGES_SELECTOR_MOVEUP_BUTTON_LABEL)));
  move_up_button_->SetEnabled(false);
  move_down_button_ = new views::NativeButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_FONT_LANGUAGE_SETTING_LANGUAGES_SELECTOR_MOVEDOWN_BUTTON_LABEL)));
  move_down_button_->SetEnabled(false);

  languages_contents_ = new views::View;
  using views::GridLayout;
  using views::ColumnSet;

  GridLayout* layout = GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);

  // Add the instructions label.
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                      GridLayout::USE_PREF, 0, 0);
  languages_instructions_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_FONT_LANGUAGE_SETTING_LANGUAGES_INSTRUCTIONS)));
  languages_instructions_->SetMultiLine(true);
  languages_instructions_->SetHorizontalAlignment(
      views::Label::ALIGN_LEFT);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(languages_instructions_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Add two columns - for table, and for button stack.
  std::vector<TableColumn> columns;
  columns.push_back(TableColumn());
  language_order_table_model_.reset(new LanguageOrderTableModel);
  language_order_table_ = new views::TableView(
      language_order_table_model_.get(), columns,
      views::TEXT_ONLY, false, true, true);
  language_order_table_->SetObserver(this);

  const int double_column_view_set_id = 1;
  column_set = layout->AddColumnSet(double_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, double_column_view_set_id);

  // Add the table to the the first column.
  layout->AddView(language_order_table_);

  // Now add the four buttons to the second column.
  button_stack_ = new views::View;
  GridLayout* button_stack_layout = new GridLayout(button_stack_);
  button_stack_->SetLayoutManager(button_stack_layout);

  column_set = button_stack_layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  button_stack_layout->StartRow(0, single_column_view_set_id);
  button_stack_layout->AddView(add_button_, 1, 1, GridLayout::FILL,
                               GridLayout::CENTER);
  button_stack_layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  button_stack_layout->StartRow(0, single_column_view_set_id);
  button_stack_layout->AddView(remove_button_, 1, 1, GridLayout::FILL,
                               GridLayout::CENTER);
  button_stack_layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  button_stack_layout->StartRow(0, single_column_view_set_id);
  button_stack_layout->AddView(move_up_button_, 1, 1, GridLayout::FILL,
                               GridLayout::CENTER);
  button_stack_layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  button_stack_layout->StartRow(0, single_column_view_set_id);
  button_stack_layout->AddView(move_down_button_, 1, 1, GridLayout::FILL,
                               GridLayout::CENTER);

  layout->AddView(button_stack_);

  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  language_info_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_CHROME_LANGUAGE_INFO)));
  language_info_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  ui_language_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_CHROME_UI_LANGUAGE)));
  ui_language_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  ui_language_model_.reset(new LanguageComboboxModel);
  change_ui_language_combobox_ =
      new views::Combobox(ui_language_model_.get());
  change_ui_language_combobox_->set_listener(this);
  dictionary_language_label_ = new views::Label(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHROME_DICTIONARY_LANGUAGE)));
  dictionary_language_label_->SetHorizontalAlignment(
      views::Label::ALIGN_LEFT);
  enable_spellchecking_checkbox_ = new views::Checkbox(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_ENABLE_SPELLCHECK)));
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kExperimentalSpellcheckerFeatures)) {
    enable_autospellcorrect_checkbox_ = new views::Checkbox(UTF16ToWide(
        l10n_util::GetStringUTF16(IDS_OPTIONS_ENABLE_AUTO_SPELL_CORRECTION)));
    enable_autospellcorrect_checkbox_->set_listener(this);
  }
  enable_spellchecking_checkbox_->set_listener(this);
  enable_spellchecking_checkbox_->SetMultiLine(true);

  // Determine Locale Codes.
  std::vector<std::string> spell_check_languages;
  SpellCheckCommon::SpellCheckLanguages(&spell_check_languages);
  dictionary_language_model_.reset(new LanguageComboboxModel(profile(),
      spell_check_languages));
  change_dictionary_language_combobox_ =
      new views::Combobox(dictionary_language_model_.get());
  change_dictionary_language_combobox_->set_listener(this);

  // SpellCheck language settings.
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(enable_spellchecking_checkbox_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  if (command_line.HasSwitch(switches::kExperimentalSpellcheckerFeatures)) {
    layout->StartRow(0, single_column_view_set_id);
    layout->AddView(enable_autospellcorrect_checkbox_);
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  }
  const int double_column_view_set_2_id = 2;
  column_set = layout->AddColumnSet(double_column_view_set_2_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, double_column_view_set_2_id);
  layout->AddView(dictionary_language_label_);
  layout->AddView(change_dictionary_language_combobox_);

  // UI language settings.
  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(language_info_label_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, double_column_view_set_2_id);
  layout->AddView(ui_language_label_);
  layout->AddView(change_ui_language_combobox_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Init member prefs so we can update the controls if prefs change.
  app_locale_.Init(prefs::kApplicationLocale,
                   g_browser_process->local_state(), this);
  dictionary_language_.Init(prefs::kSpellCheckDictionary,
                            profile()->GetPrefs(), this);
}

void LanguagesPageView::NotifyPrefChanged(const std::string* pref_name) {
  if (!pref_name || *pref_name == prefs::kAcceptLanguages) {
    language_order_table_model_->SetAcceptLanguagesString(
        accept_languages_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kApplicationLocale) {
    int index = ui_language_model_->GetSelectedLanguageIndex(
        prefs::kApplicationLocale);
    if (-1 == index) {
      // The pref value for locale isn't valid.  Use the current app locale
      // (which is what we're currently using).
      index = ui_language_model_->GetIndexFromLocale(
          g_browser_process->GetApplicationLocale());
    }
    DCHECK(-1 != index);
    change_ui_language_combobox_->SetSelectedItem(index);
    starting_ui_language_index_ = index;
  }
  if (!pref_name || *pref_name == prefs::kSpellCheckDictionary) {
    int index = dictionary_language_model_->GetSelectedLanguageIndex(
        prefs::kSpellCheckDictionary);

    // If the index for the current language cannot be found, it is due to
    // the fact that the pref-member value for the last dictionary language
    // set by the user still uses the old format; i.e. language-region, even
    // when region is not necessary. For example, if the user sets the
    // dictionary language to be French, the pref-member value in the user
    // profile is "fr-FR", whereas we now use only "fr". To resolve this issue,
    // if "fr-FR" is read from the pref, the language code ("fr" here) is
    // extracted, and re-written in the pref, so that the pref-member value for
    // dictionary language in the user profile now correctly stores "fr"
    // instead of "fr-FR".
    if (index < 0) {
      const std::string& lang_region = dictionary_language_.GetValue();
      dictionary_language_.SetValue(
          SpellCheckCommon::GetLanguageFromLanguageRegion(lang_region));
      index = dictionary_language_model_->GetSelectedLanguageIndex(
          prefs::kSpellCheckDictionary);
    }

    change_dictionary_language_combobox_->SetSelectedItem(index);
    spellcheck_language_index_selected_ = -1;
  }
  if (!pref_name || *pref_name == prefs::kEnableSpellCheck) {
    enable_spellchecking_checkbox_->SetChecked(
        enable_spellcheck_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kEnableAutoSpellCorrect) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    if (command_line.HasSwitch(switches::kExperimentalSpellcheckerFeatures)) {
      enable_autospellcorrect_checkbox_->SetChecked(
          enable_autospellcorrect_.GetValue());
    }
  }
}

void LanguagesPageView::ItemChanged(views::Combobox* sender,
                                    int prev_index,
                                    int new_index) {
  if (prev_index == new_index)
    return;

  if (sender == change_ui_language_combobox_) {
    if (new_index == starting_ui_language_index_)
      ui_language_index_selected_ = -1;
    else
      ui_language_index_selected_ = new_index;

    if (!language_warning_shown_) {
      RestartMessageBox::ShowMessageBox(GetWindow()->GetNativeWindow());
      language_warning_shown_ = true;
    }
  } else if (sender == change_dictionary_language_combobox_) {
    // Set the spellcheck language selected.
    spellcheck_language_index_selected_ = new_index;

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
      language_table_edited_ = true;
      spellcheck_language_added_ = language;
    } else {
      spellcheck_language_added_ = "";
    }
  }
}

void LanguagesPageView::OnSelectionChanged() {
  move_up_button_->SetEnabled(language_order_table_->FirstSelectedRow() > 0 &&
                              language_order_table_->SelectedRowCount() == 1);
  move_down_button_->SetEnabled(language_order_table_->FirstSelectedRow() <
                                language_order_table_->RowCount() - 1 &&
                                language_order_table_->SelectedRowCount() ==
                                1);
  remove_button_->SetEnabled(language_order_table_->SelectedRowCount() > 0);
}

void LanguagesPageView::OnRemoveLanguage() {
  int item_selected = 0;
  for (views::TableView::iterator i =
       language_order_table_->SelectionBegin();
       i != language_order_table_->SelectionEnd(); ++i) {
    language_order_table_model_->Remove(*i);
    item_selected = *i;
  }

  move_up_button_->SetEnabled(false);
  move_down_button_->SetEnabled(false);
  remove_button_->SetEnabled(false);
  int items_left = language_order_table_model_->RowCount();
  if (items_left <= 0)
    return;
  if (item_selected > items_left - 1)
    item_selected = items_left - 1;
  language_order_table_->Select(item_selected);
  OnSelectionChanged();
}

void LanguagesPageView::OnMoveDownLanguage() {
  int item_selected = language_order_table_->FirstSelectedRow();
  language_order_table_model_->MoveDown(item_selected);
  language_order_table_->Select(item_selected + 1);
  OnSelectionChanged();
}

void LanguagesPageView::OnMoveUpLanguage() {
  int item_selected = language_order_table_->FirstSelectedRow();
  language_order_table_model_->MoveUp(item_selected);
  language_order_table_->Select(item_selected - 1);

  OnSelectionChanged();
}

void LanguagesPageView::SaveChanges() {
  if (language_order_table_model_.get() && language_table_edited_) {
    accept_languages_.SetValue(
        language_order_table_model_->GetLanguageList());
  }

  if (ui_language_index_selected_ != -1) {
    UserMetricsRecordAction(UserMetricsAction("Options_AppLanguage"),
                            g_browser_process->local_state());
    app_locale_.SetValue(ui_language_model_->
        GetLocaleFromIndex(ui_language_index_selected_));

    // Remove pref values for spellcheck dictionaries forcefully.
    PrefService* prefs = profile()->GetPrefs();
    if (prefs)
      prefs->ClearPref(prefs::kSpellCheckDictionary);
  }

  if (spellcheck_language_index_selected_ != -1) {
    UserMetricsRecordAction(UserMetricsAction("Options_DictionaryLanguage"),
                            profile()->GetPrefs());
    dictionary_language_.SetValue(dictionary_language_model_->
        GetLocaleFromIndex(spellcheck_language_index_selected_));
  }

  if (enable_spellcheck_checkbox_clicked_)
    enable_spellcheck_.SetValue(enable_spellchecking_checkbox_->checked());

  if (enable_autospellcorrect_checkbox_clicked_) {
    enable_autospellcorrect_.SetValue(
        enable_autospellcorrect_checkbox_->checked());
  }
}
