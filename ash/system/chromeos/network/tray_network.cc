// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/tray_network.h"

#include "ash/shell.h"
#include "ash/system/chromeos/network/network_list_detailed_view.h"
#include "ash/system/chromeos/network/network_list_detailed_view_base.h"
#include "ash/system/chromeos/network/network_state_list_detailed_view.h"
#include "ash/system/chromeos/network/tray_network_state_observer.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_more.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/tray/tray_notification_view.h"
#include "base/command_line.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/network/network_state_handler.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace {

using ash::internal::TrayNetwork;

int GetMessageIcon(
    TrayNetwork::MessageType message_type,
    TrayNetwork::NetworkType network_type) {
  switch(message_type) {
    case TrayNetwork::ERROR_CONNECT_FAILED:
      if (TrayNetwork::NETWORK_CELLULAR == network_type)
        return IDR_AURA_UBER_TRAY_CELLULAR_NETWORK_FAILED;
      else
        return IDR_AURA_UBER_TRAY_NETWORK_FAILED;
    case TrayNetwork::MESSAGE_DATA_PROMO:
      if (network_type == TrayNetwork::NETWORK_CELLULAR_LTE)
        return IDR_AURA_UBER_TRAY_NOTIFICATION_LTE;
      else
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
            TrayNetwork::NetworkType network_type,
            const string16& in_title,
            const string16& in_message,
            const std::vector<string16>& in_links) :
        delegate(in_delegate),
        network_type_(network_type),
        title(in_title),
        message(in_message),
        links(in_links) {}
    NetworkTrayDelegate* delegate;
    TrayNetwork::NetworkType network_type_;
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
  NetworkTrayView(SystemTrayItem* owner, ColorTheme size)
      : TrayItemView(owner), color_theme_(size) {
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));

    image_view_ = color_theme_ == DARK ?
        new FixedSizedImageView(0, kTrayPopupItemHeight) :
        new views::ImageView;
    AddChildView(image_view_);

    NetworkIconInfo info;
    Shell::GetInstance()->system_tray_delegate()->
        GetMostRelevantNetworkIcon(&info, false);
    Update(info);
  }

  virtual ~NetworkTrayView() {}

  void Update(const NetworkIconInfo& info) {
    image_view_->SetImage(info.image);
    SetVisible(info.tray_icon_visible);
    SchedulePaint();
    UpdateConnectionStatus(info.name, info.connected);
  }

  // views::View override.
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE {
    state->name = connection_status_string_;
    state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
  }

 private:
  // Updates connection status and notifies accessibility event when necessary.
  void UpdateConnectionStatus(const string16& network_name, bool connected) {
    string16 new_connection_status_string;
    if (connected) {
      new_connection_status_string = l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CONNECTED_TOOLTIP, network_name);
    }
    if (new_connection_status_string != connection_status_string_) {
      connection_status_string_ = new_connection_status_string;
      if(!connection_status_string_.empty()) {
        GetWidget()->NotifyAccessibilityEvent(
            this, ui::AccessibilityTypes::EVENT_ALERT, true);
      }
    }
  }

  views::ImageView* image_view_;
  ColorTheme color_theme_;
  string16 connection_status_string_;

  DISALLOW_COPY_AND_ASSIGN(NetworkTrayView);
};

class NetworkDefaultView : public TrayItemMore {
 public:
  NetworkDefaultView(SystemTrayItem* owner, bool show_more)
      : TrayItemMore(owner, show_more) {
    Update();
  }

  virtual ~NetworkDefaultView() {}

  void Update() {
    NetworkIconInfo info;
    Shell::GetInstance()->system_tray_delegate()->
        GetMostRelevantNetworkIcon(&info, true);
    SetImage(&info.image);
    SetLabel(info.description);
    SetAccessibleName(info.description);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkDefaultView);
};

class NetworkWifiDetailedView : public NetworkDetailedView {
 public:
  NetworkWifiDetailedView(SystemTrayItem* owner, bool wifi_enabled)
      : NetworkDetailedView(owner) {
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
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    AddChildView(label);
  }

  virtual ~NetworkWifiDetailedView() {}

  // Overridden from NetworkDetailedView:

  virtual void Init() OVERRIDE {
  }

  virtual NetworkDetailedView::DetailedViewType GetViewType() const OVERRIDE {
    return NetworkDetailedView::WIFI_VIEW;
  }

  virtual void ManagerChanged() OVERRIDE {
  }

  virtual void NetworkListChanged(const NetworkStateList& networks) OVERRIDE {
  }

  virtual void NetworkServiceChanged(
      const chromeos::NetworkState* network) OVERRIDE {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkWifiDetailedView);
};

