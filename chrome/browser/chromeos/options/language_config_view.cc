// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/language_config_view.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/language_library.h"
#include "chrome/browser/chromeos/options/language_hangul_config_view.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/language_combobox_model.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "gfx/font.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/controls/button/radio_button.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/label.h"
#include "views/fill_layout.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

namespace chromeos {
using views::ColumnSet;
using views::GridLayout;

namespace {

// Creates the LanguageHangulConfigView. The function is used to create
// the object via a function pointer. See also InitInputMethodConfigViewMap().
views::DialogDelegate* CreateLanguageHangulConfigView(Profile* profile) {
  return new LanguageHangulConfigView(profile);
}

}  // namespace

// This is a LanguageComboboxModel that can handle the special language
// code used for input methods that don't fall under any other languages.
class LanguageComboboxModelWithOthers : public LanguageComboboxModel {
 public:
  LanguageComboboxModelWithOthers(Profile* profile,
                                  const std::vector<std::string>& locale_codes)
      : LanguageComboboxModel(profile, locale_codes) {
  }

  virtual std::wstring GetItemAt(int index) {
    return LanguageConfigView::MaybeRewriteLanguageName(
        GetLanguageNameAt(index));
  }
};

// The view implements a dialog for adding a language.
class AddLanguageView : public views::View,
                        public views::Combobox::Listener,
                        public views::DialogDelegate {
 public:
  explicit AddLanguageView(LanguageConfigView* parent_view)
      : parent_view_(parent_view),
        language_combobox_(NULL),
        contents_(NULL),
        selected_index_(0) {
  }

  // views::DialogDelegate overrides:
  virtual bool Accept() {
    std::string language_selected = language_combobox_model_->
        GetLocaleFromIndex(selected_index_);
    parent_view_->OnAddLanguage(language_selected);
    return true;
  }

  virtual std::wstring GetWindowTitle() const {
    return l10n_util::GetString(
        IDS_OPTIONS_SETTINGS_LANGUAGES_LANGUAGES);
  }

  // views::WindowDelegate overrides:
  virtual bool IsModal() const { return true; }
  virtual views::View* GetContentsView() { return this; }

  // views::Combobox::Listener overrides:
  virtual void ItemChanged(views::Combobox* combobox,
                           int prev_index,
                           int new_index) {
    selected_index_ = new_index;
  }

  // views::View overrides:
  gfx::Size GetPreferredSize() {
    // TODO(satorux): Create our own localized content size once the UI is
    // done.
    return gfx::Size(views::Window::GetLocalizedContentsSize(
        IDS_FONTSLANG_DIALOG_WIDTH_CHARS,
        IDS_FONTSLANG_DIALOG_HEIGHT_LINES));
  }

  virtual void Layout() {
    // Not sure why but this is needed to show contents in the dialog.
    contents_->SetBounds(0, 0, width(), height());
  }

  virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                    views::View* child) {
    if (is_add && child == this)
      Init();
  }

 private:
  void Init() {
    contents_ = new views::View;
    AddChildView(contents_);

    GridLayout* layout = new GridLayout(contents_);
    contents_->SetLayoutManager(layout);
    layout->SetInsets(kPanelVertMargin, kPanelHorizMargin,
                      kPanelVertMargin, kPanelHorizMargin);

    // Set up column sets for the grid layout.
    const int kColumnSetId = 1;
    ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);
    column_set->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 0,
                          GridLayout::USE_PREF, 0, 0);

    language_combobox_model_.reset(CreateLanguageComboboxModel());
    language_combobox_ = new views::Combobox(language_combobox_model_.get());
    language_combobox_->SetSelectedItem(selected_index_);
    language_combobox_->set_listener(this);
    layout->StartRow(0, kColumnSetId);
    layout->AddView(language_combobox_);
  }

  // Creates the language combobox model from the supported languages.
  LanguageComboboxModel* CreateLanguageComboboxModel() {
    std::vector<std::string> language_codes;
    parent_view_->GetSupportedLanguageCodes(&language_codes);
    // LanguageComboboxModel sorts languages by their display names.
    return new LanguageComboboxModelWithOthers(NULL, language_codes);
  }

  LanguageConfigView* parent_view_;

  // Combobox and its corresponding model.
  scoped_ptr<LanguageComboboxModel> language_combobox_model_;
  views::Combobox* language_combobox_;
  views::View* contents_;
  // The index of the selected item in the combobox.
  int selected_index_;

  DISALLOW_COPY_AND_ASSIGN(AddLanguageView);
};

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

