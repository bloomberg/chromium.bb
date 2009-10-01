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
#include "base/string_util.h"
#include "chrome/browser/chromeos/password_dialog_view.h"
#include "chrome/common/pref_member.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "views/background.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/slider/slider.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

using views::GridLayout;
using views::ColumnSet;

namespace {

////////////////////////////////////////////////////////////////////////////////
// WifiSSIDComboModel

// The Combobox model for the list of wifi networks
class WifiSSIDComboModel : public ComboboxModel {
 public:
  struct NetworkData {
    NetworkData() { }
    NetworkData(const string16& encryption, const int& strength)
        : encryption(encryption),
          strength(strength) { }

    string16 encryption;
    int strength;
  };
  typedef std::map<std::string, NetworkData> NetworkDataMap;

  WifiSSIDComboModel();

  virtual int GetItemCount();
  virtual std::wstring GetItemAt(int index);

  const std::string& GetSSIDAt(int index);
  bool RequiresPassword(const std::string& ssid);

 private:
  std::vector<std::string> ssids_;

  // A map of some extra data (NetworkData) keyed off the ssids.
  NetworkDataMap ssids_map_;

  void AddWifiNetwork(const std::string& ssid,
                      const string16& encryption,
                      int strength);

  DISALLOW_COPY_AND_ASSIGN(WifiSSIDComboModel);
};

WifiSSIDComboModel::WifiSSIDComboModel() {
  // TODO(chocobo): Load wifi info from conman.
  // This is just temporary data until we hook this up to real data.
  AddWifiNetwork("Wifi Combobox Mock", string16(), 80);
  AddWifiNetwork("Wifi WPA-PSK Password is chronos",
                 ASCIIToUTF16("WPA-PSK"), 60);
  AddWifiNetwork("Wifi No Encryption", string16(), 90);
}

int WifiSSIDComboModel::GetItemCount() {
  return static_cast<int>(ssids_.size());
}

std::wstring WifiSSIDComboModel::GetItemAt(int index) {
  DCHECK(static_cast<int>(ssids_.size()) > index);

  NetworkDataMap::const_iterator it = ssids_map_.find(ssids_[index]);
  DCHECK(it != ssids_map_.end());

  // TODO(chocobo): Finalize UI, then put strings in resource file.
  std::vector<string16> subst;
  subst.push_back(ASCIIToUTF16(it->first));  // $1
  // The "None" string is just temporary for now. Have not finalized the UI yet.
  if (it->second.encryption.empty())
    subst.push_back(ASCIIToUTF16("None"));  // $2
  else
    subst.push_back(it->second.encryption);  // $2
  subst.push_back(IntToString16(it->second.strength));  // $3

  return UTF16ToWide(
      ReplaceStringPlaceholders(ASCIIToUTF16("$1 ($2, $3)"), subst, NULL));
}

const std::string& WifiSSIDComboModel::GetSSIDAt(int index) {
  DCHECK(static_cast<int>(ssids_.size()) > index);
  return ssids_[index];
}

bool WifiSSIDComboModel::RequiresPassword(const std::string& ssid) {
  NetworkDataMap::const_iterator it = ssids_map_.find(ssid);
  DCHECK(it != ssids_map_.end());
  return !it->second.encryption.empty();
}

void WifiSSIDComboModel::AddWifiNetwork(const std::string& ssid,
                                        const string16& encryption,
                                        int strength) {
  ssids_.push_back(ssid);
  ssids_map_[ssid] = NetworkData(encryption, strength);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkSection

// Network section for wifi settings
class NetworkSection : public OptionsPageView,
                       public views::Combobox::Listener,
                       public PasswordDialogDelegate {
 public:
  explicit NetworkSection(Profile* profile);
  virtual ~NetworkSection() {}

  // Overridden from views::Combobox::Listener:
  virtual void ItemChanged(views::Combobox* sender,
                           int prev_index,
                           int new_index);

  // PasswordDialogDelegate implementation.
  virtual bool OnPasswordDialogCancel();
  virtual bool OnPasswordDialogAccept(const std::string& ssid,
                                      const string16& password);

  bool ConnectToWifi(const std::string& ssid, const string16& password);

 protected:
  // OptionsPageView overrides:
  virtual void InitControlLayout();
  virtual void InitContents();

 private:
  // The View that contains the contents of the section.
  views::View* contents_;

  // Controls for this section:
  views::Combobox* wifi_ssid_combobox_;

  // Used to store the index (in combobox) of the currently connected wifi.
  int last_selected_wifi_ssid_index_;

  // Dummy for now. Used to populate wifi ssid models.
  WifiSSIDComboModel wifi_ssid_model_;

  DISALLOW_COPY_AND_ASSIGN(NetworkSection);
};

////////////////////////////////////////////////////////////////////////////////
// NetworkSection

NetworkSection::NetworkSection(Profile* profile)
    : OptionsPageView(profile),
      contents_(NULL),
      wifi_ssid_combobox_(NULL),
      last_selected_wifi_ssid_index_(0) {
}

void NetworkSection::ItemChanged(views::Combobox* sender,
                                 int prev_index,
                                 int new_index) {
  if (new_index == prev_index)
    return;
  if (sender == wifi_ssid_combobox_) {
    last_selected_wifi_ssid_index_ = prev_index;
    std::string ssid = wifi_ssid_model_.GetSSIDAt(new_index);
    // Connect to wifi here. Open password page if appropriate
    if (wifi_ssid_model_.RequiresPassword(ssid)) {
      views::Window* window = views::Window::CreateChromeWindow(
          NULL,
          gfx::Rect(),
          new PasswordDialogView(this, ssid));
      window->SetIsAlwaysOnTop(true);
      window->Show();
    } else {
      ConnectToWifi(ssid, string16());
    }
  }
}

bool NetworkSection::OnPasswordDialogCancel() {
  // Change combobox to previous setting
  wifi_ssid_combobox_->SetSelectedItem(last_selected_wifi_ssid_index_);
  return true;
}

bool NetworkSection::OnPasswordDialogAccept(const std::string& ssid,
                                            const string16& password) {
  // Try connecting to wifi
  return ConnectToWifi(ssid, password);
}

bool NetworkSection::ConnectToWifi(const std::string& ssid,
                                   const string16& password) {
  // TODO(chocobo): Connect to wifi
  return password == ASCIIToUTF16("chronos");
}

void NetworkSection::InitControlLayout() {
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
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_SECTION_TITLE_NETWORK));
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font title_font =
      rb.GetFont(ResourceBundle::BaseFont).DeriveFont(0, gfx::Font::BOLD);
  title_label->SetFont(title_font);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, inset_column_layout_id);
  InitContents();
  layout->AddView(contents_);
}

