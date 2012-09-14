// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/tray_network.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_details_view.h"
#include "ash/system/tray/tray_item_more.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/tray/tray_notification_view.h"
#include "ash/system/tray/tray_views.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
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
    set_parent_window(ash::Shell::GetContainer(
        anchor->GetWidget()->GetNativeWindow()->GetRootWindow(),
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

using ash::internal::TrayNetwork;

int GetMessageIcon(TrayNetwork::MessageType message_type) {
  switch(message_type) {
    case TrayNetwork::ERROR_CONNECT_FAILED:
      return IDR_AURA_UBER_TRAY_NETWORK_FAILED;
    case TrayNetwork::MESSAGE_DATA_LOW:
      return IDR_AURA_UBER_TRAY_NETWORK_DATA_LOW;
    case TrayNetwork::MESSAGE_DATA_NONE:
      return IDR_AURA_UBER_TRAY_NETWORK_DATA_NONE;
    case TrayNetwork::MESSAGE_DATA_PROMO:
      return IDR_AURA_UBER_TRAY_NOTIFICATION_3G;
  }
  NOTREACHED();
  return 0;
}

}  // namespace

namespace ash {
namespace internal {

namespace tray {

enum ColorTheme {
  LIGHT,
  DARK,
};

class NetworkMessages {
 public:
  struct Message {
    Message() : delegate(NULL) {}
    Message(NetworkTrayDelegate* in_delegate,
            const string16& in_title,
            const string16& in_message,
            const std::vector<string16>& in_links) :
        delegate(in_delegate),
        title(in_title),
        message(in_message),
        links(in_links) {}
    NetworkTrayDelegate* delegate;
    string16 title;
    string16 message;
    std::vector<string16> links;
  };
  typedef std::map<TrayNetwork::MessageType, Message> MessageMap;

  MessageMap& messages() { return messages_; }
  const MessageMap& messages() const { return messages_; }

 private:
  MessageMap messages_;
};

class NetworkTrayView : public TrayItemView {
 public:
  NetworkTrayView(ColorTheme size, bool tray_icon)
      : color_theme_(size), tray_icon_(tray_icon) {
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));

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

class NetworkDetailedView : public TrayDetailsView {
 public:
  NetworkDetailedView() {}

  virtual ~NetworkDetailedView() {}

  virtual TrayNetwork::DetailedViewType GetViewType() const = 0;

  virtual void Update() = 0;
};

class NetworkListDetailedView : public NetworkDetailedView,
                                public views::ButtonListener,
                                public ViewClickListener {
 public:
  NetworkListDetailedView(user::LoginStatus login)
      : login_(login),
        airplane_(NULL),
        info_icon_(NULL),
        button_wifi_(NULL),
        button_mobile_(NULL),
        view_mobile_account_(NULL),
        setup_mobile_account_(NULL),
        other_wifi_(NULL),
        turn_on_wifi_(NULL),
        other_mobile_(NULL),
        settings_(NULL),
        proxy_settings_(NULL),
        info_bubble_(NULL) {
    SystemTrayDelegate* delegate = Shell::GetInstance()->tray_delegate();
    delegate->RequestNetworkScan();
    CreateItems();
    Update();
  }

  virtual ~NetworkListDetailedView() {
    if (info_bubble_)
      info_bubble_->GetWidget()->CloseNow();
  }

  void CreateItems() {
    RemoveAllChildViews(true);

    airplane_ = NULL;
    info_icon_ = NULL;
    button_wifi_ = NULL;
    button_mobile_ = NULL;
    view_mobile_account_ = NULL;
    setup_mobile_account_ = NULL;
    other_wifi_ = NULL;
    turn_on_wifi_ = NULL;
    other_mobile_ = NULL;
    settings_ = NULL;
    proxy_settings_ = NULL;

    AppendNetworkEntries();
    AppendNetworkExtra();
    AppendHeaderEntry();
    AppendHeaderButtons();

    Update();
  }

  // Overridden from NetworkDetailedView:
  virtual TrayNetwork::DetailedViewType GetViewType() const OVERRIDE {
    return TrayNetwork::LIST_VIEW;
  }

  virtual void Update() OVERRIDE {
    UpdateAvailableNetworkList();
    UpdateHeaderButtons();
    UpdateNetworkEntries();
    UpdateNetworkExtra();

    Layout();
  }

 private:
  void AppendHeaderEntry() {
    CreateSpecialRow(IDS_ASH_STATUS_TRAY_NETWORK, this);
  }

  void AppendHeaderButtons() {
    button_wifi_ = new TrayPopupHeaderButton(this,
        IDR_AURA_UBER_TRAY_WIFI_ENABLED,
        IDR_AURA_UBER_TRAY_WIFI_DISABLED,
        IDR_AURA_UBER_TRAY_WIFI_ENABLED_HOVER,
        IDR_AURA_UBER_TRAY_WIFI_DISABLED_HOVER,
        IDS_ASH_STATUS_TRAY_WIFI);
    button_wifi_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISABLE_WIFI));
    button_wifi_->SetToggledTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ENABLE_WIFI));
    footer()->AddButton(button_wifi_);

    button_mobile_ = new TrayPopupHeaderButton(this,
        IDR_AURA_UBER_TRAY_CELLULAR_ENABLED,
        IDR_AURA_UBER_TRAY_CELLULAR_DISABLED,
        IDR_AURA_UBER_TRAY_CELLULAR_ENABLED_HOVER,
        IDR_AURA_UBER_TRAY_CELLULAR_DISABLED_HOVER,
        IDS_ASH_STATUS_TRAY_CELLULAR);
    button_mobile_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISABLE_MOBILE));
    button_mobile_->SetToggledTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ENABLE_MOBILE));
    footer()->AddButton(button_mobile_);

    info_icon_ = new TrayPopupHeaderButton(this,
        IDR_AURA_UBER_TRAY_NETWORK_INFO,
        IDR_AURA_UBER_TRAY_NETWORK_INFO,
        IDR_AURA_UBER_TRAY_NETWORK_INFO_HOVER,
        IDR_AURA_UBER_TRAY_NETWORK_INFO_HOVER,
        IDS_ASH_STATUS_TRAY_NETWORK_INFO);
    info_icon_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_INFO));
    footer()->AddButton(info_icon_);
  }

  void UpdateHeaderButtons() {
    SystemTrayDelegate* delegate = Shell::GetInstance()->tray_delegate();
    button_wifi_->SetToggled(!delegate->GetWifiEnabled());
    button_mobile_->SetToggled(!delegate->GetMobileEnabled());
    button_mobile_->SetVisible(delegate->GetMobileAvailable());
    if (proxy_settings_)
      proxy_settings_->SetEnabled(delegate->IsNetworkConnected());
  }

  void AppendNetworkEntries() {
    CreateScrollableList();

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
  }

  void UpdateAvailableNetworkList() {
    network_list_.clear();
    Shell::GetInstance()->tray_delegate()->GetAvailableNetworks(&network_list_);
  }

  void RefreshNetworkScrollWithUpdatedNetworkList() {
    network_map_.clear();
    std::set<std::string> new_service_paths;

    bool needs_relayout = false;
    views::View* highlighted_view = NULL;

    for (size_t i = 0; i < network_list_.size(); ++i) {
      std::map<std::string, HoverHighlightView*>::const_iterator it =
          service_path_map_.find(network_list_[i].service_path);
      HoverHighlightView* container = NULL;
      if (it == service_path_map_.end()) {
        // Create a new view.
        container = new HoverHighlightView(this);
        container->set_fixed_height(kTrayPopupItemHeight);
        container->AddIconAndLabel(network_list_[i].image,
            network_list_[i].description.empty() ?
                network_list_[i].name : network_list_[i].description,
            network_list_[i].highlight ?
                gfx::Font::BOLD : gfx::Font::NORMAL);
        scroll_content()->AddChildViewAt(container, i);
        container->set_border(views::Border::CreateEmptyBorder(0,
            kTrayPopupPaddingHorizontal, 0, 0));
        needs_relayout = true;
      } else {
        container = it->second;
        container->RemoveAllChildViews(true);
        container->AddIconAndLabel(network_list_[i].image,
            network_list_[i].description.empty() ?
                network_list_[i].name : network_list_[i].description,
            network_list_[i].highlight ? gfx::Font::BOLD : gfx::Font::NORMAL);
        container->Layout();
        container->SchedulePaint();

        // Reordering the view if necessary.
        views::View* child = scroll_content()->child_at(i);
        if (child != container) {
          scroll_content()->ReorderChildView(container, i);
          needs_relayout = true;
        }
      }

      if (network_list_[i].highlight)
        highlighted_view = container;
      network_map_[container] = network_list_[i].service_path;
      service_path_map_[network_list_[i].service_path] = container;
      new_service_paths.insert(network_list_[i].service_path);
    }

    std::set<std::string> remove_service_paths;
    for (std::map<std::string, HoverHighlightView*>::const_iterator it =
             service_path_map_.begin(); it != service_path_map_.end(); ++it) {
      if (new_service_paths.find(it->first) == new_service_paths.end()) {
        remove_service_paths.insert(it->first);
        scroll_content()->RemoveChildView(it->second);
        needs_relayout = true;
      }
    }

    for (std::set<std::string>::const_iterator remove_it =
             remove_service_paths.begin();
         remove_it != remove_service_paths.end(); ++remove_it) {
      service_path_map_.erase(*remove_it);
    }

    if (needs_relayout) {
      scroll_content()->SizeToPreferredSize();
      static_cast<views::View*>(scroller())->Layout();
      if (highlighted_view)
        scroll_content()->ScrollRectToVisible(highlighted_view->bounds());
    }
  }

  void RefreshNetworkScrollWithEmptyNetworkList() {
    scroll_content()->RemoveAllChildViews(true);
    HoverHighlightView* container = new HoverHighlightView(this);
    container->set_fixed_height(kTrayPopupItemHeight);

    if (Shell::GetInstance()->tray_delegate()->GetWifiEnabled()) {
      NetworkIconInfo info;
      Shell::GetInstance()->tray_delegate()->
          GetMostRelevantNetworkIcon(&info, true);
      container->AddIconAndLabel(info.image,
          info.description,
          gfx::Font::NORMAL);
      container->set_border(views::Border::CreateEmptyBorder(0,
          kTrayPopupPaddingHorizontal, 0, 0));

    } else {
      container->AddLabel(ui::ResourceBundle::GetSharedInstance().
          GetLocalizedString(IDS_ASH_STATUS_TRAY_NETWORK_WIFI_DISABLED),
          gfx::Font::NORMAL);
      AddChildView(container);
    }

    scroll_content()->AddChildViewAt(container, 0);
    scroll_content()->SizeToPreferredSize();
    static_cast<views::View*>(scroller())->Layout();
  }

  void UpdateNetworkEntries() {
    if (network_list_.size() > 0 )
      RefreshNetworkScrollWithUpdatedNetworkList();
    else
      RefreshNetworkScrollWithEmptyNetworkList();

    view_mobile_account_->SetVisible(false);
    setup_mobile_account_->SetVisible(false);

    if (login_ == user::LOGGED_IN_NONE)
      return;

    std::string carrier_id, topup_url, setup_url;
    if (Shell::GetInstance()->tray_delegate()->
            GetCellularCarrierInfo(&carrier_id,
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

    turn_on_wifi_ = new TrayPopupTextButton(this,
        rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_TURN_ON_WIFI));
    bottom_row->AddTextButton(turn_on_wifi_);

    other_mobile_ = new TrayPopupTextButton(this,
        rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_OTHER_MOBILE));
    bottom_row->AddTextButton(other_mobile_);

    CreateSettingsEntry();
    DCHECK(settings_ || proxy_settings_);
    bottom_row->AddTextButton(settings_ ? settings_ : proxy_settings_);

    AddChildView(bottom_row);
  }

  void UpdateNetworkExtra() {
    if (login_ == user::LOGGED_IN_LOCKED)
      return;

    SystemTrayDelegate* delegate = Shell::GetInstance()->tray_delegate();
    if (network_list_.size() == 0 && !delegate->GetWifiEnabled()) {
      turn_on_wifi_->SetVisible(true);
      other_wifi_->SetVisible(false);
    } else {
      turn_on_wifi_->SetVisible(false);
      other_wifi_->SetVisible(true);
      other_wifi_->SetEnabled(delegate->GetWifiEnabled());
    }
    other_mobile_->SetVisible(delegate->GetMobileAvailable() &&
                              delegate->GetMobileScanSupported());
    if (other_mobile_->visible())
      other_mobile_->SetEnabled(delegate->GetMobileEnabled());

    turn_on_wifi_->parent()->Layout();
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
                             const ui::Event& event) OVERRIDE {
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
    else if (sender == button_mobile_)
      delegate->ToggleMobile();
    else if (sender == settings_)
      delegate->ShowNetworkSettings();
    else if (sender == proxy_settings_)
      delegate->ChangeProxySettings();
    else if (sender == other_mobile_)
      delegate->ShowOtherCellular();
    else if (sender == other_wifi_)
      delegate->ShowOtherWifi();
    else if (sender == turn_on_wifi_)
      delegate->ToggleWifi();
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

    if (sender == footer()->content()) {
      Shell::GetInstance()->system_tray()->ShowDefaultView(BUBBLE_USE_EXISTING);
      return;
    }

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
  std::map<std::string, HoverHighlightView*> service_path_map_;
  std::vector<NetworkIconInfo> network_list_;
  views::View* airplane_;
  TrayPopupHeaderButton* info_icon_;
  TrayPopupHeaderButton* button_wifi_;
  TrayPopupHeaderButton* button_mobile_;
  views::View* view_mobile_account_;
  views::View* setup_mobile_account_;
  TrayPopupTextButton* other_wifi_;
  TrayPopupTextButton* turn_on_wifi_;
  TrayPopupTextButton* other_mobile_;
  TrayPopupTextButton* settings_;
  TrayPopupTextButton* proxy_settings_;

  views::BubbleDelegateView* info_bubble_;

  DISALLOW_COPY_AND_ASSIGN(NetworkListDetailedView);
};