// This is a radio button associated with input method information.
class InputMethodRadioButton : public views::RadioButton {
 public:
  InputMethodRadioButton(const std::wstring& display_name,
                         int group_id,
                         const std::string& input_method_id)
      : views::RadioButton(display_name, group_id),
        input_method_id_(input_method_id) {
  }

  const std::string& input_method_id() const {
    return input_method_id_;
  }

 private:
  std::string input_method_id_;
  DISALLOW_COPY_AND_ASSIGN(InputMethodRadioButton);
};

LanguageConfigView::LanguageConfigView(Profile* profile)
    : OptionsPageView(profile),
      root_container_(NULL),
      right_container_(NULL),
      add_language_button_(NULL),
      remove_language_button_(NULL),
      preferred_language_table_(NULL) {
}

LanguageConfigView::~LanguageConfigView() {
}

void LanguageConfigView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == static_cast<views::Button*>(add_language_button_)) {
    views::Window* window = views::Window::CreateChromeWindow(
        NULL, gfx::Rect(), new AddLanguageView(this));
    window->SetIsAlwaysOnTop(true);
    window->Show();
  } else if (sender == static_cast<views::Button*>(remove_language_button_)) {
    const int row = preferred_language_table_->GetFirstSelectedRow();
    const std::string& language_code = preferred_language_codes_[row];
    DeactivateInputMethodsFor(language_code);
    // Remove the language code and the row from the table.
    preferred_language_codes_.erase(preferred_language_codes_.begin() + row);
    preferred_language_table_->OnItemsRemoved(row, 1);
    // Switch to the previous row, or the first row.
    // There should be at least one row in the table.
    preferred_language_table_->SelectRow(std::max(row - 1, 0));
  } else if (input_method_radio_buttons_.count(
      static_cast<InputMethodRadioButton*>(sender)) > 0) {
    InputMethodRadioButton* radio_button =
        static_cast<InputMethodRadioButton*>(sender);
    const std::string& input_method_id = radio_button->input_method_id();
    if (input_method_id == "USA") {
      // For the time being, we don't allow users to disable "USA" layout.
      // TODO(yusukes): remove this hack when XKB switcher gets ready.
      radio_button->SetChecked(true);
    } else if (radio_button->checked()) {
      // Deactivate all input methods first, then activate one that checked.
      DeactivateInputMethodsFor(GetLanguageCodeFromId(input_method_id));
      SetInputMethodActivated(input_method_id, true);
    }
  } else if (input_method_buttons_.count(
      static_cast<InputMethodButton*>(sender)) > 0) {
    InputMethodButton* button = static_cast<InputMethodButton*>(sender);
    views::DialogDelegate* config_view =
        CreateInputMethodConfigureView(button->input_method_id());
    if (!config_view) {
      DLOG(FATAL) << "Config view not found: " << button->input_method_id();
      return;
    }
    views::Window* window = views::Window::CreateChromeWindow(
        NULL, gfx::Rect(), config_view);
    window->SetIsAlwaysOnTop(true);
    window->Show();
  }
}

void LanguageConfigView::Layout() {
  // Not sure why but this is needed to show contents in the dialog.
  root_container_->SetBounds(0, 0, width(), height());
}

std::wstring LanguageConfigView::GetWindowTitle() const {
  return l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_LANGUAGES_DIALOG_TITLE);
}

gfx::Size LanguageConfigView::GetPreferredSize() {
  // TODO(satorux): Create our own localized content size once the UI is done.
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_FONTSLANG_DIALOG_WIDTH_CHARS,
      IDS_FONTSLANG_DIALOG_HEIGHT_LINES));
}

