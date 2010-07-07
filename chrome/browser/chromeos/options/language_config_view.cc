// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/language_config_view.h"

#include <algorithm>

#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/options/language_chewing_config_view.h"
#include "chrome/browser/chromeos/options/language_hangul_config_view.h"
#include "chrome/browser/chromeos/options/language_mozc_config_view.h"
#include "chrome/browser/chromeos/options/language_pinyin_config_view.h"
#include "chrome/browser/chromeos/options/options_window_view.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/restart_message_box.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "gfx/font.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/label.h"
#include "views/fill_layout.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

namespace chromeos {
using views::ColumnSet;
using views::GridLayout;

namespace {

// The width of the preferred language table shown on the left side.
const int kPreferredLanguageTableWidth = 300;

// Creates the LanguageHangulConfigView. The function is used to create
// the object via a function pointer. See also InitInputMethodConfigViewMap().
views::DialogDelegate* CreateLanguageChewingConfigView(Profile* profile) {
  return new LanguageChewingConfigView(profile);
}
views::DialogDelegate* CreateLanguageHangulConfigView(Profile* profile) {
  return new LanguageHangulConfigView(profile);
}
views::DialogDelegate* CreateLanguagePinyinConfigView(Profile* profile) {
  return new LanguagePinyinConfigView(profile);
}
views::DialogDelegate* CreateLanguageMozcConfigView(Profile* profile) {
  return new LanguageMozcConfigView(profile);
}

// The tags are used to identify buttons in ButtonPressed().
enum ButtonTag {
  kChangeUiLanguageButton,
  kConfigureInputMethodButton,
  kRemoveLanguageButton,
  kSelectInputMethodButton,
};

// The column set IDs are used for creating the per-language config view.
const int kPerLanguageTitleColumnSetId = 1;
const int kPerLanguageDoubleColumnSetId = 2;
const int kPerLanguageSingleColumnSetId = 3;

}  // namespace

// This is a native button associated with input method information.
class InputMethodButton : public views::NativeButton {
 public:
  InputMethodButton(views::ButtonListener* listener,
                    const std::wstring& label,
                    const std::string& input_method_id)
      : views::NativeButton(listener, label),
        input_method_id_(input_method_id) {
  }

  const std::string& input_method_id() const {
    return input_method_id_;
  }

 private:
  std::string input_method_id_;
  DISALLOW_COPY_AND_ASSIGN(InputMethodButton);
};

// This is a native button associated with UI language information.
class UiLanguageButton : public views::NativeButton {
 public:
  UiLanguageButton(views::ButtonListener* listener,
                    const std::wstring& label,
                    const std::string& language_code)
      : views::NativeButton(listener, label),
        language_code_(language_code) {
  }

  const std::string& language_code() const {
    return language_code_;
  }

 private:
  std::string language_code_;
  DISALLOW_COPY_AND_ASSIGN(UiLanguageButton);
};

// This is a checkbox button associated with input method information.
class InputMethodCheckbox : public views::Checkbox {
 public:
  InputMethodCheckbox(const std::wstring& display_name,
                      const std::string& input_method_id)
      : views::Checkbox(display_name),
        input_method_id_(input_method_id) {
  }

  const std::string& input_method_id() const {
    return input_method_id_;
  }

 private:
  std::string input_method_id_;
  DISALLOW_COPY_AND_ASSIGN(InputMethodCheckbox);
};

LanguageConfigView::LanguageConfigView(Profile* profile)
    : OptionsPageView(profile),
      model_(profile->GetPrefs()),
      root_container_(NULL),
      right_container_(NULL),
      remove_language_button_(NULL),
      preferred_language_table_(NULL) {
}

LanguageConfigView::~LanguageConfigView() {
}

void LanguageConfigView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender->tag() == kRemoveLanguageButton) {
    OnRemoveLanguage();
  } else if (sender->tag() == kSelectInputMethodButton) {
    InputMethodCheckbox* checkbox =
        static_cast<InputMethodCheckbox*>(sender);
    const std::string& input_method_id = checkbox->input_method_id();
    model_.SetInputMethodActivated(input_method_id, checkbox->checked());
    if (checkbox->checked()) {
      EnableAllCheckboxes();
    } else {
      MaybeDisableLastCheckbox();
    }
  } else if (sender->tag() == kConfigureInputMethodButton) {
    InputMethodButton* button = static_cast<InputMethodButton*>(sender);
    views::DialogDelegate* config_view =
        CreateInputMethodConfigureView(button->input_method_id());
    if (!config_view) {
      DLOG(FATAL) << "Config view not found: " << button->input_method_id();
      return;
    }
    views::Window* window = views::Window::CreateChromeWindow(
        GetOptionsViewParent(), gfx::Rect(), config_view);
    window->SetIsAlwaysOnTop(true);
    window->Show();
  } else if (sender->tag() == kChangeUiLanguageButton) {
    UiLanguageButton* button = static_cast<UiLanguageButton*>(sender);
    PrefService* prefs = g_browser_process->local_state();
    if (prefs) {
      prefs->SetString(prefs::kApplicationLocale, button->language_code());
      prefs->SavePersistentPrefs();
      RestartMessageBox::ShowMessageBox(GetWindow()->GetNativeWindow());
    }
  }
}