class NetworkWifiDetailedView : public NetworkDetailedView {
 public:
  explicit NetworkWifiDetailedView(bool wifi_enabled) {
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
                                          kTrayPopupPaddingHorizontal,
                                          10,
                                          kTrayPopupPaddingBetweenItems));
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    views::ImageView* image = new views::ImageView;
    const int image_id = wifi_enabled ?
        IDR_AURA_UBER_TRAY_WIFI_ENABLED : IDR_AURA_UBER_TRAY_WIFI_DISABLED;
    image->SetImage(bundle.GetImageNamed(image_id).ToImageSkia());
    AddChildView(image);

    const int string_id = wifi_enabled ?
        IDS_ASH_STATUS_TRAY_NETWORK_WIFI_ENABLED:
        IDS_ASH_STATUS_TRAY_NETWORK_WIFI_DISABLED;
    views::Label* label =
        new views::Label(bundle.GetLocalizedString(string_id));
    label->SetMultiLine(true);
    label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    AddChildView(label);
  }

  virtual ~NetworkWifiDetailedView() {}

  // Overridden from NetworkDetailedView:
  virtual TrayNetwork::DetailedViewType GetViewType() const OVERRIDE {
    return TrayNetwork::WIFI_VIEW;
  }

  virtual void Update() OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkWifiDetailedView);
};

