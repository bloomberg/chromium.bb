// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/language_mozc_config_view.h"

#include "app/combobox_model.h"
#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

namespace chromeos {

// The combobox model for Mozc Japanese input method prefs.
class MozcComboboxModel : public ComboboxModel {
 public:
  explicit MozcComboboxModel(const MozcMultipleChoicePreference* pref_data)
      : pref_data_(pref_data), num_items_(0) {
    // Check how many items are defined in the |pref_data->values_and_ids|
    // array.
    for (size_t i = 0; i < MozcMultipleChoicePreference::kMaxItems; ++i) {
      if ((pref_data_->values_and_ids)[i].ibus_config_value == NULL) {
        break;
      }
      ++num_items_;
    }
  }

  // Implements ComboboxModel interface.
  virtual int GetItemCount() {
    return num_items_;
  }

  // Implements ComboboxModel interface.
  virtual std::wstring GetItemAt(int index) {
    if (index < 0 || index >= num_items_) {
      LOG(ERROR) << "Index is out of bounds: " << index;
      return L"";
    }
    const int message_id = (pref_data_->values_and_ids)[index].item_message_id;
    return l10n_util::GetString(message_id);
  }

  // Gets a label for the combobox like "Input mode". This function is NOT part
  // of the ComboboxModel interface.
  std::wstring GetLabel() const {
    return l10n_util::GetString(pref_data_->label_message_id);
  }

  // Gets a config value for the ibus configuration daemon (e.g. "KUTEN_TOUTEN",
  // "KUTEN_PERIOD", ..) for an item at zero-origin |index|. This function is
  // NOT part of the ComboboxModel interface.
  std::wstring GetConfigValueAt(int index) const {
    if (index < 0 || index >= num_items_) {
      LOG(ERROR) << "Index is out of bounds: " << index;
      return L"";
    }
    return UTF8ToWide((pref_data_->values_and_ids)[index].ibus_config_value);
  }

  // Gets an index (>= 0) of an item such that GetConfigValueAt(index) is equal
  // to the |config_value|. Returns -1 if such item is not found. This function
  // is NOT part of the ComboboxModel interface.
  int GetIndexFromConfigValue(const std::wstring& config_value) const {
    for (int i = 0; i < num_items_; ++i) {
      if (GetConfigValueAt(i) == config_value) {
        return i;
      }
    }
    return -1;
  }

 private:
  const MozcMultipleChoicePreference* pref_data_;
  int num_items_;

  DISALLOW_COPY_AND_ASSIGN(MozcComboboxModel);
};

// The combobox for the dialog which has minimum width.
class MozcCombobox : public views::Combobox {
 public:
  explicit MozcCombobox(ComboboxModel* model) : Combobox(model) {
  }

  virtual gfx::Size GetPreferredSize() {
    gfx::Size size = Combobox::GetPreferredSize();
    if (size.width() < kMinComboboxWidth) {
      size.set_width(kMinComboboxWidth);
    }
    return size;
  }

 private:
  static const int kMinComboboxWidth = 100;

  DISALLOW_COPY_AND_ASSIGN(MozcCombobox);
};

LanguageMozcConfigView::LanguageMozcConfigView(Profile* profile)
    : OptionsPageView(profile), contents_(NULL) {
  for (size_t i = 0; i < kNumMozcMultipleChoicePrefs; ++i) {
    MozcPrefAndAssociatedCombobox& current = prefs_and_comboboxes_[i];
    current.multiple_choice_pref.Init(
        kMozcMultipleChoicePrefs[i].pref_name, profile->GetPrefs(), this);
    current.combobox_model =
        new MozcComboboxModel(&kMozcMultipleChoicePrefs[i]);
    current.combobox = NULL;
  }
}

LanguageMozcConfigView::~LanguageMozcConfigView() {
  for (size_t i = 0; i < kNumMozcMultipleChoicePrefs; ++i) {
    delete prefs_and_comboboxes_[i].combobox_model;
  }
}

void LanguageMozcConfigView::ItemChanged(
    views::Combobox* sender, int prev_index, int new_index) {
  for (size_t i = 0; i < kNumMozcMultipleChoicePrefs; ++i) {
    MozcPrefAndAssociatedCombobox& current = prefs_and_comboboxes_[i];
    if (current.combobox == sender) {
      const std::wstring config_value =
          current.combobox_model->GetConfigValueAt(new_index);
      LOG(INFO) << "Changing Mozc pref to " << config_value;
      // Update the Chrome pref.
      current.multiple_choice_pref.SetValue(config_value);
      break;
    }
  }
}

void LanguageMozcConfigView::Layout() {
  // Not sure why but this is needed to show contents in the dialog.
  contents_->SetBounds(0, 0, width(), height());
}

std::wstring LanguageMozcConfigView::GetWindowTitle() const {
  return l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SETTINGS_TITLE);
}

gfx::Size LanguageMozcConfigView::GetPreferredSize() {
  // TODO(satorux): Create our own localized content size once the UI is done.
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_LANGUAGES_INPUT_DIALOG_WIDTH_CHARS,
      IDS_LANGUAGES_INPUT_DIALOG_HEIGHT_LINES));
}

void LanguageMozcConfigView::InitControlLayout() {
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

  for (size_t i = 0; i < kNumMozcMultipleChoicePrefs; ++i) {
    MozcPrefAndAssociatedCombobox& current = prefs_and_comboboxes_[i];
    current.combobox = new MozcCombobox(current.combobox_model);
    current.combobox->set_listener(this);
  }
  NotifyPrefChanged();  // Sync the comboboxes with current Chrome prefs.

  // Show the comboboxes.
  for (size_t i = 0; i < kNumMozcMultipleChoicePrefs; ++i) {
    const MozcPrefAndAssociatedCombobox& current = prefs_and_comboboxes_[i];
    layout->StartRow(0, kColumnSetId);
    layout->AddView(new views::Label(current.combobox_model->GetLabel()));
    layout->AddView(current.combobox);
  }
}

void LanguageMozcConfigView::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED) {
    NotifyPrefChanged();
  }
}

void LanguageMozcConfigView::NotifyPrefChanged() {
  // Update comboboxes.
  // TODO(yusukes): We don't have to update all UI controls.
  for (size_t i = 0; i < kNumMozcMultipleChoicePrefs; ++i) {
    MozcPrefAndAssociatedCombobox& current = prefs_and_comboboxes_[i];
    const std::wstring value = current.multiple_choice_pref.GetValue();
    const int combo_index =
        current.combobox_model->GetIndexFromConfigValue(value);
    if (combo_index >= 0) {
      current.combobox->SetSelectedItem(combo_index);
    }
  }
}

}  // namespace chromeos
