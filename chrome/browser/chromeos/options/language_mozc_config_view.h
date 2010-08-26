// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_MOZC_CONFIG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_MOZC_CONFIG_VIEW_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/cros/input_method_library.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/views/options/options_page_view.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/label.h"
#include "views/controls/slider/slider.h"
#include "views/window/dialog_delegate.h"

namespace views {
class Button;
class Checkbox;
}

namespace chromeos {

class LanguageCombobox;
template <typename DataType>
class LanguageComboboxModel;

// A dialog box for showing Mozc (Japanese input method) preferences.
class LanguageMozcConfigView : public views::ButtonListener,
                               public views::Combobox::Listener,
                               public views::DialogDelegate,
                               public views::SliderListener,
                               public OptionsPageView {
 public:
  explicit LanguageMozcConfigView(Profile* profile);
  virtual ~LanguageMozcConfigView();

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
  // Updates the mozc keyboard combobox.
  void NotifyPrefChanged();

  // Resets all the preferences to the default values.
  void ResetToDefaults();

  views::View* contents_;

  views::Button* reset_to_defaults_button_;

  struct MozcPrefAndAssociatedCheckbox {
    BooleanPrefMember boolean_pref;
    views::Checkbox* checkbox;
  } prefs_and_checkboxes_[language_prefs::kNumMozcBooleanPrefs];

  struct MozcPrefAndAssociatedCombobox {
    StringPrefMember multiple_choice_pref;
    LanguageComboboxModel<const char*>* combobox_model;
    LanguageCombobox* combobox;
  } prefs_and_comboboxes_[language_prefs::kNumMozcMultipleChoicePrefs];

  struct MozcPrefAndAssociatedSlider {
    IntegerPrefMember integer_pref;
    views::Slider* slider;
  } prefs_and_sliders_[language_prefs::kNumMozcIntegerPrefs];

  DISALLOW_COPY_AND_ASSIGN(LanguageMozcConfigView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_MOZC_CONFIG_VIEW_H_
