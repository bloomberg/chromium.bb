// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/internet_page_view.h"

#include <string>

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/options/options_window_view.h"
#include "chrome/browser/chromeos/status/network_menu.h"
#include "chrome/browser/ui/views/window.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/button/native_button.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/image_view.h"
#include "views/controls/scroll_view.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// NetworkSection

class NetworkSection : public SettingsPageSection,
                       public views::ButtonListener {
 public:
  NetworkSection(InternetPageContentView* parent, Profile* profile,
                 int title_msg_id);
  virtual ~NetworkSection() {}

  // Overriden from views::Button::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  void RefreshContents();

 protected:
  enum ButtonFlags {
    OPTIONS_BUTTON    = 1 << 0,
    CONNECT_BUTTON    = 1 << 1,
    DISCONNECT_BUTTON = 1 << 2,
    FORGET_BUTTON     = 1 << 3,
  };

  // SettingsPageSection overrides:
  virtual void InitContents(GridLayout* layout);

  // Subclasses will initialize themselves in this method.
  virtual void InitSection() = 0;

  // This adds a row for a network.
  // |id| is passed back in the ButtonClicked method.
  // |icon|, |name|, |bold_name|, and |status| are displayed in the row.
  // |button_flags| is an OR of ButtonFlags that should be displayed.
  void AddNetwork(int id, const SkBitmap& icon, const std::wstring& name,
                  bool bold_name, const std::wstring& status, int button_flags,
                  int connection_type);

  // Creates a modal popup with |view|.
  void CreateModalPopup(views::WindowDelegate* view);

  // This method is called when the user click on the |button| for |id|.
  virtual void ButtonClicked(int button, int connection_type, int id) = 0;

 private:
  // This constant determines the button tag offset for the different buttons.
  // The ButtonFlag is multiplied by this offset to determine the button tag.
  // For example, for disconnect buttons (DISCONNECT_BUTTON = 4), the button tag
  //   will be offset by 4000.
  static const int kButtonIdOffset = 1000;
  // This constant determines the button tag offset for the connection types.
  // The ConnectionType is multiplied by this to determine the button tag.
  // For example, for wifi buttons (TYPE_WIFI = 2), the button tag
  //   will be offset by 200.
  static const int kConnectionTypeOffset = 100;

  InternetPageContentView* parent_;

  int quad_column_view_set_id_;

  GridLayout* layout_;

  DISALLOW_COPY_AND_ASSIGN(NetworkSection);
};

NetworkSection::NetworkSection(InternetPageContentView* parent,
                               Profile* profile,
                               int title_msg_id)
    : SettingsPageSection(profile, title_msg_id),
      parent_(parent),
      quad_column_view_set_id_(1) {
}

void NetworkSection::ButtonPressed(views::Button* sender,
                                   const views::Event& event) {
  int id = sender->tag();
  // Determine the button from the id (div by kButtonIdOffset).
  int button = id / kButtonIdOffset;
  id %= kButtonIdOffset;
  // Determine the connection type from the id (div by kConnectionTypeOffset).
  int connection_type = id / kConnectionTypeOffset;
  id %= kConnectionTypeOffset;

  ButtonClicked(button, connection_type, id);
}

void NetworkSection::RefreshContents() {
  RemoveAllChildViews(true);
  InitControlLayout();
}

void NetworkSection::InitContents(GridLayout* layout) {
  layout_ = layout;

  ColumnSet* column_set = layout_->AddColumnSet(quad_column_view_set_id_);
  // icon
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  // network name
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  // fill padding
  column_set->AddPaddingColumn(10, 0);
  // first button
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  // padding
  column_set->AddPaddingColumn(0, 10);
  // second button
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  InitSection();
}

