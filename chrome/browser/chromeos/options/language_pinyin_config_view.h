// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_PINYIN_CONFIG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_PINYIN_CONFIG_VIEW_H_

#include <string>

#include "chrome/browser/chromeos/cros/input_method_library.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/pref_member.h"
#include "chrome/browser/views/options/options_page_view.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/label.h"
#include "views/window/dialog_delegate.h"

namespace chromeos {

class LanguageCombobox;
template <typename DataType>
class LanguageComboboxModel;

// A dialog box for showing Traditional Chinese (Pinyin) input method
// preferences.
class LanguagePinyinConfigView : public views::ButtonListener,
                                 public views::Combobox::Listener,
                                 public views::DialogDelegate,
                                 public OptionsPageView {
 public:
  explicit LanguagePinyinConfigView(Profile* profile);
  virtual ~LanguagePinyinConfigView();

  // views::ButtonListener overrides.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // views::Combobox::Listener overrides.
  virtual void ItemChanged(views::Combobox* sender,
                           int prev_index,
                           int new_index);

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
  // Updates the pinyin checkboxes.
  void NotifyPrefChanged();

  BooleanPrefMember pinyin_boolean_prefs_[kNumPinyinBooleanPrefs];
  // TODO(yusukes): Support integer prefs if needed.
  views::View* contents_;

  // A checkboxes for Pinyin.
  views::Checkbox* pinyin_boolean_checkboxes_[kNumPinyinBooleanPrefs];

  struct DoublePinyinSchemaPrefAndAssociatedCombobox {
    IntegerPrefMember multiple_choice_pref;
    LanguageComboboxModel<int>* combobox_model;
    LanguageCombobox* combobox;
  } double_pinyin_schema_;

  DISALLOW_COPY_AND_ASSIGN(LanguagePinyinConfigView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_PINYIN_CONFIG_VIEW_H_
