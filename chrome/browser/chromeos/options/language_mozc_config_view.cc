// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/language_mozc_config_view.h"

#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/options/language_config_util.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/button/native_button.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

namespace {
// The tags are used to identify buttons in ButtonPressed().
enum ButtonTag {
  // 0 to kNumMozcBooleanPrefs - 1 are reserved for the checkboxes for integer
  // preferences.
  kResetToDefaultsButton = chromeos::language_prefs::kNumMozcBooleanPrefs,
};
}  // namespace

namespace chromeos {

LanguageMozcConfigView::LanguageMozcConfigView(Profile* profile)
    : OptionsPageView(profile),
      contents_(NULL),
      reset_to_defaults_button_(NULL) {
  for (size_t i = 0; i < language_prefs::kNumMozcBooleanPrefs; ++i) {
    MozcPrefAndAssociatedCheckbox& current = prefs_and_checkboxes_[i];
    current.boolean_pref.Init(
        language_prefs::kMozcBooleanPrefs[i].pref_name, profile->GetPrefs(),
        this);
    current.checkbox = NULL;
  }
  for (size_t i = 0; i < language_prefs::kNumMozcMultipleChoicePrefs; ++i) {
    MozcPrefAndAssociatedCombobox& current = prefs_and_comboboxes_[i];
    current.multiple_choice_pref.Init(
        language_prefs::kMozcMultipleChoicePrefs[i].pref_name,
        profile->GetPrefs(), this);
    current.combobox_model =
        new LanguageComboboxModel<const char*>(
            &language_prefs::kMozcMultipleChoicePrefs[i]);
    current.combobox = NULL;
  }
  for (size_t i = 0; i < language_prefs::kNumMozcIntegerPrefs; ++i) {
    MozcPrefAndAssociatedSlider& current = prefs_and_sliders_[i];
    current.integer_pref.Init(
        language_prefs::kMozcIntegerPrefs[i].pref_name, profile->GetPrefs(),
        this);
    current.slider = NULL;
  }
}

LanguageMozcConfigView::~LanguageMozcConfigView() {
  for (size_t i = 0; i < language_prefs::kNumMozcMultipleChoicePrefs; ++i) {
    delete prefs_and_comboboxes_[i].combobox_model;
  }
}

void LanguageMozcConfigView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  const int pref_id = sender->tag();
  if (pref_id == kResetToDefaultsButton) {
    ResetToDefaults();
    return;
  }
  views::Checkbox* checkbox = static_cast<views::Checkbox*>(sender);
  DCHECK(pref_id >= 0 && pref_id < static_cast<int>(
      language_prefs::kNumMozcBooleanPrefs));
  prefs_and_checkboxes_[pref_id].boolean_pref.SetValue(checkbox->checked());
}

void LanguageMozcConfigView::ItemChanged(
    views::Combobox* sender, int prev_index, int new_index) {
  if (new_index < 0 || sender->model()->GetItemCount() <= new_index) {
    LOG(ERROR) << "Invalid new_index: " << new_index;
    return;
  }
  for (size_t i = 0; i < language_prefs::kNumMozcMultipleChoicePrefs; ++i) {
    MozcPrefAndAssociatedCombobox& current = prefs_and_comboboxes_[i];
    if (current.combobox == sender) {
      const std::string config_value =
          current.combobox_model->GetConfigValueAt(new_index);
      VLOG(1) << "Changing Mozc pref to " << config_value;
      // Update the Chrome pref.
      current.multiple_choice_pref.SetValue(config_value);
      break;
    }
  }
}

void LanguageMozcConfigView::SliderValueChanged(views::Slider* sender) {
  size_t pref_id;
  for (pref_id = 0; pref_id < language_prefs::kNumMozcIntegerPrefs;
       ++pref_id) {
    if (prefs_and_sliders_[pref_id].slider == sender)
      break;
  }
  DCHECK(pref_id < language_prefs::kNumMozcIntegerPrefs);
  prefs_and_sliders_[pref_id].integer_pref.SetValue(sender->value());
}

void LanguageMozcConfigView::Layout() {
  // Not sure why but this is needed to show contents in the dialog.
  contents_->SetBounds(0, 0, width(), height());
}

int LanguageMozcConfigView::GetDialogButtons() const {
  return MessageBoxFlags::DIALOGBUTTON_OK;
}

std::wstring LanguageMozcConfigView::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK) {
    return l10n_util::GetString(IDS_OK);
  }
  return L"";
}

std::wstring LanguageMozcConfigView::GetWindowTitle() const {
  return l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SETTINGS_TITLE);
}