void NetworkSection::AddNetwork(int id, const SkBitmap& icon,
                                const std::wstring& name, bool bold_name,
                                const std::wstring& status, int button_flags,
                                int connection_type) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  // Offset id by connection type.
  id += kConnectionTypeOffset * connection_type;

  layout_->StartRow(0, quad_column_view_set_id_);
  views::ImageView* icon_view = new views::ImageView();
  icon_view->SetImage(icon);
  layout_->AddView(icon_view, 1, 2);

  views::Label* name_view = new views::Label(name);
  if (bold_name)
    name_view->SetFont(rb.GetFont(ResourceBundle::BoldFont));
  name_view->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout_->AddView(name_view);

  int num_buttons = 0;
  if (button_flags & OPTIONS_BUTTON)
    num_buttons++;
  if (button_flags & CONNECT_BUTTON)
    num_buttons++;
  if (button_flags & DISCONNECT_BUTTON)
    num_buttons++;
  if (button_flags & FORGET_BUTTON)
    num_buttons++;

  if (num_buttons > 0) {
    // We only support 2 buttons.
    DCHECK_LE(num_buttons, 2);

    if (num_buttons == 1)
      layout_->SkipColumns(1);

    if (button_flags & FORGET_BUTTON) {
      views::NativeButton* button = new views::NativeButton(this,
          UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_FORGET)));
      button->set_tag(id + kButtonIdOffset * FORGET_BUTTON);
      layout_->AddView(button, 1, 2);
    }

    if (button_flags & DISCONNECT_BUTTON) {
      views::NativeButton* button = new views::NativeButton(this, UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_DISCONNECT)));
      button->set_tag(id + kButtonIdOffset * DISCONNECT_BUTTON);
      layout_->AddView(button, 1, 2);
    }

    if (button_flags & CONNECT_BUTTON) {
      views::NativeButton* button = new views::NativeButton(this,
          UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_CONNECT)));
      button->set_tag(id + kButtonIdOffset * CONNECT_BUTTON);
      layout_->AddView(button, 1, 2);
    }

    if (button_flags & OPTIONS_BUTTON) {
      views::NativeButton* button = new views::NativeButton(this,
          UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_OPTIONS)));
      button->set_tag(id + kButtonIdOffset * OPTIONS_BUTTON);
      layout_->AddView(button, 1, 2);
    }
  }

  layout_->StartRow(0, quad_column_view_set_id_);
  layout_->SkipColumns(1);
  views::Label* status_label = new views::Label(status);
  status_label->SetFont(rb.GetFont(ResourceBundle::SmallFont));
  status_label->SetColor(SK_ColorLTGRAY);
  status_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout_->AddView(status_label);
  layout_->AddPaddingRow(0, kRelatedControlVerticalSpacing);
}

void NetworkSection::CreateModalPopup(views::WindowDelegate* view) {
  views::Window* window = browser::CreateViewsWindow(
      GetOptionsViewParent(), gfx::Rect(), view);
  window->SetIsAlwaysOnTop(true);
  window->Show();
}

////////////////////////////////////////////////////////////////////////////////
// WiredSection

class WiredSection : public NetworkSection {
 public:
  WiredSection(InternetPageContentView* parent, Profile* profile);
  virtual ~WiredSection() {}

 protected:
  // NetworkSection overrides:
  virtual void InitSection();
  virtual void ButtonClicked(int button, int connection_type, int id);

  DISALLOW_COPY_AND_ASSIGN(WiredSection);
};

WiredSection::WiredSection(InternetPageContentView* parent, Profile* profile)
    : NetworkSection(parent, profile,
                     IDS_OPTIONS_SETTINGS_SECTION_TITLE_WIRED_NETWORK) {
}