std::wstring LanguageConfigView::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK) {
    return l10n_util::GetString(IDS_DONE);
  }
  return L"";
}

std::wstring LanguageConfigView::GetWindowTitle() const {
  return l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_LANGUAGES_DIALOG_TITLE);
}

void LanguageConfigView::Layout() {
  // Not sure why but this is needed to show contents in the dialog.
  root_container_->SetBounds(0, 0, width(), height());
}

gfx::Size LanguageConfigView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_LANGUAGES_INPUT_DIALOG_WIDTH_CHARS,
      IDS_LANGUAGES_INPUT_DIALOG_HEIGHT_LINES));
}

void LanguageConfigView::OnSelectionChanged() {
  right_container_->RemoveAllChildViews(true);  // Delete the child views.

  const int row = preferred_language_table_->GetFirstSelectedRow();
  const std::string& language_code = model_.preferred_language_code_at(row);

  // Count the number of all active input methods.
  std::vector<std::string> active_input_method_ids;
  model_.GetActiveInputMethodIds(&active_input_method_ids);
  const int num_all_active_input_methods = active_input_method_ids.size();

  // Count the number of active input methods for the selected language.
  int num_selected_active_input_methods =
      model_.CountNumActiveInputMethods(language_code);

  bool remove_button_enabled = false;
  // Allow removing the language only if the following conditions are met:
  // 1. There are more than one language.
  // 2. The languge in the current row is not set to the display language.
  // 3. Removing the selected language does not result in "zero input method".
  if (preferred_language_table_->GetRowCount() > 1 &&
      language_code != g_browser_process->GetApplicationLocale() &&
      num_all_active_input_methods > num_selected_active_input_methods) {
    remove_button_enabled = true;
  }
  remove_language_button_->SetEnabled(remove_button_enabled);

  // Add the per language config view to the right area.
  right_container_->AddChildView(CreatePerLanguageConfigView(language_code));
  MaybeDisableLastCheckbox();
  // Layout the right container. This is needed for the contents on the
  // right to be displayed properly.
  right_container_->Layout();
}

std::wstring LanguageConfigView::GetText(int row, int column_id) {
  if (row >= 0 && row < static_cast<int>(
          model_.num_preferred_language_codes())) {
    return input_method::GetLanguageDisplayNameFromCode(
        model_.preferred_language_code_at(row));
  }
  NOTREACHED();
  return L"";
}

void LanguageConfigView::SetObserver(TableModelObserver* observer) {
  // We don't need the observer for the table mode, since we implement the
  // table model as part of the LanguageConfigView class.
  // http://crbug.com/38266
}

int LanguageConfigView::RowCount() {
  // Returns the number of rows of the language table.
  return model_.num_preferred_language_codes();
}

void LanguageConfigView::ItemChanged(views::Combobox* combobox,
                                     int prev_index,
                                     int new_index) {
  // Ignore the first item used for showing "Add language".
  if (new_index <= 0) {
    return;
  }
  // Get the language selected.
  std::string language_selected = add_language_combobox_model_->
      GetLocaleFromIndex(
          add_language_combobox_model_->GetLanguageIndex(new_index));
  OnAddLanguage(language_selected);
}

