// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/language_hangul_config_view.h"

#include "app/combobox_model.h"
#include "app/l10n_util.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/input_method_library.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/label.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

namespace chromeos {

// The combobox model for the list of hangul keyboards.
class HangulKeyboardComboboxModel : public ComboboxModel {
 public:
  HangulKeyboardComboboxModel() {
    for (size_t i = 0; i < language_prefs::kNumHangulKeyboardNameIDPairs;
         ++i) {
      layouts_.push_back(std::make_pair(
          l10n_util::GetStringUTF8(
              language_prefs::kHangulKeyboardNameIDPairs[i].message_id),
          language_prefs::kHangulKeyboardNameIDPairs[i].keyboard_id));
    }
  }

  // Implements ComboboxModel interface.
  virtual int GetItemCount() {
    return static_cast<int>(layouts_.size());
  }

  // Implements ComboboxModel interface.
  virtual string16 GetItemAt(int index) {
    if (index < 0 || index > GetItemCount()) {
      LOG(ERROR) << "Index is out of bounds: " << index;
      return string16();
    }
    return UTF8ToUTF16(layouts_.at(index).first);
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
  std::vector<std::pair<std::string, std::string> > layouts_;
  DISALLOW_COPY_AND_ASSIGN(HangulKeyboardComboboxModel);
};

LanguageHangulConfigView::LanguageHangulConfigView(Profile* profile)
    : OptionsPageView(profile),
      contents_(NULL),
      hangul_keyboard_combobox_(NULL),
      hangul_keyboard_combobox_model_(new HangulKeyboardComboboxModel) {
  keyboard_pref_.Init(
      prefs::kLanguageHangulKeyboard, profile->GetPrefs(), this);
}

LanguageHangulConfigView::~LanguageHangulConfigView() {
}

void LanguageHangulConfigView::ItemChanged(
    views::Combobox* sender, int prev_index, int new_index) {
  const std::string id =
      hangul_keyboard_combobox_model_->GetItemIDAt(new_index);
  VLOG(1) << "Changing Hangul keyboard pref to " << id;
  keyboard_pref_.SetValue(id);
}

void LanguageHangulConfigView::Layout() {
  // Not sure why but this is needed to show contents in the dialog.
  contents_->SetBounds(0, 0, width(), height());
}

int LanguageHangulConfigView::GetDialogButtons() const {
  return MessageBoxFlags::DIALOGBUTTON_OK;
}

std::wstring LanguageHangulConfigView::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK) {
    return l10n_util::GetString(IDS_OK);
  }
  return L"";
}

std::wstring LanguageHangulConfigView::GetWindowTitle() const {
  return l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_LANGUAGES_HANGUL_SETTINGS_TITLE);
}

gfx::Size LanguageHangulConfigView::GetPreferredSize() {
  // TODO(satorux): Create our own localized content size once the UI is done.
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_LANGUAGES_INPUT_DIALOG_WIDTH_CHARS,
      IDS_LANGUAGES_INPUT_DIALOG_HEIGHT_LINES));
}

void LanguageHangulConfigView::InitControlLayout() {
  using views::ColumnSet;
  using views::GridLayout;

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

  // Initialize the combobox to what's saved in user preferences. Otherwise,
  // ItemChanged() will be called with |new_index| == 0.
  NotifyPrefChanged();
  layout->AddView(hangul_keyboard_combobox_);
}

void LanguageHangulConfigView::Observe(NotificationType type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED) {
    // Observe(PREF_CHANGED) is called when the chromeos::Preferences object
    // changed the prefs::kLanguageHangulKeyboard preference. Note that this
    // function is NOT called when this dialog calls keyboard_pref_.SetValue().
    NotifyPrefChanged();
  }
}

void LanguageHangulConfigView::NotifyPrefChanged() {
  const std::string id = keyboard_pref_.GetValue();
  const int index =
      hangul_keyboard_combobox_model_->GetIndexFromID(id);
  if (index >= 0)
    hangul_keyboard_combobox_->SetSelectedItem(index);
}

}  // namespace chromeos
