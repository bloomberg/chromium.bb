// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings_contents_view.h"

#include <map>
#include <string>
#include <vector>

#include "app/combobox_model.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/basictypes.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/network_library.h"
#include "chrome/browser/chromeos/password_dialog_view.h"
#include "chrome/common/pref_member.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "unicode/strenum.h"
#include "unicode/timezone.h"
#include "views/background.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/slider/slider.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

using views::GridLayout;
using views::ColumnSet;

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// SettingsContentsSection

// Base section class settings
class SettingsContentsSection : public OptionsPageView {
 public:
  explicit SettingsContentsSection(Profile* profile, int title_msg_id);
  virtual ~SettingsContentsSection() {}

 protected:
  // OptionsPageView overrides:
  virtual void InitControlLayout();
  virtual void InitContents(GridLayout* layout) = 0;

  int single_column_view_set_id() const { return single_column_view_set_id_; }
  int double_column_view_set_id() const { return double_column_view_set_id_; }

 private:
  // The message id for the title of this section.
  int title_msg_id_;

  int single_column_view_set_id_;
  int double_column_view_set_id_;

  DISALLOW_COPY_AND_ASSIGN(SettingsContentsSection);
};

SettingsContentsSection::SettingsContentsSection(Profile* profile,
                                                 int title_msg_id)
    : OptionsPageView(profile),
      title_msg_id_(title_msg_id),
      single_column_view_set_id_(0),
      double_column_view_set_id_(1) {
}

void SettingsContentsSection::InitControlLayout() {
  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);

  int single_column_layout_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_layout_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 0,
                        GridLayout::USE_PREF, 0, 0);
  int inset_column_layout_id = 1;
  column_set = layout->AddColumnSet(inset_column_layout_id);
  column_set->AddPaddingColumn(0, kUnrelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::LEADING, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_layout_id);
  views::Label* title_label = new views::Label(
      l10n_util::GetString(title_msg_id_));
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font title_font =
      rb.GetFont(ResourceBundle::BaseFont).DeriveFont(0, gfx::Font::BOLD);
  title_label->SetFont(title_font);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, inset_column_layout_id);

  views::View* contents = new views::View;
  GridLayout* child_layout = new GridLayout(contents);
  contents->SetLayoutManager(child_layout);

  column_set = child_layout->AddColumnSet(single_column_view_set_id_);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  column_set = child_layout->AddColumnSet(double_column_view_set_id_);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  InitContents(child_layout);
  layout->AddView(contents);
}

////////////////////////////////////////////////////////////////////////////////
// DateTimeSection

// Date/Time section for datetime settings
class DateTimeSection : public SettingsContentsSection,
                        public views::Combobox::Listener {
 public:
  explicit DateTimeSection(Profile* profile);
  virtual ~DateTimeSection() {}

  // Overridden from views::Combobox::Listener:
  virtual void ItemChanged(views::Combobox* sender,
                           int prev_index,
                           int new_index);

 protected:
  // SettingsContentsSection overrides:
  virtual void InitContents(GridLayout* layout);
  virtual void NotifyPrefChanged(const std::wstring* pref_name);

 private:
  // The combobox model for the list of timezones.
  class TimezoneComboboxModel : public ComboboxModel {
   public:
    TimezoneComboboxModel() {
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("US/Pacific")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("US/Mountain")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("US/Central")));
      timezones_.push_back(icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8("US/Eastern")));
      // For now, add all the GMT timezones.
      // We may eventually want to use icu::TimeZone::createEnumeration()
      // to list all the timezones and pick the ones we want to show.
      for (int i = 11; i >= -12; i--) {
        // For positive i's, add the extra '+'.
        std::string str = StringPrintf(i > 0 ? "Etc/GMT+%d" : "Etc/GMT%d", i);
        icu::TimeZone* timezone = icu::TimeZone::createTimeZone(
            icu::UnicodeString::fromUTF8(str.c_str()));
        timezones_.push_back(timezone);
      }
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
      return output;
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
    : SettingsContentsSection(profile,
                              IDS_OPTIONS_SETTINGS_SECTION_TITLE_DATETIME),
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
// NetworkSection

// Network section for wifi settings
class NetworkSection : public SettingsContentsSection,
                       public views::Combobox::Listener,
                       public PasswordDialogDelegate,
                       public NetworkLibrary::Observer {
 public:
  explicit NetworkSection(Profile* profile);
  virtual ~NetworkSection();

  // Overridden from views::Combobox::Listener:
  virtual void ItemChanged(views::Combobox* sender,
                           int prev_index,
                           int new_index);

  // PasswordDialogDelegate implementation.
  virtual bool OnPasswordDialogCancel();
  virtual bool OnPasswordDialogAccept(const std::string& ssid,
                                      const string16& password);

  // NetworkLibrary::Observer implementation.
  virtual void NetworkChanged(NetworkLibrary* obj);
  virtual void NetworkTraffic(NetworkLibrary* obj, int traffic_type) {}

 protected:
  // SettingsContentsSection overrides:
  virtual void InitContents(GridLayout* layout);

 private:
  // The Combobox model for the list of wifi networks
  class WifiNetworkComboModel : public ComboboxModel {
   public:
    WifiNetworkComboModel() {
      wifi_networks_ = NetworkLibrary::Get()->wifi_networks();
    }

    virtual int GetItemCount() {
      // Item count is always 1 more than number of networks.
      // If there are no networks, then we show a message stating that.
      // If there are networks, the first item is empty.
      return static_cast<int>(wifi_networks_.size()) + 1;
    }

    virtual std::wstring GetItemAt(int index) {
      if (index == 0) {
        return wifi_networks_.empty() ?
            l10n_util::GetString(IDS_STATUSBAR_NO_NETWORKS_MESSAGE) :
            std::wstring();
      }
      // If index is greater than one, we get the corresponding network,
      // which is in the wifi_networks_ list in [index-1] position.
      return ASCIIToWide(wifi_networks_[index-1].ssid);
    }

    virtual bool HasWifiNetworks() {
      return !wifi_networks_.empty();
    }

    virtual const WifiNetwork& GetWifiNetworkAt(int index) {
      return wifi_networks_[index-1];
    }

   private:
    WifiNetworkVector wifi_networks_;

    DISALLOW_COPY_AND_ASSIGN(WifiNetworkComboModel);
  };

  // This method will change the combobox selection to the passed in wifi ssid.
  void SelectWifi(const std::string& wifi_ssid);

  // Controls for this section:
  views::Combobox* wifi_ssid_combobox_;

  // Used to store the index (in combobox) of the currently connected wifi.
  int last_selected_wifi_ssid_index_;

  // The activated wifi network.
  WifiNetwork activated_wifi_network_;

  // The wifi ssid models.
  WifiNetworkComboModel wifi_ssid_model_;

  DISALLOW_COPY_AND_ASSIGN(NetworkSection);
};

