// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/tray_network.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/system/chromeos/network/network_icon_animation.h"
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
#include "base/utf_string_conversions.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_state.h"
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

bool UseNewNetworkHandlers() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kAshEnableNewNetworkStatusArea);
}

}  // namespace

using chromeos::NetworkState;
using chromeos::NetworkStateHandler;

namespace ash {
namespace internal {

namespace tray {

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

class NetworkTrayView : public TrayItemView,
                        public network_icon::AnimationObserver {
 public:
  explicit NetworkTrayView(TrayNetwork* network_tray)
      : TrayItemView(network_tray),
        network_tray_(network_tray) {
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));

    image_view_ = new views::ImageView;
    AddChildView(image_view_);

    NetworkIconInfo info;
    if (UseNewNetworkHandlers()) {
      UpdateNetworkStateHandlerIcon();
      network_icon::NetworkIconAnimation::GetInstance()->AddObserver(this);
    } else {
      Shell::GetInstance()->system_tray_delegate()->
          GetMostRelevantNetworkIcon(&info, false);
      UpdateIcon(info.tray_icon_visible, info.image);
    }
  }

  virtual ~NetworkTrayView() {
    if (UseNewNetworkHandlers())
      network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
  }

  void Update(const NetworkIconInfo& info) {
    if (UseNewNetworkHandlers())
      return;
    UpdateIcon(info.tray_icon_visible, info.image);
    UpdateConnectionStatus(info.name, info.connected);
  }

  void UpdateNetworkStateHandlerIcon() {
    DCHECK(UseNewNetworkHandlers());
    NetworkStateHandler* handler = NetworkStateHandler::Get();
    gfx::ImageSkia image;
    string16 name;
    network_tray_->GetNetworkStateHandlerImageAndLabel(
        network_icon::ICON_TYPE_TRAY, &image, &name);
    bool show_in_tray = !image.isNull();
    UpdateIcon(show_in_tray, image);
    const NetworkState* connected_network = handler->ConnectedNetworkByType(
        NetworkStateHandler::kMatchTypeNonVirtual);
    if (connected_network)
      UpdateConnectionStatus(UTF8ToUTF16(connected_network->name()), true);
    else
      UpdateConnectionStatus(string16(), false);
  }

  // views::View override.
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE {
    state->name = connection_status_string_;
    state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
  }

  // network_icon::AnimationObserver
  virtual void NetworkIconChanged() OVERRIDE {
    if (UseNewNetworkHandlers())
      UpdateNetworkStateHandlerIcon();
  }

 private:
  // Updates connection status and notifies accessibility event when necessary.
  void UpdateConnectionStatus(const string16& network_name, bool connected) {
    string16 new_connection_status_string;
    if (connected) {
      new_connection_status_string = l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CONNECTED, network_name);
    }
    if (new_connection_status_string != connection_status_string_) {
      connection_status_string_ = new_connection_status_string;
      if(!connection_status_string_.empty()) {
        GetWidget()->NotifyAccessibilityEvent(
            this, ui::AccessibilityTypes::EVENT_ALERT, true);
      }
    }
  }

  void UpdateIcon(bool tray_icon_visible, const gfx::ImageSkia& image) {
    image_view_->SetImage(image);
    SetVisible(tray_icon_visible);
    SchedulePaint();
  }

  TrayNetwork* network_tray_;
  views::ImageView* image_view_;
  string16 connection_status_string_;

  DISALLOW_COPY_AND_ASSIGN(NetworkTrayView);
};