views::View* LanguageConfigView::CreatePerLanguageConfigView(
    const std::string& target_language_code) {
  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  // Set up column sets for the grid layout.
  const int kTitleColumnSetId = 1;
  ColumnSet* column_set = layout->AddColumnSet(kTitleColumnSetId);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  const int kDoubleColumnSetId = 2;
  column_set = layout->AddColumnSet(kDoubleColumnSetId);
  column_set->AddPaddingColumn(0, kUnrelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  // Create the title label.
  views::Label* title_label = new views::Label(
      l10n_util::GetString(
          IDS_OPTIONS_SETTINGS_LANGUAGES_INPUT_METHOD));
  const gfx::Font bold_font =
      title_label->font().DeriveFont(0, gfx::Font::BOLD);
  title_label->SetFont(bold_font);

  // Add the title label.
  layout->StartRow(0, kTitleColumnSetId);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Add input method names and configuration buttons.
  input_method_buttons_.clear();
  input_method_radio_buttons_.clear();

  const int kInputMethodRadioButtonGroupId = 0;
  std::vector<std::string> input_method_ids;
  GetSupportedInputMethodIds(&input_method_ids);
  // We only show keyboard layouts for languages that don't use IME
  // (ex. English and French). For languages that use IME, we don't show
  // keybard layouts for now.
  // TODO(satorux): This is a temporary hack. Will rework this.
  bool should_show_keyboard_layouts = true;
  for (size_t i = 0; i < input_method_ids.size(); ++i) {
    const std::string language_code =
        GetLanguageCodeFromId(input_method_ids[i]);
    if (target_language_code == language_code &&
        !LanguageLibrary::IsKeyboardLayout(input_method_ids[i])) {
      should_show_keyboard_layouts = false;
      break;
    }
  }

  for (size_t i = 0; i < input_method_ids.size(); ++i) {
    const std::string& input_method_id = input_method_ids[i];
    const std::string language_code = GetLanguageCodeFromId(input_method_id);
    const std::string display_name = GetDisplayNameFromId(input_method_id);
    if (language_code == target_language_code) {
      if (LanguageLibrary::IsKeyboardLayout(input_method_id)
          && !should_show_keyboard_layouts) {
        continue;  // Skip this input method.
      }
      layout->StartRow(0, kDoubleColumnSetId);
      // TODO(satorux): Translate display_name.
      InputMethodRadioButton* radio_button
          = new InputMethodRadioButton(UTF8ToWide(display_name),
                                       kInputMethodRadioButtonGroupId,
                                       input_method_id);
      radio_button->set_listener(this);
      // We should check the radio button associated with the active input
      // method here by radio_button->SetChecked(), but this does not work
      // for a complicated reason. Instead, we'll initialize the radio
      // buttons in InitInputMethodRadioButtons() later.
      // TODO(satorux): Get rid of the workaround.
      layout->AddView(radio_button);
      input_method_radio_buttons_.insert(radio_button);
      // Add "configure" button for the input method if we have a
      // configuration dialog for it.
      if (input_method_config_view_map_.count(input_method_id) > 0) {
        InputMethodButton* button = new InputMethodButton(
            this,
            l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_CONFIGURE),
            input_method_id);
        layout->AddView(button);
        input_method_buttons_.insert(button);
      }
    }
  }

  return contents;
}

void LanguageConfigView::OnSelectionChanged() {
  right_container_->RemoveAllChildViews(true);  // Delete the child views.

  const int row = preferred_language_table_->GetFirstSelectedRow();
  const std::string& language_code = preferred_language_codes_[row];
  // TODO(satorux): For now, don't allow users to remove English.
  // TODO(yusukes): "eng" should be changed to "xkb:en" or something like that.
  if (language_code == "eng") {
    remove_language_button_->SetEnabled(false);
  } else {
    remove_language_button_->SetEnabled(true);
  }

  // Add the per language config view to the right area.
  right_container_->AddChildView(CreatePerLanguageConfigView(language_code));
  InitInputMethodRadioButtons();
  // Let the parent container layout again. This is needed to the the
  // contents on the right to display.
  root_container_->Layout();
}

std::wstring LanguageConfigView::GetText(int row, int column_id) {
  if (row >= 0 && row < static_cast<int>(preferred_language_codes_.size())) {
    string16 language_name16 = l10n_util::GetDisplayNameForLocale(
        preferred_language_codes_[row],
        g_browser_process->GetApplicationLocale(),
        true);
    return MaybeRewriteLanguageName(UTF16ToWide(language_name16));
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
  return preferred_language_codes_.size();
}

void LanguageConfigView::InitControlLayout() {
  // Initialize the maps.
  InitInputMethodIdMaps();
  InitInputMethodConfigViewMap();

  preload_engines_.Init(
      prefs::kLanguagePreloadEngines, profile()->GetPrefs(), this);
  // TODO(yusukes): It might be safer to call GetActiveLanguages() cros API
  // here and compare the result and preload_engines_GetValue(). If there's
  // a discrepancy between IBus setting and Chrome prefs, we can resolve it
  // by calling preload_engines_SetValue() here.

  root_container_ = new views::View;
  AddChildView(root_container_);

  // Set up the layout manager for the root container.  We'll place the
  // language table on the left, and the per language config on the right.
  GridLayout* root_layout = new GridLayout(root_container_);
  root_container_->SetLayoutManager(root_layout);
  root_layout->SetInsets(kPanelVertMargin, kPanelHorizMargin,
                         kPanelVertMargin, kPanelHorizMargin);

  // Set up column sets for the grid layout.
  const int kRootColumnSetId = 0;
  ColumnSet* column_set = root_layout->AddColumnSet(kRootColumnSetId);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                        GridLayout::FIXED, 300, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1.0,
                        GridLayout::USE_PREF, 0, 0);
  root_layout->StartRow(1 /* expand */, kRootColumnSetId);

  // Initialize the language codes currently activated.
  NotifyPrefChanged();

  // Set up the container for the contents on the right.  Just adds a
  // place holder here. This will get replaced in OnSelectionChanged().
  right_container_ = new views::View;
  right_container_->SetLayoutManager(new views::FillLayout);
  right_container_->AddChildView(new views::View);

  // Add the contents on the left and the right.
  root_layout->AddView(CreateContentsOnLeft());
  root_layout->AddView(right_container_);

  // Select the first row in the language table.
  // There should be at least one language in the table.
  CHECK(!preferred_language_codes_.empty());
  preferred_language_table_->SelectRow(0);
}