void WiredSection::InitSection() {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  SkBitmap icon = *rb.GetBitmapNamed(IDR_STATUSBAR_WIRED_BLACK);
  if (!cros->ethernet_connecting() && !cros->ethernet_connected()) {
    icon = NetworkMenu::IconForDisplay(icon,
        *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_DISCONNECTED));
  }

  std::wstring name = UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET));

  int s = IDS_STATUSBAR_NETWORK_DEVICE_DISABLED;
  if (cros->ethernet_connecting())
    s = IDS_STATUSBAR_NETWORK_DEVICE_CONNECTING;
  else if (cros->ethernet_connected())
    s = IDS_STATUSBAR_NETWORK_DEVICE_CONNECTED;
  else if (cros->ethernet_enabled())
    s = IDS_STATUSBAR_NETWORK_DEVICE_DISCONNECTED;
  std::wstring status = UTF16ToWide(l10n_util::GetStringUTF16(s));

  int flags = cros->ethernet_connected() ? OPTIONS_BUTTON : 0;
  bool bold = cros->ethernet_connected() ? true : false;
  AddNetwork(0, icon, name, bold, status, flags, TYPE_ETHERNET);
}

void WiredSection::ButtonClicked(int button, int connection_type, int id) {
//  CreateModalPopup(new NetworkConfigView(
//      CrosLibrary::Get()->GetNetworkLibrary()->ethernet_network()));
}

////////////////////////////////////////////////////////////////////////////////
// WirelessSection

class WirelessSection : public NetworkSection {
 public:
  WirelessSection(InternetPageContentView* parent, Profile* profile);
  virtual ~WirelessSection() {}

 protected:
  // NetworkSection overrides:
  virtual void InitSection();
  virtual void ButtonClicked(int button, int connection_type, int id);

 private:
  // This calls NetworkSection::AddNetwork .
  // For |connecting| or |connected| networks, the name is bold.
  // The status is "Connecting" if |connecting|, "Connected" if |connected|,
  // or "Disconnected".
  // For connected networks, we show the disconnect and options buttons.
  // For !connected and !connecting networks, we show the connect button.
  void AddWirelessNetwork(int id, const SkBitmap& icon,
                          const std::wstring& name, bool connecting,
                          bool connected, int connection_type);

  WifiNetworkVector wifi_networks_;
  CellularNetworkVector cellular_networks_;

  DISALLOW_COPY_AND_ASSIGN(WirelessSection);
};

WirelessSection::WirelessSection(InternetPageContentView* parent,
                                 Profile* profile)
    : NetworkSection(parent, profile,
                     IDS_OPTIONS_SETTINGS_SECTION_TITLE_WIRELESS_NETWORK) {
}

void WirelessSection::InitSection() {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  // Wifi
  wifi_networks_ = cros->wifi_networks();
  for (size_t i = 0; i < wifi_networks_.size(); ++i) {
    std::wstring name = ASCIIToWide(wifi_networks_[i]->name());

    SkBitmap icon = NetworkMenu::IconForNetworkStrength(
        wifi_networks_[i], true);
    if (wifi_networks_[i]->encrypted()) {
      icon = NetworkMenu::IconForDisplay(icon,
          *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_SECURE));
    }

    bool connecting = wifi_networks_[i]->connecting();
    bool connected = wifi_networks_[i]->connected();
    AddWirelessNetwork(i, icon, name, connecting, connected, TYPE_WIFI);
  }

  // Cellular
  cellular_networks_ = cros->cellular_networks();
  for (size_t i = 0; i < cellular_networks_.size(); ++i) {
    std::wstring name = ASCIIToWide(cellular_networks_[i]->name());

    SkBitmap icon = NetworkMenu::IconForNetworkStrength(
        cellular_networks_[i], true);
    SkBitmap badge =
        NetworkMenu::BadgeForNetworkTechnology(cellular_networks_[i]);
    icon = NetworkMenu::IconForDisplay(icon, badge);

    bool connecting = cellular_networks_[i]->connecting();
    bool connected = cellular_networks_[i]->connected();
    AddWirelessNetwork(i, icon, name, connecting, connected, TYPE_CELLULAR);
  }
}