void LanguageConfigView::InitControlLayout() {
  // Initialize the map.
  InitInputMethodConfigViewMap();

  root_container_ = new views::View;
  AddChildView(root_container_);

  // Set up the layout manager for the root container.  We'll place the
  // language table on the left, and the per language config on the right.
  GridLayout* root_layout = new GridLayout(root_container_);
  root_container_->SetLayoutManager(root_layout);
  root_layout->SetInsets(kPanelVertMargin, kPanelHorizMargin,
                         kPanelVertMargin, kPanelHorizMargin);

  // Set up column sets for the grid layout.
  const int kMainColumnSetId = 0;
  ColumnSet* column_set = root_layout->AddColumnSet(kMainColumnSetId);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                        GridLayout::FIXED, kPreferredLanguageTableWidth, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1.0,
                        GridLayout::USE_PREF, 0, 0);
  const int kBottomColumnSetId = 1;
  column_set = root_layout->AddColumnSet(kBottomColumnSetId);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  // Set up the container for the contents on the right.  Just adds a
  // place holder here. This will get replaced in OnSelectionChanged().
  right_container_ = new views::View;
  right_container_->SetLayoutManager(new views::FillLayout);
  right_container_->AddChildView(new views::View);

  // Add the contents on the left and the right.
  root_layout->StartRow(1 /* expand */, kMainColumnSetId);
  root_layout->AddView(CreateContentsOnLeft());
  root_layout->AddView(right_container_);

  // Add the contents on the bottom.
  root_layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  root_layout->StartRow(0, kBottomColumnSetId);
  root_layout->AddView(CreateContentsOnBottom());

  // Select the first row in the language table.
  // There should be at least one language in the table, but we check it
  // here so this won't result in crash in case there is no row in the table.
  if (model_.num_preferred_language_codes() > 0) {
    preferred_language_table_->SelectRow(0);
  }
}

void LanguageConfigView::Show(Profile* profile, gfx::NativeWindow parent) {
  UserMetrics::RecordAction(UserMetricsAction("LanguageConfigView_Open"));
  views::Window* window = views::Window::CreateChromeWindow(
      parent, gfx::Rect(), new LanguageConfigView(profile));
  window->SetIsAlwaysOnTop(true);
  window->Show();
}

void LanguageConfigView::InitInputMethodConfigViewMap() {
  input_method_config_view_map_["chewing"] = CreateLanguageChewingConfigView;
  input_method_config_view_map_["hangul"] = CreateLanguageHangulConfigView;
  input_method_config_view_map_["mozc"] = CreateLanguageMozcConfigView;
  input_method_config_view_map_["mozc-dv"] = CreateLanguageMozcConfigView;
  input_method_config_view_map_["mozc-jp"] = CreateLanguageMozcConfigView;
  input_method_config_view_map_["pinyin"] = CreateLanguagePinyinConfigView;
}

void LanguageConfigView::OnAddLanguage(const std::string& language_code) {
  // Skip if the language is already in the preferred_language_codes_.
  if (model_.HasLanguageCode(language_code)) {
    return;
  }
  // Activate the first input language associated with the language. We have
  // to call this before the OnItemsAdded() call below so the checkbox
  // for the first input language gets checked.
  std::vector<std::string> input_method_ids;
  model_.GetInputMethodIdsFromLanguageCode(language_code, &input_method_ids);
  if (!input_method_ids.empty()) {
    model_.SetInputMethodActivated(input_method_ids[0], true);
  }

  // Append the language to the list of language codes.
  const int added_at = model_.AddLanguageCode(language_code);
  // Notify the table that the new row added at |added_at|.
  preferred_language_table_->OnItemsAdded(added_at, 1);
  // For some reason, OnItemsAdded() alone does not redraw the table. Need
  // to tell the table that items are changed. TODO(satorux): Investigate
  // if it's a bug in TableView2.
  preferred_language_table_->OnItemsChanged(
      0, model_.num_preferred_language_codes());
  // Switch to the row added.
  preferred_language_table_->SelectRow(added_at);

  // Mark the language to be ignored.
  add_language_combobox_model_->SetIgnored(language_code, true);
  ResetAddLanguageCombobox();
}

void LanguageConfigView::OnRemoveLanguage() {
  const int row = preferred_language_table_->GetFirstSelectedRow();
  const std::string& language_code = model_.preferred_language_code_at(row);
  // Mark the language not to be ignored.
  add_language_combobox_model_->SetIgnored(language_code, false);
  ResetAddLanguageCombobox();
  // Deactivate the associated input methods.
  model_.DeactivateInputMethodsFor(language_code);
  // Remove the language code and the row from the table.
  model_.RemoveLanguageAt(row);
  preferred_language_table_->OnItemsRemoved(row, 1);
  // Switch to the previous row, or the first row.
  // There should be at least one row in the table.
  preferred_language_table_->SelectRow(std::max(row - 1, 0));
}