gfx::Size LanguageMozcConfigView::GetPreferredSize() {
  // TODO(satorux): Create our own localized content size once the UI is done.
  gfx::Size preferred_size = views::Window::GetLocalizedContentsSize(
      IDS_LANGUAGES_INPUT_DIALOG_WIDTH_CHARS,
      IDS_LANGUAGES_INPUT_DIALOG_HEIGHT_LINES);
  // TODO(mazda): Remove the manual adjustment.
  // The padding is needed for accommodating all the controls in the dialog.
  const int kHeightPadding = 80;
  preferred_size.Enlarge(0, kHeightPadding);
  return preferred_size;
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
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  for (size_t i = 0; i < language_prefs::kNumMozcBooleanPrefs; ++i) {
    MozcPrefAndAssociatedCheckbox& current = prefs_and_checkboxes_[i];
    current.checkbox = new views::Checkbox(
        l10n_util::GetString(language_prefs::kMozcBooleanPrefs[i].message_id));
    current.checkbox->set_listener(this);
    current.checkbox->set_tag(i);
  }
  for (size_t i = 0; i < language_prefs::kNumMozcMultipleChoicePrefs; ++i) {
    MozcPrefAndAssociatedCombobox& current = prefs_and_comboboxes_[i];
    current.combobox = new LanguageCombobox(current.combobox_model);
    current.combobox->set_listener(this);
  }
  for (size_t i = 0; i < language_prefs::kNumMozcIntegerPrefs; ++i) {
    MozcPrefAndAssociatedSlider& current = prefs_and_sliders_[i];
    current.slider = new views::Slider(
        language_prefs::kMozcIntegerPrefs[i].min_pref_value,
        language_prefs::kMozcIntegerPrefs[i].max_pref_value,
        1,
        static_cast<views::Slider::StyleFlags>(
            views::Slider::STYLE_DRAW_VALUE |
            views::Slider::STYLE_UPDATE_ON_RELEASE),
        this);
  }
  NotifyPrefChanged();  // Sync the comboboxes with current Chrome prefs.

  reset_to_defaults_button_ = new views::NativeButton(
      this, l10n_util::GetString(
          IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_RESET_TO_DEFAULTS_BUTTON));
  reset_to_defaults_button_->set_tag(kResetToDefaultsButton);
  layout->StartRow(0, kColumnSetId);
  layout->AddView(reset_to_defaults_button_);

  // Show the checkboxes.
  for (size_t i = 0; i < language_prefs::kNumMozcBooleanPrefs; ++i) {
    const MozcPrefAndAssociatedCheckbox& current = prefs_and_checkboxes_[i];
    layout->StartRow(0, kColumnSetId);
    layout->AddView(current.checkbox, 3, 1);
  }
  // Show the comboboxes.
  for (size_t i = 0; i < language_prefs::kNumMozcMultipleChoicePrefs; ++i) {
    const MozcPrefAndAssociatedCombobox& current = prefs_and_comboboxes_[i];
    layout->StartRow(0, kColumnSetId);
    layout->AddView(new views::Label(current.combobox_model->GetLabel()));
    layout->AddView(current.combobox);
  }
  for (size_t i = 0; i < language_prefs::kNumMozcIntegerPrefs; ++i) {
    const MozcPrefAndAssociatedSlider& current = prefs_and_sliders_[i];
    layout->StartRow(0, kColumnSetId);
    layout->AddView(new views::Label(
        l10n_util::GetString(
            language_prefs::kMozcIntegerPrefs[i].message_id)));
    layout->AddView(current.slider);
  }
  NotifyPrefChanged();  // Sync the slider with current Chrome prefs.
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
  for (size_t i = 0; i < language_prefs::kNumMozcBooleanPrefs; ++i) {
    MozcPrefAndAssociatedCheckbox& current = prefs_and_checkboxes_[i];
    const bool checked = current.boolean_pref.GetValue();
    current.checkbox->SetChecked(checked);
  }
  for (size_t i = 0; i < language_prefs::kNumMozcMultipleChoicePrefs; ++i) {
    MozcPrefAndAssociatedCombobox& current = prefs_and_comboboxes_[i];
    const std::string value = current.multiple_choice_pref.GetValue();
    for (int i = 0; i < current.combobox_model->num_items(); ++i) {
      if (current.combobox_model->GetConfigValueAt(i) == value) {
        current.combobox->SetSelectedItem(i);
        break;
      }
    }
  }
  for (size_t i = 0; i < language_prefs::kNumMozcIntegerPrefs; ++i) {
    MozcPrefAndAssociatedSlider& current = prefs_and_sliders_[i];
    const int value = current.integer_pref.GetValue();
    current.slider->SetValue(value);
  }
}

void LanguageMozcConfigView::ResetToDefaults() {
  for (size_t i = 0; i < language_prefs::kNumMozcBooleanPrefs; ++i) {
    prefs_and_checkboxes_[i].boolean_pref.SetValue(
        language_prefs::kMozcBooleanPrefs[i].default_pref_value);
  }
  for (size_t i = 0; i < language_prefs::kNumMozcMultipleChoicePrefs; ++i) {
    prefs_and_comboboxes_[i].multiple_choice_pref.SetValue(
        language_prefs::kMozcMultipleChoicePrefs[i].default_pref_value);
  }
  for (size_t i = 0; i < language_prefs::kNumMozcIntegerPrefs; ++i) {
    prefs_and_sliders_[i].integer_pref.SetValue(
        language_prefs::kMozcIntegerPrefs[i].default_pref_value);
  }
  // Reflect the preference changes to the controls.
  NotifyPrefChanged();
}

}  // namespace chromeos
