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
#include "ash/system/tray/tray_views.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
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
const int kNetworkListHeight = 160;

// Creates a row of labels.
views::View* CreateTextLabels(const string16& text_label,
                              const std::string& text_string) {
  const SkColor text_color = SkColorSetARGB(127, 0, 0, 0);
  views::View* view = new views::View;
  view->SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
        0, 0, 1));

  views::Label* label = new views::Label(text_label);
  label->SetFont(label->font().DeriveFont(-1));
  label->SetEnabledColor(text_color);
  view->AddChildView(label);

  label = new views::Label(UTF8ToUTF16(": "));
  label->SetFont(label->font().DeriveFont(-1));
  label->SetEnabledColor(text_color);
  view->AddChildView(label);

  label = new views::Label(UTF8ToUTF16(text_string));
  label->SetFont(label->font().DeriveFont(-1));
  label->SetEnabledColor(text_color);
  view->AddChildView(label);

  return view;
}

// A bubble you canno activate.
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

}

namespace ash {
namespace internal {

namespace tray {

enum ColorTheme {
  LIGHT,
  DARK,
};

class NetworkTrayView : public views::View {
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
    icon_ = new NetworkTrayView(DARK, false /* tray_icon */);
    ReplaceIcon(icon_);
    Update();
  }

  virtual ~NetworkDefaultView() {}

  void Update() {
    NetworkIconInfo info;
    Shell::GetInstance()->tray_delegate()->
        GetMostRelevantNetworkIcon(&info, true);
    icon_->Update(info);
    SetLabel(info.description);
    SetAccessibleName(info.description);
  }

 private:
  NetworkTrayView* icon_;

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
        mobile_account_(NULL),
        other_wifi_(NULL),
        other_mobile_(NULL),
        toggle_wifi_(NULL),
        toggle_mobile_(NULL),
        settings_(NULL),
        proxy_settings_(NULL),
        info_bubble_(NULL) {
    SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kVertical, 1, 1, 1));
    set_background(views::Background::CreateSolidBackground(kBackgroundColor));
    SystemTrayDelegate* delegate = Shell::GetInstance()->tray_delegate();
    delegate->RequestNetworkScan();
    Update();
  }

  virtual ~NetworkDetailedView() {
    if (info_bubble_)
      info_bubble_->GetWidget()->CloseNow();
  }

  void Update() {
    RemoveAllChildViews(true);

    header_ = NULL;
    airplane_ = NULL;
    mobile_account_ = NULL;
    other_wifi_ = NULL;
    other_mobile_ = NULL;
    toggle_wifi_ = NULL;
    toggle_mobile_ = NULL;
    settings_ = NULL;
    proxy_settings_ = NULL;

    AppendHeaderEntry();
    AppendNetworkEntries();

    if (login_ != user::LOGGED_IN_LOCKED) {
      AppendNetworkExtra();
      AppendNetworkToggles();
      AppendSettingsEntry();
   }

    Layout();
  }

 private:
  void AppendHeaderEntry() {
    header_ = CreateDetailedHeaderEntry(IDS_ASH_STATUS_TRAY_NETWORK, this);

    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    info_icon_ = new views::ImageButton(this);
    info_icon_->SetImage(views::CustomButton::BS_NORMAL,
        bundle.GetImageNamed(IDR_AURA_UBER_TRAY_NETWORK_INFO).ToSkBitmap());
    header_->AddChildView(info_icon_);

    AddChildView(header_);
  }

