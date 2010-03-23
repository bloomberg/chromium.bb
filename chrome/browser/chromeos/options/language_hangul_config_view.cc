// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/language_hangul_config_view.h"

#include "app/combobox_model.h"
#include "app/l10n_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/language_library.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/label.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

namespace chromeos {

const char LanguageHangulConfigView::kHangulSection[] = "engine/Hangul";
const char LanguageHangulConfigView::kHangulKeyboardConfigName[] =
    "HangulKeyboard";

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

LanguageHangulConfigView::LanguageHangulConfigView()
    : contents_(NULL),
      hangul_keyboard_combobox_(NULL),
      hangul_keyboard_combobox_model_(new HangulKeyboardComboboxModel) {
}

LanguageHangulConfigView::~LanguageHangulConfigView() {
}

void LanguageHangulConfigView::ItemChanged(
    views::Combobox* sender, int prev_index, int new_index) {
  ImeConfigValue config;
  config.type = ImeConfigValue::kValueTypeString;
  config.string_value =
      hangul_keyboard_combobox_model_->GetItemIDAt(new_index);
  CrosLibrary::Get()->GetLanguageLibrary()->SetImeConfig(
      kHangulSection, kHangulKeyboardConfigName, config);

  UpdateHangulKeyboardCombobox();
}

void LanguageHangulConfigView::Layout() {
  // Not sure why but this is needed to show contents in the dialog.
  contents_->SetBounds(0, 0, width(), height());
}

std::wstring LanguageHangulConfigView::GetWindowTitle() const {
  return l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_LANGUAGES_HANGUL_SETTINGS_TITLE);
}

gfx::Size LanguageHangulConfigView::GetPreferredSize() {
  // TODO(satorux): Create our own localized content size once the UI is done.
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_FONTSLANG_DIALOG_WIDTH_CHARS,
      IDS_FONTSLANG_DIALOG_HEIGHT_LINES));
}

void LanguageHangulConfigView::ViewHierarchyChanged(
    bool is_add, views::View* parent, views::View* child) {
  // Can't init before we're inserted into a Container.
  if (is_add && child == this) {
    Init();
  }
}

void LanguageHangulConfigView::Init() {
  using views::ColumnSet;
  using views::GridLayout;

  if (contents_) return; // Already initialized.
  contents_ = new views::View;
  AddChildView(contents_);

  GridLayout* layout = new GridLayout(contents_);
  layout->SetInsets(kPanelVertMargin, kPanelHorizMargin,
                    kPanelVertMargin, kPanelHorizMargin);
  contents_->SetLayoutManager(layout);

  const int kColumnSetId = 0;
  ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, kColumnSetId);

  // Settings for the Hangul IME.
  layout->AddView(new views::Label(
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_KEYBOARD_LAYOUT_TEXT)));

  hangul_keyboard_combobox_
      = new views::Combobox(hangul_keyboard_combobox_model_.get());
  hangul_keyboard_combobox_->set_listener(this);
  UpdateHangulKeyboardCombobox();
  layout->AddView(hangul_keyboard_combobox_);
}

void LanguageHangulConfigView::UpdateHangulKeyboardCombobox() {
  DCHECK(hangul_keyboard_combobox_);
  ImeConfigValue config;
  if (CrosLibrary::Get()->GetLanguageLibrary()->GetImeConfig(
          kHangulSection, kHangulKeyboardConfigName, &config)) {
    const int index
        = hangul_keyboard_combobox_model_->GetIndexFromID(config.string_value);
    if (index >= 0) {
      hangul_keyboard_combobox_->SetSelectedItem(index);
    }
  }
}

}  // namespace chromeos
