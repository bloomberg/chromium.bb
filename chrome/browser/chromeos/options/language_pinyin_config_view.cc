// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/language_pinyin_config_view.h"

#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/input_method_library.h"
#include "chrome/browser/chromeos/options/language_config_util.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/profiles/profile.h"
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

LanguagePinyinConfigView::LanguagePinyinConfigView(Profile* profile)
    : OptionsPageView(profile), contents_(NULL) {
  for (size_t i = 0; i < language_prefs::kNumPinyinBooleanPrefs; ++i) {
    pinyin_boolean_prefs_[i].Init(
        language_prefs::kPinyinBooleanPrefs[i].pref_name, profile->GetPrefs(),
        this);
    pinyin_boolean_checkboxes_[i] = NULL;
  }

  double_pinyin_schema_.multiple_choice_pref.Init(
      language_prefs::kPinyinDoublePinyinSchema.pref_name,
      profile->GetPrefs(), this);
  double_pinyin_schema_.combobox_model =
      new LanguageComboboxModel<int>(
          &language_prefs::kPinyinDoublePinyinSchema);
  double_pinyin_schema_.combobox = NULL;
}

LanguagePinyinConfigView::~LanguagePinyinConfigView() {
}

void LanguagePinyinConfigView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  views::Checkbox* checkbox = static_cast<views::Checkbox*>(sender);
  const int pref_id = checkbox->tag();
  DCHECK(pref_id >= 0 && pref_id < static_cast<int>(
      language_prefs::kNumPinyinBooleanPrefs));
  pinyin_boolean_prefs_[pref_id].SetValue(checkbox->checked());
}

void LanguagePinyinConfigView::ItemChanged(
    views::Combobox* sender, int prev_index, int new_index) {
  if (double_pinyin_schema_.combobox == sender) {
    const int config_value =
        double_pinyin_schema_.combobox_model->GetConfigValueAt(new_index);
    VLOG(1) << "Changing Pinyin pref to " << config_value;
    // Update the Chrome pref.
    double_pinyin_schema_.multiple_choice_pref.SetValue(config_value);
  }
}

void LanguagePinyinConfigView::Layout() {
  // Not sure why but this is needed to show contents in the dialog.
  contents_->SetBounds(0, 0, width(), height());
}

int LanguagePinyinConfigView::GetDialogButtons() const {
  return MessageBoxFlags::DIALOGBUTTON_OK;
}

std::wstring LanguagePinyinConfigView::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK) {
    return UTF16ToWide(l10n_util::GetStringUTF16(IDS_OK));
  }
  return L"";
}

std::wstring LanguagePinyinConfigView::GetWindowTitle() const {
  return UTF16ToWide(l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTINGS_TITLE));
}

gfx::Size LanguagePinyinConfigView::GetPreferredSize() {
  // TODO(satorux): Create our own localized content size once the UI is done.
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_LANGUAGES_INPUT_DIALOG_WIDTH_CHARS,
      IDS_LANGUAGES_INPUT_DIALOG_HEIGHT_LINES));
}

void LanguagePinyinConfigView::InitControlLayout() {
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

  for (size_t i = 0; i < language_prefs::kNumPinyinBooleanPrefs; ++i) {
    pinyin_boolean_checkboxes_[i] = new views::Checkbox(
        UTF16ToWide(l10n_util::GetStringUTF16(
            language_prefs::kPinyinBooleanPrefs[i].message_id)));
    pinyin_boolean_checkboxes_[i]->set_listener(this);
    pinyin_boolean_checkboxes_[i]->set_tag(i);
  }
  double_pinyin_schema_.combobox =
      new LanguageCombobox(double_pinyin_schema_.combobox_model);
  double_pinyin_schema_.combobox->set_listener(this);

  NotifyPrefChanged();
  for (size_t i = 0; i < language_prefs::kNumPinyinBooleanPrefs; ++i) {
    layout->StartRow(0, kColumnSetId);
    layout->AddView(pinyin_boolean_checkboxes_[i]);
  }
  layout->StartRow(0, kColumnSetId);
  layout->AddView(
      new views::Label(double_pinyin_schema_.combobox_model->GetLabel()));
  layout->AddView(double_pinyin_schema_.combobox);
}

void LanguagePinyinConfigView::Observe(NotificationType type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED) {
    NotifyPrefChanged();
  }
}

void LanguagePinyinConfigView::NotifyPrefChanged() {
  for (size_t i = 0; i < language_prefs::kNumPinyinBooleanPrefs; ++i) {
    const bool checked = pinyin_boolean_prefs_[i].GetValue();
    pinyin_boolean_checkboxes_[i]->SetChecked(checked);
  }
  const int value = double_pinyin_schema_.multiple_choice_pref.GetValue();
  for (int i = 0; i < double_pinyin_schema_.combobox_model->num_items(); ++i) {
    if (double_pinyin_schema_.combobox_model->GetConfigValueAt(i) == value) {
      double_pinyin_schema_.combobox->SetSelectedItem(i);
      break;
    }
  }
}

}  // namespace chromeos
