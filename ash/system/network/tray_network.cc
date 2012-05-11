// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/tray_network.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_more.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/tray/tray_views.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/widget/widget.h"

namespace {

// Height of the list of networks in the popup.
const int kNetworkListHeight = 203;

// Create a label with the font size and color used in the network info bubble.
views::Label* CreateInfoBubbleLabel(const string16& text) {
  const SkColor text_color = SkColorSetARGB(127, 0, 0, 0);
  views::Label* label = new views::Label(text);
  label->SetFont(label->font().DeriveFont(-1));
  label->SetEnabledColor(text_color);
  return label;
}

// Create a row of labels for the network info bubble.
views::View* CreateInfoBubbleLine(const string16& text_label,
                                  const std::string& text_string) {
  views::View* view = new views::View;
  view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 1));
  view->AddChildView(CreateInfoBubbleLabel(text_label));
  view->AddChildView(CreateInfoBubbleLabel(UTF8ToUTF16(": ")));
  view->AddChildView(CreateInfoBubbleLabel(UTF8ToUTF16(text_string)));
  return view;
}

// A bubble that cannot be activated.
class NonActivatableSettingsBubble : public views::BubbleDelegateView {
 public:
  NonActivatableSettingsBubble(views::View* anchor, views::View* content)
      : views::BubbleDelegateView(anchor, views::BubbleBorder::TOP_RIGHT) {
    set_use_focusless(true);
    set_parent_window(ash::Shell::GetInstance()->GetContainer(
        ash::internal::kShellWindowId_SettingBubbleContainer));
    SetLayoutManager(new views::FillLayout());
    AddChildView(content);
  }

  virtual ~NonActivatableSettingsBubble() {}

  virtual bool CanActivate() const OVERRIDE {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NonActivatableSettingsBubble);
};

}  // namespace

namespace ash {
namespace internal {

namespace tray {

enum ColorTheme {
  LIGHT,
  DARK,
};

class NetworkTrayView : public TrayItemView {
 public:
  NetworkTrayView(ColorTheme size, bool tray_icon)
      : color_theme_(size), tray_icon_(tray_icon) {
    SetLayoutManager(new views::FillLayout());

    image_view_ = color_theme_ == DARK ?
        new FixedSizedImageView(0, kTrayPopupItemHeight) :
        new views::ImageView;
    AddChildView(image_view_);

    NetworkIconInfo info;
    Shell::GetInstance()->tray_delegate()->
        GetMostRelevantNetworkIcon(&info, false);
    Update(info);
  }

  virtual ~NetworkTrayView() {}

  void Update(const NetworkIconInfo& info) {
    image_view_->SetImage(info.image);
    if (tray_icon_)
      SetVisible(info.tray_icon_visible);
    SchedulePaint();
  }

 private:
  views::ImageView* image_view_;
  ColorTheme color_theme_;
  bool tray_icon_;

  DISALLOW_COPY_AND_ASSIGN(NetworkTrayView);
};

class NetworkDefaultView : public TrayItemMore {
 public:
  explicit NetworkDefaultView(SystemTrayItem* owner)
      : TrayItemMore(owner) {
    Update();
  }

  virtual ~NetworkDefaultView() {}

  void Update() {
    NetworkIconInfo info;
    Shell::GetInstance()->tray_delegate()->
        GetMostRelevantNetworkIcon(&info, true);
    SetImage(&info.image);
    SetLabel(info.description);
    SetAccessibleName(info.description);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkDefaultView);
};

class NetworkDetailedView : public views::View,
                            public views::ButtonListener,
                            public ViewClickListener {
 public:
  explicit NetworkDetailedView(user::LoginStatus login)
      : login_(login),
        header_(NULL),
        airplane_(NULL),
        info_icon_(NULL),
        button_wifi_(NULL),
        button_cellular_(NULL),
        view_mobile_account_(NULL),
        setup_mobile_account_(NULL),
        networks_list_(NULL),
        scroller_(NULL),
        other_wifi_(NULL),
        other_mobile_(NULL),
        settings_(NULL),
        proxy_settings_(NULL),
        info_bubble_(NULL) {
    SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kVertical, 0, 0, 0));
    set_background(views::Background::CreateSolidBackground(kBackgroundColor));
    SystemTrayDelegate* delegate = Shell::GetInstance()->tray_delegate();
    delegate->RequestNetworkScan();
    CreateItems();
    Update();
  }

  virtual ~NetworkDetailedView() {
    if (info_bubble_)
      info_bubble_->GetWidget()->CloseNow();
  }