class NetworkMessageView : public views::View,
                           public views::LinkListener {
 public:
  NetworkMessageView(TrayNetwork* owner,
                     TrayNetwork::MessageType message_type,
                     const NetworkMessages::Message& network_msg)
      : owner_(owner),
        message_type_(message_type),
        network_type_(network_msg.network_type_) {
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));

    if (!network_msg.title.empty()) {
      views::Label* title = new views::Label(network_msg.title);
      title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      title->SetFont(title->font().DeriveFont(0, gfx::Font::BOLD));
      AddChildView(title);
    }

    if (!network_msg.message.empty()) {
      views::Label* message = new views::Label(network_msg.message);
      message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      message->SetMultiLine(true);
      message->SizeToFit(kTrayNotificationContentsWidth);
      AddChildView(message);
    }

    if (!network_msg.links.empty()) {
      for (size_t i = 0; i < network_msg.links.size(); ++i) {
        views::Link* link = new views::Link(network_msg.links[i]);
        link->set_id(i);
        link->set_listener(this);
        link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
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
    owner_->LinkClicked(message_type_, source->id());
  }

  TrayNetwork::MessageType message_type() const { return message_type_; }
  TrayNetwork::NetworkType network_type() const { return network_type_; }

 private:
  TrayNetwork* owner_;
  TrayNetwork::MessageType message_type_;
  TrayNetwork::NetworkType network_type_;

  DISALLOW_COPY_AND_ASSIGN(NetworkMessageView);
};

class NetworkNotificationView : public TrayNotificationView {
 public:
  explicit NetworkNotificationView(TrayNetwork* owner)
      : TrayNotificationView(owner, 0) {
    CreateMessageView();
    InitView(network_message_view_);
    SetIconImage(*ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        GetMessageIcon(network_message_view_->message_type(),
            network_message_view_->network_type())));
  }

  // Overridden from TrayNotificationView.
  virtual void OnClose() OVERRIDE {
    tray_network()->ClearNetworkMessage(network_message_view_->message_type());
  }

  virtual void OnClickAction() OVERRIDE {
    if (network_message_view_->message_type() !=
        TrayNetwork::MESSAGE_DATA_PROMO)
      owner()->PopupDetailedView(0, true);
  }

  void Update() {
    CreateMessageView();
    UpdateViewAndImage(network_message_view_,
        *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            GetMessageIcon(network_message_view_->message_type(),
                network_message_view_->network_type())));
  }

 private:
  TrayNetwork* tray_network() {
    return static_cast<TrayNetwork*>(owner());
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

TrayNetwork::TrayNetwork(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      tray_(NULL),
      default_(NULL),
      detailed_(NULL),
      notification_(NULL),
      messages_(new tray::NetworkMessages()),
      request_wifi_view_(false) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kEnableNewNetworkHandlers)) {
    network_state_observer_.reset(new TrayNetworkStateObserver(this));
  }
  Shell::GetInstance()->system_tray_notifier()->AddNetworkObserver(this);
}

TrayNetwork::~TrayNetwork() {
  Shell::GetInstance()->system_tray_notifier()->RemoveNetworkObserver(this);
}

views::View* TrayNetwork::CreateTrayView(user::LoginStatus status) {
  CHECK(tray_ == NULL);
  tray_ = new tray::NetworkTrayView(this, tray::LIGHT);
  return tray_;
}

views::View* TrayNetwork::CreateDefaultView(user::LoginStatus status) {
  CHECK(default_ == NULL);
  default_ =
      new tray::NetworkDefaultView(this, status != user::LOGGED_IN_LOCKED);
  return default_;
}

views::View* TrayNetwork::CreateDetailedView(user::LoginStatus status) {
  CHECK(detailed_ == NULL);
  // Clear any notifications when showing the detailed view.
  messages_->messages().clear();
  HideNotificationView();
  if (request_wifi_view_) {
    SystemTrayDelegate* delegate = Shell::GetInstance()->system_tray_delegate();
    // The Wi-Fi state is not toggled yet at this point.
    detailed_ = new tray::NetworkWifiDetailedView(this,
                                                  !delegate->GetWifiEnabled());
    request_wifi_view_ = false;
  } else {
    if (CommandLine::ForCurrentProcess()->HasSwitch(
            chromeos::switches::kEnableNewNetworkHandlers)) {
      detailed_ = new tray::NetworkStateListDetailedView(this, status);
    } else {
      detailed_ = new tray::NetworkListDetailedView(
          this, status, IDS_ASH_STATUS_TRAY_NETWORK);
    }
    detailed_->Init();
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
    detailed_->ManagerChanged();
}

void TrayNetwork::SetNetworkMessage(NetworkTrayDelegate* delegate,
                                   MessageType message_type,
                                   NetworkType network_type,
                                   const string16& title,
                                   const string16& message,
                                   const std::vector<string16>& links) {
  messages_->messages()[message_type] = tray::NetworkMessages::Message(
      delegate, network_type, title, message, links);
  if (notification_)
    notification_->Update();
  else
    ShowNotificationView();
}

void TrayNetwork::ClearNetworkMessage(MessageType message_type) {
  messages_->messages().erase(message_type);
  if (messages_->messages().empty()) {
    HideNotificationView();
    return;
  }
  if (notification_)
    notification_->Update();
  else
    ShowNotificationView();
}

void TrayNetwork::OnWillToggleWifi() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kEnableNewNetworkHandlers)) {
    return;  // Handled in TrayNetworkStateObserver::NetworkManagerChanged()
  }
  if (!detailed_ ||
      detailed_->GetViewType() == tray::NetworkDetailedView::WIFI_VIEW) {
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