class NetworkDefaultView : public TrayItemMore,
                           public network_icon::AnimationObserver {
 public:
  NetworkDefaultView(TrayNetwork* network_tray, bool show_more)
      : TrayItemMore(network_tray, show_more),
        network_tray_(network_tray) {
    Update();
    if (UseNewNetworkHandlers())
      network_icon::NetworkIconAnimation::GetInstance()->AddObserver(this);
  }

  virtual ~NetworkDefaultView() {
    if (UseNewNetworkHandlers())
      network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
  }

  void Update() {
    if (UseNewNetworkHandlers()) {
      gfx::ImageSkia image;
      string16 label;
      network_tray_->GetNetworkStateHandlerImageAndLabel(
          network_icon::ICON_TYPE_DEFAULT_VIEW, &image, &label);
      SetImage(&image);
      SetLabel(label);
      SetAccessibleName(label);
    } else {
      NetworkIconInfo info;
      Shell::GetInstance()->system_tray_delegate()->
          GetMostRelevantNetworkIcon(&info, true);
      SetImage(&info.image);
      SetLabel(info.description);
      SetAccessibleName(info.description);
    }
  }

  // network_icon::AnimationObserver
  virtual void NetworkIconChanged() OVERRIDE {
    Update();
  }

 private:
  TrayNetwork* network_tray_;

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

  virtual void NetworkListChanged() OVERRIDE {
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
  NetworkMessageView(TrayNetwork* tray_network,
                     TrayNetwork::MessageType message_type,
                     const NetworkMessages::Message& network_msg)
      : tray_network_(tray_network),
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
    tray_network_->LinkClicked(message_type_, source->id());
  }

  TrayNetwork::MessageType message_type() const { return message_type_; }
  TrayNetwork::NetworkType network_type() const { return network_type_; }

 private:
  TrayNetwork* tray_network_;
  TrayNetwork::MessageType message_type_;
  TrayNetwork::NetworkType network_type_;

  DISALLOW_COPY_AND_ASSIGN(NetworkMessageView);
};

class NetworkNotificationView : public TrayNotificationView {
 public:
  explicit NetworkNotificationView(TrayNetwork* tray_network)
      : TrayNotificationView(tray_network, 0),
        tray_network_(tray_network) {
    CreateMessageView();
    InitView(network_message_view_);
    SetIconImage(*ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        GetMessageIcon(network_message_view_->message_type(),
            network_message_view_->network_type())));
  }

  // Overridden from TrayNotificationView.
  virtual void OnClose() OVERRIDE {
    tray_network_->ClearNetworkMessage(network_message_view_->message_type());
  }

  virtual void OnClickAction() OVERRIDE {
    if (network_message_view_->message_type() !=
        TrayNetwork::MESSAGE_DATA_PROMO)
      tray_network_->PopupDetailedView(0, true);
  }

  void Update() {
    CreateMessageView();
    UpdateViewAndImage(network_message_view_,
        *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            GetMessageIcon(network_message_view_->message_type(),
                network_message_view_->network_type())));
  }

 private:
  void CreateMessageView() {
    // Display the first (highest priority) message.
    CHECK(!tray_network_->messages()->messages().empty());
    NetworkMessages::MessageMap::const_iterator iter =
        tray_network_->messages()->messages().begin();
    network_message_view_ =
        new NetworkMessageView(tray_network_, iter->first, iter->second);
  }

  TrayNetwork* tray_network_;
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
      request_wifi_view_(false),
      uninitialized_msg_(0) {
  if (UseNewNetworkHandlers())
    network_state_observer_.reset(new TrayNetworkStateObserver(this));
  Shell::GetInstance()->system_tray_notifier()->AddNetworkObserver(this);
}

TrayNetwork::~TrayNetwork() {
  Shell::GetInstance()->system_tray_notifier()->RemoveNetworkObserver(this);
}

views::View* TrayNetwork::CreateTrayView(user::LoginStatus status) {
  CHECK(tray_ == NULL);
  tray_ = new tray::NetworkTrayView(this);
  return tray_;
}

views::View* TrayNetwork::CreateDefaultView(user::LoginStatus status) {
  CHECK(default_ == NULL);
  CHECK(tray_ != NULL);
  default_ = new tray::NetworkDefaultView(
      this, status != user::LOGGED_IN_LOCKED);
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
    if (UseNewNetworkHandlers()) {
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
  if (UseNewNetworkHandlers())
    return;  // Handled in TrayNetworkStateObserver::NetworkManagerChanged()
  if (!detailed_ ||
      detailed_->GetViewType() == tray::NetworkDetailedView::WIFI_VIEW) {
    request_wifi_view_ = true;
    PopupDetailedView(kTrayPopupAutoCloseDelayForTextInSeconds, false);
  }
}

void TrayNetwork::TrayNetworkUpdated() {
  if (tray_  && UseNewNetworkHandlers())
    tray_->UpdateNetworkStateHandlerIcon();
  if (default_)
    default_->Update();
}

void TrayNetwork::NetworkServiceChanged(const chromeos::NetworkState* network) {
  if (!network->IsConnectingState())
    connecting_networks_.erase(network->path());
}

void TrayNetwork::ConnectToNetwork(const std::string& service_path) {
  DCHECK(UseNewNetworkHandlers());
  const NetworkState* network =
      NetworkStateHandler::Get()->GetNetworkState(service_path);
  if (!network)
    return;
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kEnableNewNetworkConfigurationHandlers) &&
      !network->IsConnectedState()) {
    chromeos::NetworkConfigurationHandler::Get()->Connect(
        service_path,
        base::Bind(&base::DoNothing),
        chromeos::network_handler::ErrorCallback());
    connecting_networks_.insert(service_path);
  } else {
    // This will show the settings UI for a connected network.
    // TODO(stevenjb): Change the API to explicitly show network settings.
    Shell::GetInstance()->system_tray_delegate()->ConnectToNetwork(
        service_path);
  }
}