class NetworkMessageView : public views::View,
                           public views::LinkListener {
 public:
  NetworkMessageView(TrayNetwork* tray,
                     TrayNetwork::MessageType message_type,
                     const NetworkMessages::Message& network_msg)
      : tray_(tray),
        message_type_(message_type) {
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));

    if (!network_msg.title.empty()) {
      views::Label* title = new views::Label(network_msg.title);
      title->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
      title->SetFont(title->font().DeriveFont(0, gfx::Font::BOLD));
      AddChildView(title);
    }

    if (!network_msg.message.empty()) {
      views::Label* message = new views::Label(network_msg.message);
      message->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
      message->SetMultiLine(true);
      message->SizeToFit(kTrayNotificationContentsWidth);
      AddChildView(message);
    }

    if (!network_msg.links.empty()) {
      for (size_t i = 0; i < network_msg.links.size(); ++i) {
        views::Link* link = new views::Link(network_msg.links[i]);
        link->set_id(i);
        link->set_listener(this);
        link->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
        link->SetMultiLine(true);
        link->SizeToFit(kTrayNotificationContentsWidth);
        AddChildView(link);
      }
    }
  }

  virtual ~NetworkMessageView() {
  }

  // Overridden from views::LinkListener.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE {
    tray_->LinkClicked(message_type_, source->id());
  }

  TrayNetwork::MessageType message_type() const { return message_type_; }

 private:
  TrayNetwork* tray_;
  TrayNetwork::MessageType message_type_;

  DISALLOW_COPY_AND_ASSIGN(NetworkMessageView);
};