  void CreateItems() {
    RemoveAllChildViews(true);

    header_ = NULL;
    airplane_ = NULL;
    info_icon_ = NULL;
    button_wifi_ = NULL;
    button_cellular_ = NULL;
    view_mobile_account_ = NULL;
    setup_mobile_account_ = NULL;
    networks_list_ = NULL;
    scroller_ = NULL;
    other_wifi_ = NULL;
    other_mobile_ = NULL;
    settings_ = NULL;
    proxy_settings_ = NULL;

    AppendNetworkEntries();
    AppendNetworkExtra();
    AppendHeaderEntry();
    AppendHeaderButtons();

    Update();
  }

  void Update() {
    UpdateHeaderButtons();
    UpdateNetworkEntries();
    UpdateNetworkExtra();

    Layout();
  }

 private:
  void AppendHeaderEntry() {
    header_ = new SpecialPopupRow();
    header_->SetTextLabel(IDS_ASH_STATUS_TRAY_NETWORK, this);
    AddChildView(header_);
  }

  void AppendHeaderButtons() {
    button_wifi_ = new TrayPopupHeaderButton(this,
        IDR_AURA_UBER_TRAY_WIFI_ENABLED,
        IDR_AURA_UBER_TRAY_WIFI_DISABLED,
        IDR_AURA_UBER_TRAY_WIFI_ENABLED_HOVER,
        IDR_AURA_UBER_TRAY_WIFI_DISABLED_HOVER);
    header_->AddButton(button_wifi_);

    button_cellular_ = new TrayPopupHeaderButton(this,
        IDR_AURA_UBER_TRAY_CELLULAR_ENABLED,
        IDR_AURA_UBER_TRAY_CELLULAR_DISABLED,
        IDR_AURA_UBER_TRAY_CELLULAR_ENABLED_HOVER,
        IDR_AURA_UBER_TRAY_CELLULAR_DISABLED_HOVER);
    header_->AddButton(button_cellular_);

    info_icon_ = new TrayPopupHeaderButton(this,
        IDR_AURA_UBER_TRAY_NETWORK_INFO,
        IDR_AURA_UBER_TRAY_NETWORK_INFO,
        IDR_AURA_UBER_TRAY_NETWORK_INFO_HOVER,
        IDR_AURA_UBER_TRAY_NETWORK_INFO_HOVER);
    header_->AddButton(info_icon_);
  }

  void UpdateHeaderButtons() {
    SystemTrayDelegate* delegate = Shell::GetInstance()->tray_delegate();
    button_wifi_->SetToggled(!delegate->GetWifiEnabled());
    button_cellular_->SetToggled(!delegate->GetCellularEnabled());
    button_cellular_->SetVisible(delegate->GetCellularAvailable());
  }

  void AppendNetworkEntries() {
    FixedSizedScrollView* scroller = new FixedSizedScrollView;
    networks_list_ = new views::View;
    networks_list_->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kVertical, 0, 0, 1));

    HoverHighlightView* container = new HoverHighlightView(this);
    container->set_fixed_height(kTrayPopupItemHeight);
    container->AddLabel(ui::ResourceBundle::GetSharedInstance().
        GetLocalizedString(IDS_ASH_STATUS_TRAY_MOBILE_VIEW_ACCOUNT),
        gfx::Font::NORMAL);
    AddChildView(container);
    view_mobile_account_ = container;

    container = new HoverHighlightView(this);
    container->set_fixed_height(kTrayPopupItemHeight);
    container->AddLabel(ui::ResourceBundle::GetSharedInstance().
        GetLocalizedString(IDS_ASH_STATUS_TRAY_SETUP_MOBILE),
        gfx::Font::NORMAL);
    AddChildView(container);
    setup_mobile_account_ = container;