void WirelessSection::ButtonClicked(int button, int connection_type, int id) {
  if (connection_type == TYPE_CELLULAR) {
    if (static_cast<int>(cellular_networks_.size()) > id) {
      if (button == CONNECT_BUTTON) {
        // Connect to cellular network.
        CrosLibrary::Get()->GetNetworkLibrary()->ConnectToCellularNetwork(
            cellular_networks_[id]);
      } else if (button == DISCONNECT_BUTTON) {
        CrosLibrary::Get()->GetNetworkLibrary()->DisconnectFromWirelessNetwork(
            cellular_networks_[id]);
      } else {
//        CreateModalPopup(new NetworkConfigView(cellular_networks_[id]));
      }
    }
  } else if (connection_type == TYPE_WIFI) {
    if (static_cast<int>(wifi_networks_.size()) > id) {
      if (button == CONNECT_BUTTON) {
        // Connect to wifi here. Open password page if appropriate.
        if (wifi_networks_[id]->encrypted()) {
//          NetworkConfigView* view =
//              new NetworkConfigView(wifi_networks_[id], true);
//          CreateModalPopup(view);
//          view->SetLoginTextfieldFocus();
        } else {
          CrosLibrary::Get()->GetNetworkLibrary()->ConnectToWifiNetwork(
              wifi_networks_[id], std::string(), std::string(), std::string());
        }
      } else if (button == DISCONNECT_BUTTON) {
        CrosLibrary::Get()->GetNetworkLibrary()->DisconnectFromWirelessNetwork(
            wifi_networks_[id]);
      } else {
//        CreateModalPopup(new NetworkConfigView(wifi_networks_[id], false));
      }
    }
  } else {
    NOTREACHED();
  }
}

void WirelessSection::AddWirelessNetwork(int id, const SkBitmap& icon,
    const std::wstring& name, bool connecting, bool connected,
    int connection_type) {
  bool bold = connecting || connected;

  int s = IDS_STATUSBAR_NETWORK_DEVICE_DISCONNECTED;
  if (connecting)
    s = IDS_STATUSBAR_NETWORK_DEVICE_CONNECTING;
  else if (connected)
    s = IDS_STATUSBAR_NETWORK_DEVICE_CONNECTED;
  std::wstring status = UTF16ToWide(l10n_util::GetStringUTF16(s));

  int flags = 0;
  if (connected) {
    flags |= DISCONNECT_BUTTON | OPTIONS_BUTTON;
  } else if (!connecting) {
    flags |= CONNECT_BUTTON;
  }

  AddNetwork(id, icon, name, bold, status, flags, connection_type);
}

////////////////////////////////////////////////////////////////////////////////
// RememberedSection

class RememberedSection : public NetworkSection {
 public:
  RememberedSection(InternetPageContentView* parent, Profile* profile);
  virtual ~RememberedSection() {}

 protected:
  // NetworkSection overrides:
  virtual void InitSection();
  virtual void ButtonClicked(int button, int connection_type, int id);

 private:
  WifiNetworkVector wifi_networks_;

  DISALLOW_COPY_AND_ASSIGN(RememberedSection);
};

RememberedSection::RememberedSection(InternetPageContentView* parent,
                                     Profile* profile)
    : NetworkSection(parent, profile,
                     IDS_OPTIONS_SETTINGS_SECTION_TITLE_REMEMBERED_NETWORK) {
}

void RememberedSection::InitSection() {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  // Wifi
  wifi_networks_ = cros->remembered_wifi_networks();
  for (size_t i = 0; i < wifi_networks_.size(); ++i) {
    std::wstring name = ASCIIToWide(wifi_networks_[i]->name());

    SkBitmap icon = *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS0_BLACK);
    if (wifi_networks_[i]->encrypted()) {
      icon = NetworkMenu::IconForDisplay(icon,
          *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_SECURE));
    }

    AddNetwork(i, icon, name, false, std::wstring(), FORGET_BUTTON, TYPE_WIFI);
  }
}

