// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_CHEWING_CONFIG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_CHEWING_CONFIG_VIEW_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/cros/input_method_library.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/views/options/options_page_view.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/label.h"
#include "views/controls/slider/slider.h"
#include "views/window/dialog_delegate.h"

namespace chromeos {

class LanguageCombobox;
template <typename DataType>
class LanguageComboboxModel;

// A dialog box for showing Traditional Chinese (Chewing) input method
// preferences.
class LanguageChewingConfigView : public views::ButtonListener,
                                  public views::Combobox::Listener,
                                  public views::DialogDelegate,
                                  public views::SliderListener,
                                  public OptionsPageView {
 public:
  explicit LanguageChewingConfigView(Profile* profile);
  virtual ~LanguageChewingConfigView();

  // views::ButtonListener overrides.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // views::Combobox::Listener overrides.
  virtual void ItemChanged(views::Combobox* sender,
                           int prev_index,
                           int new_index);

  // views::SliderListener overrides.
  virtual void SliderValueChanged(views::Slider* sender);

  // views::DialogDelegate overrides.
  virtual bool IsModal() const { return true; }
  virtual views::View* GetContentsView() { return this; }
  virtual int GetDialogButtons() const;
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;
  virtual std::wstring GetWindowTitle() const;

  // views::View overrides.
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

  // OptionsPageView overrides.
  virtual void InitControlLayout();

  // NotificationObserver overrides.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // Updates the chewing checkboxes.
  void NotifyPrefChanged();

  BooleanPrefMember chewing_boolean_prefs_[
      language_prefs::kNumChewingBooleanPrefs];
  IntegerPrefMember chewing_integer_prefs_[
      language_prefs::kNumChewingIntegerPrefs];
  views::View* contents_;

  // Checkboxes for Chewing.
  views::Checkbox* chewing_boolean_checkboxes_[
      language_prefs::kNumChewingBooleanPrefs];

  views::Slider* chewing_integer_sliders_[
      language_prefs::kNumChewingIntegerPrefs];

  struct ChewingPrefAndAssociatedCombobox {
    StringPrefMember multiple_choice_pref;
    LanguageComboboxModel<const char*>* combobox_model;
    LanguageCombobox* combobox;
  } prefs_and_comboboxes_[language_prefs::kNumChewingMultipleChoicePrefs];

  struct HsuSelKeyTypePrefAndAssociatedCombobox {
    IntegerPrefMember multiple_choice_pref;
    LanguageComboboxModel<int>* combobox_model;
    LanguageCombobox* combobox;
  } hsu_sel_key_type_;

  DISALLOW_COPY_AND_ASSIGN(LanguageChewingConfigView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_CHEWING_CONFIG_VIEW_H_
