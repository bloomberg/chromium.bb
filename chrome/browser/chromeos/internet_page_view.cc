// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/internet_page_view.h"

#include <string>

#include "app/combobox_model.h"
#include "chrome/browser/chromeos/network_library.h"
#include "chrome/browser/chromeos/password_dialog_view.h"
#include "grit/generated_resources.h"
#include "views/controls/button/native_button.h"
#include "views/controls/combobox/combobox.h"
#include "views/window/window.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// NetworkSection

// Network section for wifi settings
class NetworkSection : public SettingsPageSection,
                       public views::Combobox::Listener,
                       public views::ButtonListener,
                       public PasswordDialogDelegate,
                       public NetworkLibrary::Observer {
 public:
  explicit NetworkSection(Profile* profile);
  virtual ~NetworkSection();

  // Overridden from views::Combobox::Listener:
  virtual void ItemChanged(views::Combobox* sender,
                           int prev_index,
                           int new_index);

  // Overriden from views::Button::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // PasswordDialogDelegate implementation.
  virtual bool OnPasswordDialogCancel();
  virtual bool OnPasswordDialogAccept(const std::string& ssid,
                                      const string16& password);

  // NetworkLibrary::Observer implementation.
  virtual void NetworkChanged(NetworkLibrary* obj);
  virtual void NetworkTraffic(NetworkLibrary* obj, int traffic_type) {}

 protected:
  // SettingsPageSection overrides:
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

  // Status labels
  views::Label* ethernet_status_label_;
  views::Label* wifi_status_label_;
  views::Label* cellular_status_label_;

  // Options buttons
  views::NativeButton* ethernet_options_button_;
  views::NativeButton* wifi_options_button_;
  views::NativeButton* cellular_options_button_;

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
    : SettingsPageSection(profile, IDS_OPTIONS_SETTINGS_SECTION_TITLE_NETWORK),
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

void NetworkSection::ButtonPressed(views::Button* sender,
                                   const views::Event& event) {
  // TODO(chocobo): Open options dialog.
  if (sender == ethernet_options_button_) {
  } else if (sender == wifi_options_button_) {
  } else if (sender == cellular_options_button_) {
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
  // Ethernet status.
  int status;
  if (obj->ethernet_connecting())
    status = IDS_STATUSBAR_NETWORK_DEVICE_CONNECTING;
  else if (obj->ethernet_connected())
    status = IDS_STATUSBAR_NETWORK_DEVICE_CONNECTED;
  else if (obj->ethernet_enabled())
    status = IDS_STATUSBAR_NETWORK_DEVICE_DISCONNECTED;
  else
    status = IDS_STATUSBAR_NETWORK_DEVICE_DISABLED;
  ethernet_status_label_->SetText(l10n_util::GetString(status));

  // Wifi status.
  if (obj->wifi_connecting())
    status = IDS_STATUSBAR_NETWORK_DEVICE_CONNECTING;
  else if (obj->wifi_connected())
    status = IDS_STATUSBAR_NETWORK_DEVICE_CONNECTED;
  else if (obj->wifi_enabled())
    status = IDS_STATUSBAR_NETWORK_DEVICE_DISCONNECTED;
  else
    status = IDS_STATUSBAR_NETWORK_DEVICE_DISABLED;
  wifi_status_label_->SetText(l10n_util::GetString(status));

  // Cellular status.
  status = IDS_STATUSBAR_NETWORK_DEVICE_DISABLED;
  if (obj->cellular_connecting())
    status = IDS_STATUSBAR_NETWORK_DEVICE_CONNECTING;
  else if (obj->cellular_connected())
    status = IDS_STATUSBAR_NETWORK_DEVICE_CONNECTED;
  else if (obj->cellular_enabled())
    status = IDS_STATUSBAR_NETWORK_DEVICE_DISCONNECTED;
  else
    status = IDS_STATUSBAR_NETWORK_DEVICE_DISABLED;
  cellular_status_label_->SetText(l10n_util::GetString(status));

  // Select wifi combo box.
  SelectWifi(obj->wifi_ssid());
}

void NetworkSection::InitContents(GridLayout* layout) {
  int quad_column_view_set_id = 1;
  ColumnSet* column_set = layout->AddColumnSet(quad_column_view_set_id);
  // device
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  // fill padding
  column_set->AddPaddingColumn(10, 0);
  // selection
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  // padding
  column_set->AddPaddingColumn(0, 10);
  // options
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  // Ethernet
  layout->StartRow(0, quad_column_view_set_id);
  views::Label* label = new views::Label(l10n_util::GetString(
      IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET));
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->AddView(label);
  layout->SkipColumns(1);
  ethernet_options_button_ = new views::NativeButton(this,
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_OPTIONS));
  layout->AddView(ethernet_options_button_, 1, 2);

  layout->StartRow(0, quad_column_view_set_id);
  ethernet_status_label_ = new views::Label();
  ethernet_status_label_->SetFont(rb.GetFont(ResourceBundle::SmallFont));
  ethernet_status_label_->SetColor(SK_ColorLTGRAY);
  ethernet_status_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->AddView(ethernet_status_label_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Wifi
  layout->StartRow(0, quad_column_view_set_id);
  label = new views::Label(l10n_util::GetString(
      IDS_STATUSBAR_NETWORK_DEVICE_WIFI));
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->AddView(label);
  wifi_ssid_combobox_ = new views::Combobox(&wifi_ssid_model_);
  wifi_ssid_combobox_->SetSelectedItem(last_selected_wifi_ssid_index_);
  wifi_ssid_combobox_->set_listener(this);
  // Select the initial connected wifi network.
  layout->AddView(wifi_ssid_combobox_, 1, 2);
  wifi_options_button_ = new views::NativeButton(this,
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_OPTIONS));
  layout->AddView(wifi_options_button_, 1, 2);

  layout->StartRow(0, quad_column_view_set_id);
  wifi_status_label_ = new views::Label();
  wifi_status_label_->SetFont(rb.GetFont(ResourceBundle::SmallFont));
  wifi_status_label_->SetColor(SK_ColorLTGRAY);
  wifi_status_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->AddView(wifi_status_label_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Cellular
  layout->StartRow(0, quad_column_view_set_id);
  label = new views::Label(l10n_util::GetString(
      IDS_STATUSBAR_NETWORK_DEVICE_CELLULAR));
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->AddView(label);
  layout->SkipColumns(1);
  cellular_options_button_ = new views::NativeButton(this,
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_OPTIONS));
  layout->AddView(cellular_options_button_, 1, 2);

  layout->StartRow(0, quad_column_view_set_id);
  cellular_status_label_ = new views::Label();
  cellular_status_label_->SetFont(rb.GetFont(ResourceBundle::SmallFont));
  cellular_status_label_->SetColor(SK_ColorLTGRAY);
  cellular_status_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->AddView(cellular_status_label_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Call NetworkChanged to set initial values.
  NetworkChanged(NetworkLibrary::Get());
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
// InternetPageView

////////////////////////////////////////////////////////////////////////////////
// InternetPageView, SettingsPageView implementation:

void InternetPageView::InitControlLayout() {
  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(new NetworkSection(profile()));
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
}

}  // namespace chromeos