void NetworkSection::InitContents() {
  contents_ = new views::View;
  GridLayout* layout = new GridLayout(contents_);
  contents_->SetLayoutManager(layout);

  wifi_ssid_combobox_ = new views::Combobox(&wifi_ssid_model_);
  wifi_ssid_combobox_->set_listener(this);

  int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(wifi_ssid_combobox_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
}

////////////////////////////////////////////////////////////////////////////////
// TouchpadSection

class TouchpadSection : public OptionsPageView,
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
  // OptionsPageView overrides:
  virtual void InitControlLayout();
  virtual void InitContents();
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
    : OptionsPageView(profile),
      contents_(NULL),
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
                                L"Options_TapToClickCheckbox_Enable" :
                                L"Options_TapToClickCheckbox_Disable",
                            profile()->GetPrefs());
    tap_to_click_enabled_.SetValue(enabled);
  } else if (sender == enable_vert_edge_scroll_checkbox_) {
    bool enabled = enable_vert_edge_scroll_checkbox_->checked();
    UserMetricsRecordAction(enabled ?
                                L"Options_VertEdgeScrollCheckbox_Enable" :
                                L"Options_VertEdgeScrollCheckbox_Disable",
                            profile()->GetPrefs());
    vert_edge_scroll_enabled_.SetValue(enabled);
  }
}

void TouchpadSection::SliderValueChanged(views::Slider* sender) {
  if (sender == speed_factor_slider_) {
    double value = speed_factor_slider_->value();
    UserMetricsRecordAction(L"Options_SpeedFactorSlider_Changed",
                            profile()->GetPrefs());
    speed_factor_.SetValue(value);
  } else if (sender == sensitivity_slider_) {
    double value = sensitivity_slider_->value();
    UserMetricsRecordAction(L"Options_SensitivitySlider_Changed",
                            profile()->GetPrefs());
    sensitivity_.SetValue(value);
  }
}

void TouchpadSection::InitControlLayout() {
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
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_SECTION_TITLE_TOUCHPAD));
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font title_font =
      rb.GetFont(ResourceBundle::BaseFont).DeriveFont(0, gfx::Font::BOLD);
  title_label->SetFont(title_font);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, inset_column_layout_id);
  InitContents();
  layout->AddView(contents_);
}

void TouchpadSection::InitContents() {
  contents_ = new views::View;
  GridLayout* layout = new GridLayout(contents_);
  contents_->SetLayoutManager(layout);

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

  int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  int double_column_view_set_id = 1;
  column_set = layout->AddColumnSet(double_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, double_column_view_set_id);
  layout->AddView(new views::Label(
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_SENSITIVITY_DESCRIPTION)));
  layout->AddView(sensitivity_slider_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, double_column_view_set_id);
  layout->AddView(new views::Label(
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_SPEED_FACTOR_DESCRIPTION)));
  layout->AddView(speed_factor_slider_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(enable_tap_to_click_checkbox_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
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

}  // namespace

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

  // TODO(chocobo): Add NetworkSection back in when we finalized the UI.
//  layout->StartRow(0, single_column_view_set_id);
//  layout->AddView(new NetworkSection(profile()));
//  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(new TouchpadSection(profile()));
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
}
