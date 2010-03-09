// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/language_config_view.h"

#include "app/combobox_model.h"
#include "app/l10n_util.h"
#include "chrome/browser/chromeos/cros/language_library.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/label.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

namespace chromeos {

const char LanguageConfigView::kHangulSection[] = "engine/Hangul";
const char LanguageConfigView::kHangulKeyboardConfigName[] = "HangulKeyboard";

// This is a checkbox associated with input language information.
class LanguageCheckbox : public views::Checkbox {
 public:
  explicit LanguageCheckbox(const InputLanguage& language)
      : views::Checkbox(UTF8ToWide(language.display_name)),
        language_(language) {
  }

  const InputLanguage& language() const {
    return language_;
  }

 private:
  InputLanguage language_;
  DISALLOW_COPY_AND_ASSIGN(LanguageCheckbox);
};

// The combobox model for the list of hangul keyboards.
class HangulKeyboardComboboxModel : public ComboboxModel {
 public:
  HangulKeyboardComboboxModel() {
    // We have to sync the IDs ("2", "3f", "39", ...) with those in
    // ibus-hangul/setup/main.py.
    // TODO(yusukes): Use l10n_util::GetString for these label strings.
    layouts_.push_back(std::make_pair(L"Dubeolsik", "2"));
    layouts_.push_back(std::make_pair(L"Sebeolsik Final", "3f"));
    layouts_.push_back(std::make_pair(L"Sebeolsik 390", "39"));
    layouts_.push_back(std::make_pair(L"Sebeolsik No-shift", "3s"));
    layouts_.push_back(std::make_pair(L"Sebeolsik 2 set", "32"));
  }

  // Implements ComboboxModel interface.
  virtual int GetItemCount() {
    return static_cast<int>(layouts_.size());
  }

  // Implements ComboboxModel interface.
  virtual std::wstring GetItemAt(int index) {
    if (index < 0 || index > GetItemCount()) {
      LOG(ERROR) << "Index is out of bounds: " << index;
      return L"";
    }
    return layouts_.at(index).first;
  }

  // Gets a keyboard layout ID (e.g. "2", "3f", ..) for an item at zero-origin
  // |index|. This function is NOT part of the ComboboxModel interface.
  std::string GetItemIDAt(int index) {
    if (index < 0 || index > GetItemCount()) {
      LOG(ERROR) << "Index is out of bounds: " << index;
      return "";
    }
    return layouts_.at(index).second;
  }

  // Gets an index (>= 0) of an item whose keyboard layout ID is |layout_ld|.
  // Returns -1 if such item is not found. This function is NOT part of the
  // ComboboxModel interface.
  int GetIndexFromID(const std::string& layout_id) {
    for (size_t i = 0; i < layouts_.size(); ++i) {
      if (GetItemIDAt(i) == layout_id) {
        return static_cast<int>(i);
      }
    }
    return -1;
  }

 private:
  std::vector<std::pair<std::wstring, std::string> > layouts_;
  DISALLOW_COPY_AND_ASSIGN(HangulKeyboardComboboxModel);
};


LanguageConfigView::LanguageConfigView() :
    contents_(NULL),
    hangul_keyboard_combobox_(NULL),
    hangul_keyboard_combobox_model_(new HangulKeyboardComboboxModel) {
}

LanguageConfigView::~LanguageConfigView() {
}

void LanguageConfigView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  LanguageCheckbox* checkbox = static_cast<LanguageCheckbox*>(sender);
  const InputLanguage& language = checkbox->language();
  // Check if the checkbox is now being checked.
  if (checkbox->checked()) {
    // TODO(yusukes): limit the number of active languages so the pop-up menu
    //   of the language_menu_button does not overflow.

    // Try to activate the language.
    if (!LanguageLibrary::Get()->ActivateLanguage(language.category,
                                                  language.id)) {
      // Activation should never fail (failure means something is broken),
      // but if it fails we just revert the checkbox and ignore the error.
      // TODO(satorux): Implement a better error handling if it becomes
      // necessary.
      checkbox->SetChecked(false);
      LOG(ERROR) << "Failed to activate language: " << language.display_name;
    }
  } else {
    // Try to deactivate the language.
    if (!LanguageLibrary::Get()->DeactivateLanguage(language.category,
                                                    language.id)) {
      // See a comment above about the activation failure.
      checkbox->SetChecked(true);
      LOG(ERROR) << "Failed to deactivate language: " << language.display_name;
    }
  }
}