    scroller->set_border(views::Border::CreateSolidSidedBorder(1, 0, 1, 0,
        SkColorSetARGB(25, 0, 0, 0)));
    scroller->set_fixed_size(
        gfx::Size(networks_list_->GetPreferredSize().width() +
                  scroller->GetScrollBarWidth(),
                  kNetworkListHeight));
    scroller->SetContentsView(networks_list_);
    AddChildView(scroller);
    scroller_ = scroller;
  }

  void UpdateNetworkEntries() {
    SystemTrayDelegate* delegate = Shell::GetInstance()->tray_delegate();
    std::vector<NetworkIconInfo> list;
    delegate->GetAvailableNetworks(&list);

    network_map_.clear();
    networks_list_->RemoveAllChildViews(true);
    views::View* highlighted_view = NULL;
    for (size_t i = 0; i < list.size(); i++) {
      HoverHighlightView* container = new HoverHighlightView(this);
      container->set_fixed_height(kTrayPopupItemHeight);
      container->AddIconAndLabel(list[i].image,
          list[i].description.empty() ? list[i].name : list[i].description,
          list[i].highlight ? gfx::Font::BOLD : gfx::Font::NORMAL);
      networks_list_->AddChildView(container);
      if (list[i].highlight)
        highlighted_view = container;
      container->set_border(views::Border::CreateEmptyBorder(0,
          kTrayPopupDetailsIconWidth, 0, 0));
      network_map_[container] = list[i].service_path;
    }
    networks_list_->SizeToPreferredSize();
    scroller_->Layout();
    if (highlighted_view)
      networks_list_->ScrollRectToVisible(highlighted_view->bounds());

    view_mobile_account_->SetVisible(false);
    setup_mobile_account_->SetVisible(false);

    if (login_ == user::LOGGED_IN_NONE)
      return;

    std::string carrier_id, topup_url, setup_url;
    if (delegate->GetCellularCarrierInfo(&carrier_id,
                                         &topup_url,
                                         &setup_url)) {
      if (carrier_id != carrier_id_) {
        carrier_id_ = carrier_id;
        if (!topup_url.empty())
          topup_url_ = topup_url;
      }

      if (!setup_url.empty())
        setup_url_ = setup_url;

      if (!topup_url_.empty())
        view_mobile_account_->SetVisible(true);
      if (!setup_url_.empty())
        setup_mobile_account_->SetVisible(true);
    }
  }

  void AppendNetworkExtra() {
    if (login_ == user::LOGGED_IN_LOCKED)
      return;

    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

    TrayPopupTextButtonContainer* bottom_row =
        new TrayPopupTextButtonContainer;

    other_wifi_ = new TrayPopupTextButton(this,
        rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_OTHER_WIFI));
    bottom_row->AddTextButton(other_wifi_);

    other_mobile_ = new TrayPopupTextButton(this,
        rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_OTHER_MOBILE));
    bottom_row->AddTextButton(other_mobile_);

    CreateSettingsEntry();
    bottom_row->AddTextButton(settings_ ? settings_ : proxy_settings_);

    AddChildView(bottom_row);
  }

  void UpdateNetworkExtra() {
    if (login_ == user::LOGGED_IN_LOCKED)
      return;

    SystemTrayDelegate* delegate = Shell::GetInstance()->tray_delegate();
    other_wifi_->SetEnabled(delegate->GetWifiEnabled());
    other_mobile_->SetVisible(delegate->GetCellularAvailable() &&
                              delegate->GetCellularScanSupported());
    if (other_mobile_->visible())
      other_mobile_->SetEnabled(delegate->GetCellularEnabled());
  }

  void AppendAirplaneModeEntry() {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    HoverHighlightView* container = new HoverHighlightView(this);
    container->set_fixed_height(kTrayPopupItemHeight);
    container->AddIconAndLabel(
        *rb.GetImageNamed(IDR_AURA_UBER_TRAY_NETWORK_AIRPLANE).ToSkBitmap(),
        rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_AIRPLANE_MODE),
        gfx::Font::NORMAL);
    AddChildView(container);
    airplane_ = container;
  }

  // Adds a settings entry when logged in, and an entry for changing proxy
  // settings otherwise.
  void CreateSettingsEntry() {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    if (login_ != user::LOGGED_IN_NONE) {
      // Settings, only if logged in.
      settings_ = new TrayPopupTextButton(this,
          rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_NETWORK_SETTINGS));
    } else {
      proxy_settings_ = new TrayPopupTextButton(this,
          rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_NETWORK_PROXY_SETTINGS));
    }
  }

  views::View* CreateNetworkInfoView() {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    std::string ip_address, ethernet_address, wifi_address;
    Shell::GetInstance()->tray_delegate()->GetNetworkAddresses(&ip_address,
        &ethernet_address, &wifi_address);

    views::View* container = new views::View;
    container->SetLayoutManager(new
        views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));
    container->set_border(views::Border::CreateEmptyBorder(0, 5, 0, 5));

    // GetNetworkAddresses returns empty strings if no information is available.
    if (!ip_address.empty()) {
      container->AddChildView(CreateInfoBubbleLine(bundle.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_IP), ip_address));
    }
    if (!ethernet_address.empty()) {
      container->AddChildView(CreateInfoBubbleLine(bundle.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_ETHERNET), ethernet_address));
    }
    if (!wifi_address.empty()) {
      container->AddChildView(CreateInfoBubbleLine(bundle.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_WIFI), wifi_address));
    }

    // Avoid an empty bubble in the unlikely event that there is no network
    // information at all.
    if (!container->has_children()) {
      container->AddChildView(CreateInfoBubbleLabel(bundle.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_NO_NETWORKS)));
    }

    return container;
  }

  void ToggleInfoBubble() {
    if (ResetInfoBubble())
      return;

    info_bubble_ = new NonActivatableSettingsBubble(
        info_icon_, CreateNetworkInfoView());
    views::BubbleDelegateView::CreateBubble(info_bubble_);
    info_bubble_->Show();
  }

  // Returns whether an existing info-bubble was closed.
  bool ResetInfoBubble() {
    if (!info_bubble_)
      return false;
    info_bubble_->GetWidget()->Close();
    info_bubble_ = NULL;
    return true;
  }

  // Overridden from ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE {
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    if (sender == info_icon_) {
      ToggleInfoBubble();
      return;
    }

    // If the info bubble was visible, close it when some other item is clicked
    // on.
    ResetInfoBubble();
    if (sender == button_wifi_)
      delegate->ToggleWifi();
    else if (sender == button_cellular_)
      delegate->ToggleCellular();
    else if (sender == settings_)
      delegate->ShowNetworkSettings();
    else if (sender == proxy_settings_)
      delegate->ChangeProxySettings();
    else if (sender == other_mobile_)
      delegate->ShowOtherCellular();
    else if (sender == other_wifi_)
      delegate->ShowOtherWifi();
    else
      NOTREACHED();
  }

  // Overridden from ViewClickListener.
  virtual void ClickedOn(views::View* sender) OVERRIDE {
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    // If the info bubble was visible, close it when some other item is clicked
    // on.
    ResetInfoBubble();

    if (sender == header_->content())
      Shell::GetInstance()->tray()->ShowDefaultView();

    if (login_ == user::LOGGED_IN_LOCKED)
      return;

    if (sender == view_mobile_account_) {
      delegate->ShowCellularURL(topup_url_);
    } else if (sender == setup_mobile_account_) {
      delegate->ShowCellularURL(setup_url_);
    } else if (sender == airplane_) {
      delegate->ToggleAirplaneMode();
    } else {
      std::map<views::View*, std::string>::iterator find;
      find = network_map_.find(sender);
      if (find != network_map_.end()) {
        std::string network_id = find->second;
        delegate->ConnectToNetwork(network_id);
      }
    }
  }

  std::string carrier_id_;
  std::string topup_url_;
  std::string setup_url_;

  user::LoginStatus login_;
  std::map<views::View*, std::string> network_map_;
  SpecialPopupRow* header_;
  views::View* airplane_;
  TrayPopupHeaderButton* info_icon_;
  TrayPopupHeaderButton* button_wifi_;
  TrayPopupHeaderButton* button_cellular_;
  views::View* view_mobile_account_;
  views::View* setup_mobile_account_;
  views::View* networks_list_;
  views::View* scroller_;
  TrayPopupTextButton* other_wifi_;
  TrayPopupTextButton* other_mobile_;
  TrayPopupTextButton* settings_;
  TrayPopupTextButton* proxy_settings_;

  views::BubbleDelegateView* info_bubble_;

  DISALLOW_COPY_AND_ASSIGN(NetworkDetailedView);
};

}  // namespace tray