void LanguageConfigView::ResetAddLanguageCombobox() {
  // -1 to ignore "Add language". If there are more than one language,
  // enable the combobox. Otherwise, disable it.
  if (add_language_combobox_model_->GetItemCount() - 1 > 0) {
    add_language_combobox_->SetEnabled(true);
  } else {
    add_language_combobox_->SetEnabled(false);
  }
  // Go back to the initial "Add language" state.
  add_language_combobox_->ModelChanged();
  add_language_combobox_->SetSelectedItem(0);
}

views::View* LanguageConfigView::CreateContentsOnLeft() {
  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  // Set up column sets for the grid layout.
  const int kTableColumnSetId = 0;
  ColumnSet* column_set = layout->AddColumnSet(kTableColumnSetId);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  // Create the language table.
  std::vector<TableColumn> columns;
  TableColumn column(0,
                     l10n_util::GetString(
                         IDS_OPTIONS_SETTINGS_LANGUAGES_LANGUAGES),
                     TableColumn::LEFT, -1, 0);
  columns.push_back(column);
  // We don't show horizontal and vertical lines.
  const int options = (views::TableView2::SINGLE_SELECTION |
                       views::TableView2::RESIZABLE_COLUMNS |
                       views::TableView2::AUTOSIZE_COLUMNS);
  preferred_language_table_ =
      new views::TableView2(this, columns, views::TEXT_ONLY, options);
  // Set the observer so OnSelectionChanged() will be invoked when a
  // selection is changed in the table.
  preferred_language_table_->SetObserver(this);

  // Add the language table.
  layout->StartRow(1 /* expand vertically */, kTableColumnSetId);
  layout->AddView(preferred_language_table_);

  return contents;
}

views::View* LanguageConfigView::CreateContentsOnBottom() {
  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  // Set up column sets for the grid layout.
  const int kButtonsColumnSetId = 0;
  ColumnSet* column_set = layout->AddColumnSet(kButtonsColumnSetId);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 0,
                        GridLayout::USE_PREF, 0, 0);

  // Create the add language combobox model_.
  // LanguageComboboxModel sorts languages by their display names.
  add_language_combobox_model_.reset(
      new AddLanguageComboboxModel(NULL, model_.supported_language_codes()));
  // Mark the existing preferred languages to be ignored.
  for (size_t i = 0; i < model_.num_preferred_language_codes(); ++i) {
    add_language_combobox_model_->SetIgnored(
        model_.preferred_language_code_at(i),
        true);
  }
  // Create the add language combobox.
  add_language_combobox_
      = new views::Combobox(add_language_combobox_model_.get());
  add_language_combobox_->set_listener(this);
  ResetAddLanguageCombobox();

  // Create the remove button.
  remove_language_button_ = new views::NativeButton(
      this, l10n_util::GetString(
          IDS_OPTIONS_SETTINGS_LANGUAGES_REMOVE_BUTTON));
  remove_language_button_->set_tag(kRemoveLanguageButton);

  // Add the add and remove buttons.
  layout->StartRow(0, kButtonsColumnSetId);
  layout->AddView(add_language_combobox_);
  layout->AddView(remove_language_button_);

  return contents;
}