void RememberedSection::ButtonClicked(int button, int connection_type, int id) {
  if (connection_type == TYPE_WIFI) {
    if (static_cast<int>(wifi_networks_.size()) > id) {
      CrosLibrary::Get()->GetNetworkLibrary()->ForgetWifiNetwork(
          wifi_networks_[id]->service_path());
    }
  } else {
    NOTREACHED();
  }
}

////////////////////////////////////////////////////////////////////////////////
// InternetPageContentView

class InternetPageContentView : public SettingsPageView {
 public:
  explicit InternetPageContentView(Profile* profile);
  virtual ~InternetPageContentView() {}

  virtual void RefreshContents();

  // views::View overrides.
  virtual int GetLineScrollIncrement(views::ScrollView* scroll_view,
                                     bool is_horizontal, bool is_positive);
  virtual void Layout();
  virtual void DidChangeBounds(const gfx::Rect& previous,
                               const gfx::Rect& current);

 protected:
  // SettingsPageView implementation:
  virtual void InitControlLayout();

 private:
  int line_height_;
  WiredSection* wired_section_;
  WirelessSection* wireless_section_;
  RememberedSection* remembered_section_;

  DISALLOW_COPY_AND_ASSIGN(InternetPageContentView);
};

////////////////////////////////////////////////////////////////////////////////
// InternetPageContentView, SettingsPageView implementation:

InternetPageContentView::InternetPageContentView(Profile* profile)
    : SettingsPageView(profile) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  line_height_ = rb.GetFont(ResourceBundle::BaseFont).GetHeight();
}

void InternetPageContentView::RefreshContents() {
  wired_section_->RefreshContents();
  wireless_section_->RefreshContents();
  remembered_section_->RefreshContents();
}

int InternetPageContentView::GetLineScrollIncrement(
    views::ScrollView* scroll_view,
    bool is_horizontal,
    bool is_positive) {
  if (!is_horizontal)
    return line_height_;
  return View::GetPageScrollIncrement(scroll_view, is_horizontal, is_positive);
}

void InternetPageContentView::Layout() {
  // Set the width to the parent width and the height to the preferred height.
  // We will have a vertical scrollbar if the preferred height is longer
  // than the parent's height.
  SetBounds(0, 0, GetParent()->width(), GetPreferredSize().height());
  View::Layout();
}

void InternetPageContentView::DidChangeBounds(const gfx::Rect& previous,
                                              const gfx::Rect& current) {
  // Override to do nothing. Calling Layout() interferes with our scrolling.
}

void InternetPageContentView::InitControlLayout() {
  GridLayout* layout = GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  wired_section_ = new WiredSection(this, profile());
  layout->AddView(wired_section_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  wireless_section_ = new WirelessSection(this, profile());
  layout->AddView(wireless_section_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  remembered_section_ = new RememberedSection(this, profile());
  layout->AddView(remembered_section_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
}

////////////////////////////////////////////////////////////////////////////////
// InternetPageView

InternetPageView::InternetPageView(Profile* profile)
    : SettingsPageView(profile),
      contents_view_(new InternetPageContentView(profile)),
      scroll_view_(new views::ScrollView) {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  cros->AddNetworkManagerObserver(this);
}

InternetPageView::~InternetPageView() {
  CrosLibrary::Get()->GetNetworkLibrary()->RemoveNetworkManagerObserver(this);
}

void InternetPageView::OnNetworkManagerChanged(NetworkLibrary* obj) {
  // Refresh wired, wireless, and remembered networks.
  // Remember the current scroll region, and try to scroll back afterwards.
  gfx::Rect rect = scroll_view_->GetVisibleRect();
  contents_view_->RefreshContents();
  Layout();
  scroll_view_->ScrollContentsRegionToBeVisible(rect);
}

void InternetPageView::Layout() {
  contents_view_->Layout();
  scroll_view_->SetBounds(GetLocalBounds(false));
  scroll_view_->Layout();
}

void InternetPageView::InitControlLayout() {
  AddChildView(scroll_view_);
  scroll_view_->SetContents(contents_view_);
}

}  // namespace chromeos