class NetworkNotificationView : public TrayNotificationView {
 public:
  explicit NetworkNotificationView(TrayNetwork* tray)
      : TrayNotificationView(tray, 0) {
    CreateMessageView();
    InitView(network_message_view_);
    SetIconImage(*ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        GetMessageIcon(network_message_view_->message_type())));
  }

  // Overridden from TrayNotificationView.
  virtual void OnClose() OVERRIDE {
    tray_network()->ClearNetworkMessage(network_message_view_->message_type());
  }

  virtual void OnClickAction() OVERRIDE {
    if (network_message_view_->message_type() !=
        TrayNetwork::MESSAGE_DATA_PROMO)
      tray()->PopupDetailedView(0, true);
  }

  void Update() {
    CreateMessageView();
    UpdateViewAndImage(network_message_view_,
        *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            GetMessageIcon(network_message_view_->message_type())));
  }

 private:
  TrayNetwork* tray_network() {
    return static_cast<TrayNetwork*>(tray());
  }

  void CreateMessageView() {
    // Display the first (highest priority) message.
    CHECK(!tray_network()->messages()->messages().empty());
    NetworkMessages::MessageMap::const_iterator iter =
        tray_network()->messages()->messages().begin();
    network_message_view_ =
        new NetworkMessageView(tray_network(), iter->first, iter->second);
  }

  tray::NetworkMessageView* network_message_view_;

  DISALLOW_COPY_AND_ASSIGN(NetworkNotificationView);
};

}  // namespace tray