bool TrayNetwork::HasConnectingNetwork(const std::string& service_path) {
  return connecting_networks_.count(service_path) > 0;
}

void TrayNetwork::GetNetworkStateHandlerImageAndLabel(
    network_icon::IconType icon_type,
    gfx::ImageSkia* image,
    string16* label) {
  NetworkStateHandler* handler = NetworkStateHandler::Get();
  const NetworkState* network = handler->ConnectedNetworkByType(
      NetworkStateHandler::kMatchTypeNonVirtual);
  if (network && network->type() == flimflam::kTypeEthernet &&
      icon_type == network_icon::ICON_TYPE_TRAY) {
    *image = gfx::ImageSkia();  // Don't show ethernet in the tray.
    return;
  }
  const NetworkState* connecting_network = handler->ConnectingNetworkByType(
      NetworkStateHandler::kMatchTypeWireless);
  // If we are connecting to a network, and there is either no connected
  // network, or the connection was user requested, use the connecting
  // network.
  if (connecting_network &&
      (!network ||
       HasConnectingNetwork(connecting_network->path()))) {
    network = connecting_network;
  }
  if (!network) {
    // If no connecting network, check for cellular initializing.
    int uninitialized_msg = GetUninitializedMsg();
    if (uninitialized_msg != 0) {
      *image = network_icon::GetImageForConnectingNetwork(
          icon_type, flimflam::kTypeCellular);
      if (label)
        *label = l10n_util::GetStringUTF16(uninitialized_msg);
    } else {
      // Otherwise show the disconnected wifi icon.
      *image = network_icon::GetImageForDisconnectedNetwork(
          icon_type, flimflam::kTypeWifi);
      if (label) {
        *label = l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_NETWORK_NOT_CONNECTED);
      }
    }
    return;
  }
  // Get icon and label for connected or connecting network.
  *image = network_icon::GetImageForNetwork(network, icon_type);
  if (label)
    *label = network_icon::GetLabelForNetwork(network, icon_type);
}

int TrayNetwork::GetUninitializedMsg() {
  NetworkStateHandler* handler = NetworkStateHandler::Get();
  if (handler->TechnologyUninitialized(
          NetworkStateHandler::kMatchTypeMobile)) {
    uninitialized_msg_ = IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR;
    uninitialized_state_time_ = base::Time::Now();
    return uninitialized_msg_;
  } else if (handler->GetScanningByType(
      NetworkStateHandler::kMatchTypeMobile)) {
    uninitialized_msg_ = IDS_ASH_STATUS_TRAY_CELLULAR_SCANNING;
    uninitialized_state_time_ = base::Time::Now();
    return uninitialized_msg_;
  }
  // There can be a delay between leaving the Initializing state and when
  // a Cellular device shows up, so keep showing the initializing
  // animation for a bit to avoid flashing the disconnect icon.
  const int kInitializingDelaySeconds = 1;
  base::TimeDelta dtime = base::Time::Now() - uninitialized_state_time_;
  if (dtime.InSeconds() < kInitializingDelaySeconds)
    return uninitialized_msg_;
  return 0;
}

void TrayNetwork::LinkClicked(MessageType message_type, int link_id) {
  tray::NetworkMessages::MessageMap::const_iterator iter =
      messages()->messages().find(message_type);
  if (iter != messages()->messages().end() && iter->second.delegate)
    iter->second.delegate->NotificationLinkClicked(link_id);
}

}  // namespace internal
}  // namespace ash
