// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/system_page_view.h"

#include <string>
#include <vector>

#include "app/combobox_model.h"
#include "base/stl_util-inl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/options/language_config_view.h"
#include "chrome/browser/pref_member.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "unicode/timezone.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/button/native_button.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/slider/slider.h"
#include "views/window/window.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// DateTimeSection

// Date/Time section for datetime settings
class DateTimeSection : public SettingsPageSection,
                        public views::Combobox::Listener {
 public:
  explicit DateTimeSection(Profile* profile);
  virtual ~DateTimeSection() {}

  // Overridden from views::Combobox::Listener:
  virtual void ItemChanged(views::Combobox* sender,
                           int prev_index,
                           int new_index);

 protected:
  // SettingsPageSection overrides:
  virtual void InitContents(GridLayout* layout);
  virtual void NotifyPrefChanged(const std::wstring* pref_name);

 private:
  // The combobox model for the list of timezones.
  class TimezoneComboboxModel : public ComboboxModel {
   public:
    TimezoneComboboxModel() {
      // TODO(chocobo): For now, add all the GMT timezones.
      // We may eventually want to use icu::TimeZone::createEnumeration()
      // to list all the timezones and pick the ones we want to show.
      // NOTE: This currently does not handle daylight savings properly
      // b/c this is just a manually selected list of timezones that
      // happen to span the GMT-11 to GMT+12 Today. When daylight savings
      // kick in, this list might have more than one timezone in the same
      // GMT bucket.
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Pacific/Samoa")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("US/Hawaii")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("US/Alaska")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("US/Pacific")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("US/Mountain")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("US/Central")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("US/Eastern")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("America/Santiago")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("America/Sao_Paulo")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Atlantic/South_Georgia")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Atlantic/Cape_Verde")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Europe/London")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Europe/Rome")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Europe/Helsinki")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Europe/Moscow")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Asia/Dubai")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Asia/Karachi")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Asia/Dhaka")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Asia/Bangkok")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Asia/Hong_Kong")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Asia/Tokyo")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Australia/Sydney")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Asia/Magadan")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("Pacific/Auckland")));
    }

    virtual ~TimezoneComboboxModel() {
      STLDeleteElements(&timezones_);
    }

    virtual int GetItemCount() {
      return static_cast<int>(timezones_.size());
    }

    virtual std::wstring GetItemAt(int index) {
      icu::UnicodeString name;
      timezones_[index]->getDisplayName(name);
      std::wstring output;
      UTF16ToWide(name.getBuffer(), name.length(), &output);
      int hour_offset = timezones_[index]->getRawOffset() / 3600000;
      return StringPrintf(hour_offset == 0 ? L"(GMT) " : (hour_offset > 0 ?
          L"(GMT+%d) " : L"(GMT%d) "), hour_offset) + output;
    }

    virtual std::wstring GetTimeZoneIDAt(int index) {
      icu::UnicodeString id;
      timezones_[index]->getID(id);
      std::wstring output;
      UTF16ToWide(id.getBuffer(), id.length(), &output);
      return output;
    }

   private:
    std::vector<icu::TimeZone*> timezones_;

    DISALLOW_COPY_AND_ASSIGN(TimezoneComboboxModel);
  };

  // Selects the timezone.
  void SelectTimeZone(const std::wstring& id);

  // TimeZone combobox model.
  views::Combobox* timezone_combobox_;

  // Controls for this section:
  TimezoneComboboxModel timezone_combobox_model_;

  // Preferences for this section:
  StringPrefMember timezone_;

  DISALLOW_COPY_AND_ASSIGN(DateTimeSection);
};

DateTimeSection::DateTimeSection(Profile* profile)
    : SettingsPageSection(profile, IDS_OPTIONS_SETTINGS_SECTION_TITLE_DATETIME),
      timezone_combobox_(NULL) {
}

void DateTimeSection::ItemChanged(views::Combobox* sender,
                                  int prev_index,
                                  int new_index) {
  if (new_index == prev_index)
    return;
  timezone_.SetValue(timezone_combobox_model_.GetTimeZoneIDAt(new_index));
}

void DateTimeSection::InitContents(GridLayout* layout) {
  timezone_combobox_ = new views::Combobox(&timezone_combobox_model_);
  timezone_combobox_->set_listener(this);

  layout->StartRow(0, double_column_view_set_id());
  layout->AddView(new views::Label(
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_TIMEZONE_DESCRIPTION)));
  layout->AddView(timezone_combobox_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Init member prefs so we can update the controls if prefs change.
  timezone_.Init(prefs::kTimeZone, profile()->GetPrefs(), this);
}