NetworkSection::NetworkSection(Profile* profile)
    : SettingsContentsSection(profile,
                              IDS_OPTIONS_SETTINGS_SECTION_TITLE_NETWORK),
      wifi_ssid_combobox_(NULL),
      last_selected_wifi_ssid_index_(0) {
  NetworkLibrary::Get()->AddObserver(this);
}

NetworkSection::~NetworkSection() {
  NetworkLibrary::Get()->RemoveObserver(this);
}

void NetworkSection::ItemChanged(views::Combobox* sender,
                                 int prev_index,
                                 int new_index) {
  if (new_index == prev_index)
    return;

  // Don't allow switching to the first item (which is empty).
  if (new_index == 0) {
    wifi_ssid_combobox_->SetSelectedItem(prev_index);
    return;
  }

  if (!wifi_ssid_model_.HasWifiNetworks())
    return;

  last_selected_wifi_ssid_index_ = prev_index;
  activated_wifi_network_ = wifi_ssid_model_.GetWifiNetworkAt(new_index);
  // Connect to wifi here. Open password page if appropriate.
  if (activated_wifi_network_.encrypted) {
    views::Window* window = views::Window::CreateChromeWindow(
        NULL,
        gfx::Rect(),
        new PasswordDialogView(this, activated_wifi_network_.ssid));
    window->SetIsAlwaysOnTop(true);
    window->Show();
  } else {
    NetworkLibrary::Get()->ConnectToWifiNetwork(activated_wifi_network_,
                                                    string16());
  }
}

bool NetworkSection::OnPasswordDialogCancel() {
  // Change combobox to previous setting.
  wifi_ssid_combobox_->SetSelectedItem(last_selected_wifi_ssid_index_);
  return true;
}

bool NetworkSection::OnPasswordDialogAccept(const std::string& ssid,
                                            const string16& password) {
  NetworkLibrary::Get()->ConnectToWifiNetwork(activated_wifi_network_,
                                                  password);
  return true;
}

void NetworkSection::NetworkChanged(NetworkLibrary* obj) {
  SelectWifi(obj->wifi_ssid());
}

void NetworkSection::InitContents(GridLayout* layout) {
  wifi_ssid_combobox_ = new views::Combobox(&wifi_ssid_model_);
  wifi_ssid_combobox_->SetSelectedItem(last_selected_wifi_ssid_index_);
  wifi_ssid_combobox_->set_listener(this);

  layout->StartRow(0, single_column_view_set_id());
  layout->AddView(wifi_ssid_combobox_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Select the initial connected wifi network.
  SelectWifi(NetworkLibrary::Get()->wifi_ssid());
}

void NetworkSection::SelectWifi(const std::string& wifi_ssid) {
  if (wifi_ssid_model_.HasWifiNetworks() && wifi_ssid_combobox_) {
    // If we are not connected to any wifi network, wifi_ssid will be empty.
    // So we select the first item.
    if (wifi_ssid.empty()) {
      wifi_ssid_combobox_->SetSelectedItem(0);
    } else {
      // Loop through the list and select the ssid that matches.
      for (int i = 1; i < wifi_ssid_model_.GetItemCount(); i++) {
        if (wifi_ssid_model_.GetWifiNetworkAt(i).ssid == wifi_ssid) {
          last_selected_wifi_ssid_index_ = i;
          wifi_ssid_combobox_->SetSelectedItem(i);
          return;
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// TouchpadSection

class TouchpadSection : public SettingsContentsSection,
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
  // SettingsContentsSection overrides:
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
    : SettingsContentsSection(profile,
                              IDS_OPTIONS_SETTINGS_SECTION_TITLE_TOUCHPAD),
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
// SettingsContentsView

////////////////////////////////////////////////////////////////////////////////
// SettingsContentsView, OptionsPageView implementation:

void SettingsContentsView::InitControlLayout() {
  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(new DateTimeSection(profile()));
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  // TODO(chocobo): Add NetworkSection back once we have the UI finalized.
//  layout->StartRow(0, single_column_view_set_id);
//  layout->AddView(new NetworkSection(profile()));
//  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(new TouchpadSection(profile()));
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
}

}  // namespace chromeos