  void AppendNetworkEntries() {
    SystemTrayDelegate* delegate = Shell::GetInstance()->tray_delegate();
    std::vector<NetworkIconInfo> list;
    delegate->GetAvailableNetworks(&list);
    FixedSizedScrollView* scroller = new FixedSizedScrollView;
    views::View* networks = new views::View;
    networks->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kVertical, 0, 0, 1));
    for (size_t i = 0; i < list.size(); i++) {
      HoverHighlightView* container = new HoverHighlightView(this);
      container->AddIconAndLabel(list[i].image,
          list[i].description.empty() ? list[i].name : list[i].description,
          list[i].highlight ? gfx::Font::BOLD : gfx::Font::NORMAL);
      networks->AddChildView(container);
      container->set_border(views::Border::CreateEmptyBorder(0,
          kTrayPopupDetailsIconWidth, 0, 0));
      network_map_[container] = list[i].service_path;
    }

    if (login_ != user::LOGGED_IN_NONE) {
      std::string carrier_id, topup_url;
      if (delegate->GetCellularCarrierInfo(&carrier_id, &topup_url)) {
        if (carrier_id != carrier_id_) {
          carrier_id_ = carrier_id;
          if (!topup_url.empty())
            topup_url_ = topup_url;
        }
        if (!topup_url_.empty()) {
          HoverHighlightView* container = new HoverHighlightView(this);
          container->AddLabel(ui::ResourceBundle::GetSharedInstance().
              GetLocalizedString(IDS_ASH_STATUS_TRAY_MOBILE_VIEW_ACCOUNT),
              gfx::Font::NORMAL);
          AddChildView(container);
          mobile_account_ = container;
        }
      }
    }

    scroller->set_border(views::Border::CreateSolidSidedBorder(1, 0, 1, 0,
        SkColorSetARGB(25, 0, 0, 0)));
    scroller->set_fixed_size(
        gfx::Size(networks->GetPreferredSize().width() +
                  scroller->GetScrollBarWidth(),
                  kNetworkListHeight));
    scroller->SetContentsView(networks);
    AddChildView(scroller);
  }

  void AppendNetworkExtra() {
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    if (delegate->GetWifiEnabled()) {
      HoverHighlightView* container = new HoverHighlightView(this);
      container->AddLabel(rb.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_OTHER_WIFI), gfx::Font::NORMAL);
      AddChildView(container);
      other_wifi_ = container;
    }

    if (delegate->GetCellularEnabled()) {
      if (delegate->GetCellularScanSupported()) {
        HoverHighlightView* container = new HoverHighlightView(this);
        container->AddLabel(rb.GetLocalizedString(
            IDS_ASH_STATUS_TRAY_OTHER_MOBILE), gfx::Font::NORMAL);
        AddChildView(container);
        other_mobile_ = container;
      }
    }
  }

  void AppendNetworkToggles() {
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    if (delegate->GetWifiAvailable()) {
      HoverHighlightView* container = new HoverHighlightView(this);
      container->AddLabel(rb.GetLocalizedString(delegate->GetWifiEnabled() ?
          IDS_ASH_STATUS_TRAY_DISABLE_WIFI :
          IDS_ASH_STATUS_TRAY_ENABLE_WIFI), gfx::Font::NORMAL);
      AddChildView(container);
      toggle_wifi_ = container;
    }

    if (delegate->GetCellularAvailable()) {
      HoverHighlightView* container = new HoverHighlightView(this);
      container->AddLabel(rb.GetLocalizedString(
          delegate->GetCellularEnabled() ?  IDS_ASH_STATUS_TRAY_DISABLE_MOBILE :
                                            IDS_ASH_STATUS_TRAY_ENABLE_MOBILE),
          gfx::Font::NORMAL);
      AddChildView(container);
      toggle_mobile_ = container;
    }
  }

  void AppendAirplaneModeEntry() {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    HoverHighlightView* container = new HoverHighlightView(this);
    container->AddIconAndLabel(
        *rb.GetImageNamed(IDR_AURA_UBER_TRAY_NETWORK_AIRPLANE).ToSkBitmap(),
        rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_AIRPLANE_MODE),
        gfx::Font::NORMAL);
    AddChildView(container);
    airplane_ = container;
  }

  // Adds a settings entry when logged in, and an entry for changing proxy
  // settings otherwise.
  void AppendSettingsEntry() {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    if (login_ != user::LOGGED_IN_NONE) {
      // Settings, only if logged in.
      HoverHighlightView* container = new HoverHighlightView(this);
      container->AddLabel(rb.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_NETWORK_SETTINGS), gfx::Font::NORMAL);
      AddChildView(container);
      settings_ = container;
    } else {
      // Allow changing proxy settings in the login screen.
      HoverHighlightView* container = new HoverHighlightView(this);
      container->AddLabel(rb.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_NETWORK_PROXY_SETTINGS), gfx::Font::NORMAL);
      AddChildView(container);
      proxy_settings_ = container;
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
    container->set_border(views::Border::CreateEmptyBorder(0, 20, 0, 0));

    container->AddChildView(CreateTextLabels(bundle.GetLocalizedString(
        IDS_ASH_STATUS_TRAY_IP), ip_address));
    container->AddChildView(CreateTextLabels(bundle.GetLocalizedString(
        IDS_ASH_STATUS_TRAY_ETHERNET), ethernet_address));
    container->AddChildView(CreateTextLabels(bundle.GetLocalizedString(
        IDS_ASH_STATUS_TRAY_WIFI), wifi_address));

    return container;
  }

  void ToggleInfoBubble() {
    if (info_bubble_) {
      info_bubble_->GetWidget()->CloseNow();
      info_bubble_ = NULL;
      return;
    }
    info_bubble_ = new NonActivatableSettingsBubble(
        info_icon_, CreateNetworkInfoView());
    views::BubbleDelegateView::CreateBubble(info_bubble_);
    info_bubble_->Show();
  }

  // Overridden from views::View.
  virtual void Layout() OVERRIDE {
    views::View::Layout();

    // Align the network info view and icon.
    gfx::Rect header_bounds = header_->bounds();
    gfx::Size icon_size = info_icon_->size();

    info_icon_->SetBounds(
        header_->width() - icon_size.width() - kTrayPopupPaddingHorizontal, 3,
        icon_size.width(), icon_size.height());
  }

  // Overridden from ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE {
    ToggleInfoBubble();
  }

  // Overridden from ViewClickListener.
  virtual void ClickedOn(views::View* sender) OVERRIDE {
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    // If the info bubble was visible, close it when some other item is clicked
    // on.
    if (info_bubble_) {
      info_bubble_->GetWidget()->Close();
      info_bubble_ = NULL;
    }

    if (sender == header_)
      Shell::GetInstance()->tray()->ShowDefaultView();

    if (login_ == user::LOGGED_IN_LOCKED)
      return;

    if (sender == settings_) {
      delegate->ShowNetworkSettings();
    } else if (sender == proxy_settings_) {
      delegate->ChangeProxySettings();
    } else if (sender == mobile_account_) {
      delegate->ShowCellularTopupURL(topup_url_);
    } else if (sender == other_wifi_) {
      delegate->ShowOtherWifi();
    } else if (sender == other_mobile_) {
      delegate->ShowOtherCellular();
    } else if (sender == toggle_wifi_) {
      delegate->ToggleWifi();
    } else if (sender == toggle_mobile_) {
      delegate->ToggleCellular();
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

  user::LoginStatus login_;
  std::map<views::View*, std::string> network_map_;
  views::View* header_;
  views::View* airplane_;
  views::ImageButton* info_icon_;
  views::View* mobile_account_;
  views::View* other_wifi_;
  views::View* other_mobile_;
  views::View* toggle_wifi_;
  views::View* toggle_mobile_;
  views::View* settings_;
  views::View* proxy_settings_;

  views::BubbleDelegateView* info_bubble_;

  DISALLOW_COPY_AND_ASSIGN(NetworkDetailedView);
};

}  // namespace tray

TrayNetwork::TrayNetwork() {
}

TrayNetwork::~TrayNetwork() {
}

views::View* TrayNetwork::CreateTrayView(user::LoginStatus status) {
  tray_.reset(new tray::NetworkTrayView(tray::LIGHT, true /*tray_icon*/));
  return tray_.get();
}

views::View* TrayNetwork::CreateDefaultView(user::LoginStatus status) {
  default_.reset(new tray::NetworkDefaultView(this));
  return default_.get();
}

views::View* TrayNetwork::CreateDetailedView(user::LoginStatus status) {
  detailed_.reset(new tray::NetworkDetailedView(status));
  return detailed_.get();
}

void TrayNetwork::DestroyTrayView() {
  tray_.reset();
}

void TrayNetwork::DestroyDefaultView() {
  default_.reset();
}

void TrayNetwork::DestroyDetailedView() {
  detailed_.reset();
}

void TrayNetwork::OnNetworkRefresh(const NetworkIconInfo& info) {
  if (tray_.get())
    tray_->Update(info);
  if (default_.get())
    default_->Update();
  if (detailed_.get())
    detailed_->Update();
}

}  // namespace internal
}  // namespace ash