void LanguageConfigView::InitInputMethodConfigViewMap() {
  input_method_config_view_map_["hangul"] =
      CreateLanguageHangulConfigView;
}

void LanguageConfigView::InitInputMethodIdMaps() {
  // TODO(satorux): Use GetSupportedInputMethods() instead.
  // GetSupportedLanguages() never return NULL.
  scoped_ptr<InputLanguageList> supported_input_methods(
      CrosLibrary::Get()->GetLanguageLibrary()->GetSupportedLanguages());
  for (size_t i = 0; i < supported_input_methods->size(); ++i) {
    // TODO(satorux): Use InputMethodDescriptor instead.
    const InputLanguage& input_method = supported_input_methods->at(i);
    // Normalize the language code as some engines return three-letter
    // codes like "jpn" wheres some other engines return two-letter codes
    // like "ja".
    std::string language_code =
        LanguageLibrary::NormalizeLanguageCode(input_method.language_code);
    id_to_language_code_map_.insert(
        std::make_pair(input_method.id, language_code));
    id_to_display_name_map_.insert(
        std::make_pair(input_method.id, input_method.display_name));
  }
}

void LanguageConfigView::InitInputMethodRadioButtons() {
  for (std::set<InputMethodRadioButton*>::iterator
           iter = input_method_radio_buttons_.begin();
       iter != input_method_radio_buttons_.end(); ++iter) {
    // Check the radio button associated with the active input method.
    // There should be only one active input method here.
    if (InputMethodIsActivated((*iter)->input_method_id())) {
      (*iter)->SetChecked(true);
      break;
    }
  }
}

views::View* LanguageConfigView::CreateContentsOnLeft() {
  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  // Set up column sets for the grid layout.
  const int kTableColumnSetId = 1;
  ColumnSet* column_set = layout->AddColumnSet(kTableColumnSetId);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  const int kButtonsColumnSetId = 2;
  column_set = layout->AddColumnSet(kButtonsColumnSetId);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 0,
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

  // Create the add and remove buttons.
  add_language_button_ = new views::NativeButton(
      this, l10n_util::GetString(
          IDS_OPTIONS_SETTINGS_LANGUAGES_ADD_BUTTON));
  remove_language_button_ = new views::NativeButton(
      this, l10n_util::GetString(
          IDS_OPTIONS_SETTINGS_LANGUAGES_REMOVE_BUTTON));

  // Add the add and remove buttons.
  layout->StartRow(0, kButtonsColumnSetId);
  layout->AddView(add_language_button_);
  layout->AddView(remove_language_button_);

  return contents;
}

void LanguageConfigView::OnAddLanguage(const std::string& language_code) {
  if (std::find(preferred_language_codes_.begin(),
                preferred_language_codes_.end(),
                language_code) == preferred_language_codes_.end()) {
    // Activate the first input language associated with the language. We have
    // to call this before the OnItemsAdded() call below so the radio button
    // for the first input language gets checked.
    std::vector<std::string> input_method_ids;
    GetSupportedInputMethodIds(&input_method_ids);
    for (size_t i = 0; i < input_method_ids.size(); ++i) {
      if (GetLanguageCodeFromId(input_method_ids[i]) == language_code) {
        SetInputMethodActivated(input_method_ids[i], true);
        break;
      }
    }

    // Append the language to the list of language codes.
    preferred_language_codes_.push_back(language_code);
    // Update the language table accordingly.
    preferred_language_table_->OnItemsAdded(RowCount() - 1, 1);
    preferred_language_table_->SelectRow(RowCount() - 1);
  }
}