void LanguageConfigView::ItemChanged(
    views::Combobox* sender, int prev_index, int new_index) {
  ImeConfigValue config;
  config.type = ImeConfigValue::kValueTypeString;
  config.string_value =
      hangul_keyboard_combobox_model_->GetItemIDAt(new_index);
  LanguageLibrary::Get()->SetImeConfig(
      kHangulSection, kHangulKeyboardConfigName, config);

  UpdateHangulKeyboardCombobox();
}

void LanguageConfigView::Layout() {
  // Not sure why but this is needed to show contents in the dialog.
  contents_->SetBounds(0, 0, width(), height());
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

void LanguageConfigView::ViewHierarchyChanged(
    bool is_add, views::View* parent, views::View* child) {
  // Can't init before we're inserted into a Container.
  if (is_add && child == this) {
    Init();
  }
}

void LanguageConfigView::Init() {
  using views::ColumnSet;
  using views::GridLayout;

  if (contents_) return; // Already initialized.
  contents_ = new views::View;
  AddChildView(contents_);

  GridLayout* layout = new GridLayout(contents_);
  contents_->SetLayoutManager(layout);
  layout->SetInsets(kPanelVertMargin, kPanelHorizMargin,
                    kPanelVertMargin, kPanelHorizMargin);

  // Set up the single column set.
  const int kSingleColumnViewSetId = 1;
  ColumnSet* column_set = layout->AddColumnSet(kSingleColumnViewSetId);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  // Set up the double column set.
  const int kDoubleColumnSetId = 2;
  column_set = layout->AddColumnSet(kDoubleColumnSetId);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);


  // GetActiveLanguages() and GetSupportedLanguages() never return NULL.
  scoped_ptr<InputLanguageList> active_language_list(
      LanguageLibrary::Get()->GetActiveLanguages());
  scoped_ptr<InputLanguageList> supported_language_list(
      LanguageLibrary::Get()->GetSupportedLanguages());

  for (size_t i = 0; i < supported_language_list->size(); ++i) {
    const InputLanguage& language = supported_language_list->at(i);
    // Check if |language| is active. Note that active_language_list is
    // small (usually a couple), so scanning here is fine.
    const bool language_is_active =
        (std::find(active_language_list->begin(),
                   active_language_list->end(),
                   language) != active_language_list->end());
    // Create a checkbox.
    LanguageCheckbox* checkbox = new LanguageCheckbox(language);
    checkbox->SetChecked(language_is_active);
    checkbox->set_listener(this);

    // We use two columns. Start a column if the counter is an even number.
    if (i % 2 == 0) {
      layout->StartRow(0, kDoubleColumnSetId);
    }
    // Add the checkbox to the layout manager.
    layout->AddView(checkbox);
  }

  // Settings for IME engines.
  // TODO(yusukes): This is a temporary location of the settings. Ask UX team
  //   where is the best place for this and then move the code.
  // TODO(yusukes): Use l10n_util::GetString for all views::Labels.

  // Hangul IME
  hangul_keyboard_combobox_
      = new views::Combobox(hangul_keyboard_combobox_model_.get());
  hangul_keyboard_combobox_->set_listener(this);
  UpdateHangulKeyboardCombobox();
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, kSingleColumnViewSetId);
  layout->AddView(
      new views::Label(
          l10n_util::GetString(IDS_OPTIONS_SETTINGS_HANGUL_IME_TEXT)),
      1, 1, GridLayout::LEADING, GridLayout::LEADING);
  layout->StartRow(0, kDoubleColumnSetId);
  layout->AddView(new views::Label(
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_KEYBOARD_LAYOUT_TEXT)));
  layout->AddView(hangul_keyboard_combobox_);
}

void LanguageConfigView::UpdateHangulKeyboardCombobox() {
  DCHECK(hangul_keyboard_combobox_);
  ImeConfigValue config;
  if (LanguageLibrary::Get()->GetImeConfig(
          kHangulSection, kHangulKeyboardConfigName, &config)) {
    const int index
        = hangul_keyboard_combobox_model_->GetIndexFromID(config.string_value);
    if (index >= 0) {
      hangul_keyboard_combobox_->SetSelectedItem(index);
    }
  }
}

}  // namespace chromeos
