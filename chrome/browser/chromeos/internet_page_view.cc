// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/internet_page_view.h"

#include <string>

#include "app/combobox_model.h"
#include "chrome/browser/chromeos/network_library.h"
#include "chrome/browser/chromeos/password_dialog_view.h"
#include "grit/generated_resources.h"
#include "views/controls/combobox/combobox.h"
#include "views/window/window.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// NetworkSection

// Network section for wifi settings
class NetworkSection : public SettingsPageSection,
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