TrayNetwork::TrayNetwork()
    : tray_(NULL),
      default_(NULL),
      detailed_(NULL),
      notification_(NULL),
      messages_(new tray::NetworkMessages()),
      request_wifi_view_(false) {
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
  if (request_wifi_view_) {
    SystemTrayDelegate* delegate = Shell::GetInstance()->tray_delegate();
    // The Wi-Fi state is not toggled yet at this point.
    detailed_ = new tray::NetworkWifiDetailedView(!delegate->GetWifiEnabled());
    request_wifi_view_ = false;
  } else {
    detailed_ = new tray::NetworkListDetailedView(status);
  }
  return detailed_;
}

views::View* TrayNetwork::CreateNotificationView(user::LoginStatus status) {
  CHECK(notification_ == NULL);
  if (messages_->messages().empty())
    return NULL;  // Message has already been cleared.
  notification_ = new tray::NetworkNotificationView(this);
  return notification_;
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

void TrayNetwork::DestroyNotificationView() {
  notification_ = NULL;
}

void TrayNetwork::UpdateAfterLoginStatusChange(user::LoginStatus status) {
}

void TrayNetwork::UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) {
  SetTrayImageItemBorder(tray_, alignment);
}

void TrayNetwork::OnNetworkRefresh(const NetworkIconInfo& info) {
  if (tray_)
    tray_->Update(info);
  if (default_)
    default_->Update();
  if (detailed_)
    detailed_->Update();
}

void TrayNetwork::SetNetworkMessage(NetworkTrayDelegate* delegate,
                                   MessageType message_type,
                                   const string16& title,
                                   const string16& message,
                                   const std::vector<string16>& links) {
  messages_->messages()[message_type] =
      tray::NetworkMessages::Message(delegate, title, message, links);
  if (notification_)
    notification_->Update();
  else
    ShowNotificationView();
}

void TrayNetwork::ClearNetworkMessage(MessageType message_type) {
  messages_->messages().erase(message_type);
  if (messages_->messages().empty()) {
    if (notification_)
      HideNotificationView();
    return;
  }
  if (notification_)
    notification_->Update();
  else
    ShowNotificationView();
}

void TrayNetwork::OnWillToggleWifi() {
  if (!detailed_ || detailed_->GetViewType() == WIFI_VIEW) {
    request_wifi_view_ = true;
    PopupDetailedView(kTrayPopupAutoCloseDelayForTextInSeconds, false);
  }
}

void TrayNetwork::LinkClicked(MessageType message_type, int link_id) {
  tray::NetworkMessages::MessageMap::const_iterator iter =
      messages()->messages().find(message_type);
  if (iter != messages()->messages().end() && iter->second.delegate)
    iter->second.delegate->NotificationLinkClicked(link_id);
}

}  // namespace internal
}  // namespace ash