views::View* LanguageConfigView::CreatePerLanguageConfigView(
    const std::string& target_language_code) {
  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  // Set up column sets for the grid layout.
  ColumnSet* column_set = layout->AddColumnSet(kPerLanguageTitleColumnSetId);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  column_set = layout->AddColumnSet(kPerLanguageDoubleColumnSetId);
  column_set->AddPaddingColumn(0, kUnrelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  column_set = layout->AddColumnSet(kPerLanguageSingleColumnSetId);
  column_set->AddPaddingColumn(0, kUnrelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  AddUiLanguageSection(target_language_code, layout);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);
  AddInputMethodSection(target_language_code, layout);

  return contents;
}

void LanguageConfigView::AddUiLanguageSection(const std::string& language_code,
                                              views::GridLayout* layout) {
  // Create the language name label.
  const std::string application_locale =
      g_browser_process->GetApplicationLocale();
  const string16 language_name16 = l10n_util::GetDisplayNameForLocale(
      language_code, application_locale, true);
  const std::wstring language_name
      = input_method::MaybeRewriteLanguageName(UTF16ToWide(language_name16));
  views::Label* language_name_label = new views::Label(language_name);
  language_name_label->SetFont(
      language_name_label->font().DeriveFont(0, gfx::Font::BOLD));

  // Add the language name label.
  layout->StartRow(0, kPerLanguageTitleColumnSetId);
  layout->AddView(language_name_label);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, kPerLanguageSingleColumnSetId);
  if (application_locale == language_code) {
    layout->AddView(
        new views::Label(
            l10n_util::GetStringF(
                IDS_OPTIONS_SETTINGS_LANGUAGES_IS_DISPLAYED_IN_THIS_LANGUAGE,
                l10n_util::GetString(IDS_PRODUCT_OS_NAME))));
  } else {
    UiLanguageButton* button = new UiLanguageButton(
      this, l10n_util::GetStringF(
          IDS_OPTIONS_SETTINGS_LANGUAGES_DISPLAY_IN_THIS_LANGUAGE,
          l10n_util::GetString(IDS_PRODUCT_OS_NAME)),
      language_code);
    button->set_tag(kChangeUiLanguageButton);
    layout->AddView(button);
  }
}

void LanguageConfigView::AddInputMethodSection(
    const std::string& language_code,
    views::GridLayout* layout) {
  // Create the input method title label.
  views::Label* input_method_title_label = new views::Label(
      l10n_util::GetString(
          IDS_OPTIONS_SETTINGS_LANGUAGES_INPUT_METHOD));
  input_method_title_label->SetFont(
      input_method_title_label->font().DeriveFont(0, gfx::Font::BOLD));

  // Add the input method title label.
  layout->StartRow(0, kPerLanguageTitleColumnSetId);
  layout->AddView(input_method_title_label);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Add input method names and configuration buttons.
  input_method_checkboxes_.clear();

  // Get the list of input method ids associated with the language code.
  std::vector<std::string> input_method_ids;
  model_.GetInputMethodIdsFromLanguageCode(language_code, &input_method_ids);

  for (size_t i = 0; i < input_method_ids.size(); ++i) {
    const std::string& input_method_id = input_method_ids[i];
    const std::string display_name =
        input_method::GetInputMethodDisplayNameFromId(input_method_id);
    layout->StartRow(0, kPerLanguageDoubleColumnSetId);
    InputMethodCheckbox* checkbox
        = new InputMethodCheckbox(UTF8ToWide(display_name),
                                  input_method_id);
    checkbox->set_listener(this);
    checkbox->set_tag(kSelectInputMethodButton);
    if (model_.InputMethodIsActivated(input_method_id)) {
      checkbox->SetChecked(true);
    }

    layout->AddView(checkbox);
    input_method_checkboxes_.insert(checkbox);
    // Add "configure" button for the input method if we have a
    // configuration dialog for it.
    if (input_method_config_view_map_.count(input_method_id) > 0) {
      InputMethodButton* button = new InputMethodButton(
          this,
          l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_CONFIGURE),
          input_method_id);
      button->set_tag(kConfigureInputMethodButton);
      layout->AddView(button);
    }
  }
}

views::DialogDelegate* LanguageConfigView::CreateInputMethodConfigureView(
    const std::string& input_method_id) {
  InputMethodConfigViewMap::const_iterator iter =
      input_method_config_view_map_.find(input_method_id);
  if (iter != input_method_config_view_map_.end()) {
    CreateDialogDelegateFunction function = iter->second;
    return function(profile());
  }
  return NULL;
}

void LanguageConfigView::MaybeDisableLastCheckbox() {
  std::vector<std::string> input_method_ids;
  model_.GetActiveInputMethodIds(&input_method_ids);
  if (input_method_ids.size() <= 1) {
    for (std::set<InputMethodCheckbox*>::iterator checkbox =
             input_method_checkboxes_.begin();
         checkbox != input_method_checkboxes_.end(); ++checkbox) {
      if ((*checkbox)->checked())
        (*checkbox)->SetEnabled(false);
    }
  }
}

void LanguageConfigView::EnableAllCheckboxes() {
  for (std::set<InputMethodCheckbox*>::iterator checkbox =
           input_method_checkboxes_.begin();
       checkbox != input_method_checkboxes_.end(); ++checkbox) {
    (*checkbox)->SetEnabled(true);
  }
}

}  // namespace chromeos