TrayNetwork::TrayNetwork()
    : tray_(NULL),
      default_(NULL),
      detailed_(NULL) {
}

TrayNetwork::~TrayNetwork() {
}

views::View* TrayNetwork::CreateTrayView(user::LoginStatus status) {
  CHECK(tray_ == NULL);
  tray_ = new tray::NetworkTrayView(tray::LIGHT, true /*tray_icon*/);
  return tray_;
}

views::View* TrayNetwork::CreateDefaultView(user::LoginStatus status) {
  CHECK(default_ == NULL);
  default_ = new tray::NetworkDefaultView(this);
  return default_;
}

views::View* TrayNetwork::CreateDetailedView(user::LoginStatus status) {
  CHECK(detailed_ == NULL);
  detailed_ = new tray::NetworkDetailedView(status);
  return detailed_;
}

void TrayNetwork::DestroyTrayView() {
  tray_ = NULL;
}

void TrayNetwork::DestroyDefaultView() {
  default_ = NULL;
}

void TrayNetwork::DestroyDetailedView() {
  detailed_ = NULL;
}

void TrayNetwork::UpdateAfterLoginStatusChange(user::LoginStatus status) {
}

void TrayNetwork::OnNetworkRefresh(const NetworkIconInfo& info) {
  if (tray_)
    tray_->Update(info);
  if (default_)
    default_->Update();
  if (detailed_)
    detailed_->Update();
}

}  // namespace internal
}  // namespace ash