void LanguageConfigView::DeactivateInputMethodsFor(
    const std::string& language_code) {
  std::vector<std::string> input_method_ids;
  GetSupportedInputMethodIds(&input_method_ids);
  for (size_t i = 0; i < input_method_ids.size(); ++i) {
    if (GetLanguageCodeFromId(input_method_ids[i]) == language_code) {
      SetInputMethodActivated(input_method_ids[i], false);
      // Do not break; here in order to disable all engines that belong to
      // |language_code|.
    }
  }

  // Switch back to the US English.
  // TODO(yusukes): what if "USA" is not active?
  CrosLibrary::Get()->GetLanguageLibrary()->ChangeLanguage(
      chromeos::LANGUAGE_CATEGORY_XKB, "USA");
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

void LanguageConfigView::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED) {
    NotifyPrefChanged();
  }
}

void LanguageConfigView::SetInputMethodActivated(
    const std::string& input_method_id, bool activated) {
  DCHECK(!input_method_id.empty());
  std::vector<std::string> input_method_ids;
  GetActiveInputMethodIds(&input_method_ids);

  std::set<std::string> id_set(input_method_ids.begin(),
                               input_method_ids.end());
  if (activated) {
    // Add |id| if it's not already added.
    id_set.insert(input_method_id);
  } else {
    id_set.erase(input_method_id);
  }

  // Update Chrome's preference.
  std::vector<std::string> new_input_method_ids(id_set.begin(), id_set.end());
  preload_engines_.SetValue(UTF8ToWide(JoinString(new_input_method_ids, ',')));
}

bool LanguageConfigView::InputMethodIsActivated(
    const std::string& input_method_id) {
  std::vector<std::string> input_method_ids;
  GetActiveInputMethodIds(&input_method_ids);
  return (std::find(input_method_ids.begin(), input_method_ids.end(),
                    input_method_id) != input_method_ids.end());
}

void LanguageConfigView::GetActiveInputMethodIds(
    std::vector<std::string>* out_input_method_ids) {
  const std::wstring value = preload_engines_.GetValue();
  out_input_method_ids->clear();
  SplitString(WideToUTF8(value), ',', out_input_method_ids);
}

void LanguageConfigView::GetSupportedInputMethodIds(
    std::vector<std::string>* out_input_method_ids) const {
  out_input_method_ids->clear();
  std::map<std::string, std::string>::const_iterator iter;
  for (iter = id_to_language_code_map_.begin();
       iter != id_to_language_code_map_.end();
       ++iter) {
    out_input_method_ids->push_back(iter->first);
  }
}

void LanguageConfigView::GetSupportedLanguageCodes(
    std::vector<std::string>* out_language_codes) const {
  std::set<std::string> language_code_set;
  std::map<std::string, std::string>::const_iterator iter;
  for (iter = id_to_language_code_map_.begin();
       iter != id_to_language_code_map_.end();
       ++iter) {
    language_code_set.insert(iter->second);
  }
  out_language_codes->clear();
  out_language_codes->assign(
      language_code_set.begin(), language_code_set.end());
}

std::string LanguageConfigView::GetLanguageCodeFromId(
    const std::string& input_method_id) const {
  std::map<std::string, std::string>::const_iterator iter
      = id_to_language_code_map_.find(input_method_id);
  return (iter == id_to_language_code_map_.end()) ? "" : iter->second;
}

std::string LanguageConfigView::GetDisplayNameFromId(
    const std::string& input_method_id) const {
  std::map<std::string, std::string>::const_iterator iter
      = id_to_display_name_map_.find(input_method_id);
  return (iter == id_to_display_name_map_.end()) ? "" : iter->second;
}

void LanguageConfigView::NotifyPrefChanged() {
  std::vector<std::string> input_method_ids;
  GetActiveInputMethodIds(&input_method_ids);

  std::set<std::string> language_code_set;
  for (size_t i = 0; i < input_method_ids.size(); ++i) {
    const std::string language_code =
        GetLanguageCodeFromId(input_method_ids[i]);
    language_code_set.insert(language_code);
  }

  preferred_language_codes_.clear();
  preferred_language_codes_.assign(
      language_code_set.begin(), language_code_set.end());
}

std::wstring LanguageConfigView::MaybeRewriteLanguageName(
    const std::wstring& language_name) {
  // "t" is used as the language code for input methods that don't fall
  // under any other languages.
  if (language_name == L"t") {
    return l10n_util::GetString(
        IDS_OPTIONS_SETTINGS_LANGUAGES_OTHERS);
  }
  return language_name;
}

}  // namespace chromeos