void DateTimeSection::NotifyPrefChanged(const std::wstring* pref_name) {
  if (!pref_name || *pref_name == prefs::kTimeZone) {
    std::wstring timezone = timezone_.GetValue();
    SelectTimeZone(timezone);
  }
}

void DateTimeSection::SelectTimeZone(const std::wstring& id) {
  for (int i = 0; i < timezone_combobox_model_.GetItemCount(); i++) {
    if (timezone_combobox_model_.GetTimeZoneIDAt(i) == id) {
      timezone_combobox_->SetSelectedItem(i);
      return;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// TouchpadSection

class TouchpadSection : public SettingsPageSection,
                        public views::ButtonListener,
                        public views::SliderListener {
 public:
  explicit TouchpadSection(Profile* profile);
  virtual ~TouchpadSection() {}

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overridden from views::SliderListener:
  virtual void SliderValueChanged(views::Slider* sender);

 protected:
  // SettingsPageSection overrides:
  virtual void InitContents(GridLayout* layout);
  virtual void NotifyPrefChanged(const std::wstring* pref_name);

 private:
  // The View that contains the contents of the section.
  views::View* contents_;

  // Controls for this section:
  views::Checkbox* enable_tap_to_click_checkbox_;
  views::Checkbox* enable_vert_edge_scroll_checkbox_;
  views::Slider* speed_factor_slider_;
  views::Slider* sensitivity_slider_;

  // Preferences for this section:
  BooleanPrefMember tap_to_click_enabled_;
  BooleanPrefMember vert_edge_scroll_enabled_;
  IntegerPrefMember speed_factor_;
  IntegerPrefMember sensitivity_;

  DISALLOW_COPY_AND_ASSIGN(TouchpadSection);
};

TouchpadSection::TouchpadSection(Profile* profile)
    : SettingsPageSection(profile, IDS_OPTIONS_SETTINGS_SECTION_TITLE_TOUCHPAD),
      enable_tap_to_click_checkbox_(NULL),
      enable_vert_edge_scroll_checkbox_(NULL),
      speed_factor_slider_(NULL),
      sensitivity_slider_(NULL) {
}

void TouchpadSection::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == enable_tap_to_click_checkbox_) {
    bool enabled = enable_tap_to_click_checkbox_->checked();
    UserMetricsRecordAction(enabled ?
                                "Options_TapToClickCheckbox_Enable" :
                                "Options_TapToClickCheckbox_Disable",
                            profile()->GetPrefs());
    tap_to_click_enabled_.SetValue(enabled);
  } else if (sender == enable_vert_edge_scroll_checkbox_) {
    bool enabled = enable_vert_edge_scroll_checkbox_->checked();
    UserMetricsRecordAction(enabled ?
                                "Options_VertEdgeScrollCheckbox_Enable" :
                                "Options_VertEdgeScrollCheckbox_Disable",
                            profile()->GetPrefs());
    vert_edge_scroll_enabled_.SetValue(enabled);
  }
}

void TouchpadSection::SliderValueChanged(views::Slider* sender) {
  if (sender == speed_factor_slider_) {
    double value = speed_factor_slider_->value();
    UserMetricsRecordAction("Options_SpeedFactorSlider_Changed",
                            profile()->GetPrefs());
    speed_factor_.SetValue(value);
  } else if (sender == sensitivity_slider_) {
    double value = sensitivity_slider_->value();
    UserMetricsRecordAction("Options_SensitivitySlider_Changed",
                            profile()->GetPrefs());
    sensitivity_.SetValue(value);
  }
}

void TouchpadSection::InitContents(GridLayout* layout) {
  enable_tap_to_click_checkbox_ = new views::Checkbox(l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_TAP_TO_CLICK_ENABLED_DESCRIPTION));
  enable_tap_to_click_checkbox_->set_listener(this);
  enable_tap_to_click_checkbox_->SetMultiLine(true);
  enable_vert_edge_scroll_checkbox_ = new views::Checkbox(l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_VERT_EDGE_SCROLL_ENABLED_DESCRIPTION));
  enable_vert_edge_scroll_checkbox_->set_listener(this);
  enable_vert_edge_scroll_checkbox_->SetMultiLine(true);
  // Create speed factor slider with values between 1 and 10 step 1
  speed_factor_slider_ = new views::Slider(1, 10, 1,
      static_cast<views::Slider::StyleFlags>(
          views::Slider::STYLE_DRAW_VALUE |
          views::Slider::STYLE_UPDATE_ON_RELEASE),
      this);
  // Create sensitivity slider with values between 1 and 10 step 1
  sensitivity_slider_ = new views::Slider(1, 10, 1,
      static_cast<views::Slider::StyleFlags>(
          views::Slider::STYLE_DRAW_VALUE |
          views::Slider::STYLE_UPDATE_ON_RELEASE),
      this);

  layout->StartRow(0, double_column_view_set_id());
  layout->AddView(new views::Label(
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_SENSITIVITY_DESCRIPTION)));
  layout->AddView(sensitivity_slider_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, double_column_view_set_id());
  layout->AddView(new views::Label(
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_SPEED_FACTOR_DESCRIPTION)));
  layout->AddView(speed_factor_slider_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id());
  layout->AddView(enable_tap_to_click_checkbox_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id());
  layout->AddView(enable_vert_edge_scroll_checkbox_);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);

  // Init member prefs so we can update the controls if prefs change.
  tap_to_click_enabled_.Init(prefs::kTapToClickEnabled,
                             profile()->GetPrefs(), this);
  vert_edge_scroll_enabled_.Init(prefs::kVertEdgeScrollEnabled,
                                 profile()->GetPrefs(), this);
  speed_factor_.Init(prefs::kTouchpadSpeedFactor,
                     profile()->GetPrefs(), this);
  sensitivity_.Init(prefs::kTouchpadSensitivity,
                    profile()->GetPrefs(), this);
}

void TouchpadSection::NotifyPrefChanged(const std::wstring* pref_name) {
  if (!pref_name || *pref_name == prefs::kTapToClickEnabled) {
    bool enabled =  tap_to_click_enabled_.GetValue();
    enable_tap_to_click_checkbox_->SetChecked(enabled);
  }
  if (!pref_name || *pref_name == prefs::kVertEdgeScrollEnabled) {
    bool enabled =  vert_edge_scroll_enabled_.GetValue();
    enable_vert_edge_scroll_checkbox_->SetChecked(enabled);
  }
  if (!pref_name || *pref_name == prefs::kTouchpadSpeedFactor) {
    double value =  speed_factor_.GetValue();
    speed_factor_slider_->SetValue(value);
  }
  if (!pref_name || *pref_name == prefs::kTouchpadSensitivity) {
    double value =  sensitivity_.GetValue();
    sensitivity_slider_->SetValue(value);
  }
}

////////////////////////////////////////////////////////////////////////////////
// LanguageSection

// TextInput section for text input settings.
class LanguageSection : public SettingsPageSection,
                        public views::ButtonListener {
 public:
  explicit LanguageSection(Profile* profile);
  virtual ~LanguageSection() {}

 private:
  // Overridden from SettingsPageSection:
  virtual void InitContents(GridLayout* layout);

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event);

  views::NativeButton* customize_languages_button_;

  DISALLOW_COPY_AND_ASSIGN(LanguageSection);
};

LanguageSection::LanguageSection(Profile* profile)
    : SettingsPageSection(profile,
                          IDS_OPTIONS_SETTINGS_SECTION_TITLE_LANGUAGE) {
}

void LanguageSection::InitContents(GridLayout* layout) {
  // Add the customize button.
  layout->StartRow(0, single_column_view_set_id());
  customize_languages_button_ = new views::NativeButton(
      this,
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_CUSTOMIZE));
  layout->AddView(customize_languages_button_, 1, 1,
                  GridLayout::LEADING, GridLayout::CENTER);
}

void LanguageSection::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == customize_languages_button_) {
    views::Window* window = views::Window::CreateChromeWindow(
        NULL,
        gfx::Rect(),
        new LanguageConfigView());
    window->SetIsAlwaysOnTop(true);
    window->Show();
  }
}

////////////////////////////////////////////////////////////////////////////////
// SystemPageView

////////////////////////////////////////////////////////////////////////////////
// SystemPageView, SettingsPageView implementation:

void SystemPageView::InitControlLayout() {
  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(new DateTimeSection(profile()));
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(new TouchpadSection(profile()));
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(new LanguageSection(profile()));
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
}

}  // namespace chromeos
