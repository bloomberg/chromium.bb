// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/tray_network.h"

#include "ash/ash_switches.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/shelf/shelf_util.h"
#include "ash/shell.h"
#include "ash/system/chromeos/network/network_state_list_detailed_view.h"
#include "ash/system/chromeos/network/tray_network_state_observer.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_more.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/tray/tray_utils.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "grit/ui_chromeos_strings.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/network/network_icon.h"
#include "ui/chromeos/network/network_icon_animation.h"
#include "ui/chromeos/network/network_icon_animation_observer.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

using chromeos::NetworkHandler;
using chromeos::NetworkState;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;

namespace ash {
namespace tray {

class NetworkTrayView : public TrayItemView,
                        public ui::network_icon::AnimationObserver {
 public:
  explicit NetworkTrayView(TrayNetwork* network_tray)
      : TrayItemView(network_tray) {
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));

    image_view_ = new views::ImageView;
    AddChildView(image_view_);

    UpdateNetworkStateHandlerIcon();
  }

  ~NetworkTrayView() override {
    ui::network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
  }

  const char* GetClassName() const override { return "NetworkTrayView"; }

  void UpdateNetworkStateHandlerIcon() {
    NetworkStateHandler* handler =
        NetworkHandler::Get()->network_state_handler();
    gfx::ImageSkia image;
    base::string16 name;
    bool animating = false;
    ui::network_icon::GetDefaultNetworkImageAndLabel(
        ui::network_icon::ICON_TYPE_TRAY, &image, &name, &animating);
    bool show_in_tray = !image.isNull();
    UpdateIcon(show_in_tray, image);
    if (animating)
      ui::network_icon::NetworkIconAnimation::GetInstance()->AddObserver(this);
    else
      ui::network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(
          this);
    // Update accessibility.
    const NetworkState* connected_network =
        handler->ConnectedNetworkByType(NetworkTypePattern::NonVirtual());
    if (connected_network) {
      UpdateConnectionStatus(base::UTF8ToUTF16(connected_network->name()),
                             true);
    } else {
      UpdateConnectionStatus(base::string16(), false);
    }
  }

  void UpdateAlignment(wm::ShelfAlignment alignment) {
    SetLayoutManager(new views::BoxLayout(IsHorizontalAlignment(alignment)
                                              ? views::BoxLayout::kHorizontal
                                              : views::BoxLayout::kVertical,
                                          0, 0, 0));
    Layout();
  }

  // views::View override.
  void GetAccessibleState(ui::AXViewState* state) override {
    state->name = connection_status_string_;
    state->role = ui::AX_ROLE_BUTTON;
  }

  // ui::network_icon::AnimationObserver
  void NetworkIconChanged() override { UpdateNetworkStateHandlerIcon(); }

 private:
  // Updates connection status and notifies accessibility event when necessary.
  void UpdateConnectionStatus(const base::string16& network_name,
                              bool connected) {
    base::string16 new_connection_status_string;
    if (connected) {
      new_connection_status_string = l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CONNECTED, network_name);
    }
    if (new_connection_status_string != connection_status_string_) {
      connection_status_string_ = new_connection_status_string;
      if (!connection_status_string_.empty())
        NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, true);
    }
  }

  void UpdateIcon(bool tray_icon_visible, const gfx::ImageSkia& image) {
    image_view_->SetImage(image);
    SetVisible(tray_icon_visible);
    SchedulePaint();
  }

  views::ImageView* image_view_;
  base::string16 connection_status_string_;

  DISALLOW_COPY_AND_ASSIGN(NetworkTrayView);
};

class NetworkDefaultView : public TrayItemMore,
                           public ui::network_icon::AnimationObserver {
 public:
  NetworkDefaultView(TrayNetwork* network_tray, bool show_more)
      : TrayItemMore(network_tray, show_more) {
    Update();
  }

  ~NetworkDefaultView() override {
    ui::network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
  }

  void Update() {
    gfx::ImageSkia image;
    base::string16 label;
    bool animating = false;
    ui::network_icon::GetDefaultNetworkImageAndLabel(
        ui::network_icon::ICON_TYPE_DEFAULT_VIEW, &image, &label, &animating);
    if (animating)
      ui::network_icon::NetworkIconAnimation::GetInstance()->AddObserver(this);
    else
      ui::network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(
          this);
    SetImage(&image);
    SetLabel(label);
    SetAccessibleName(label);
  }

  // ui::network_icon::AnimationObserver
  void NetworkIconChanged() override { Update(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkDefaultView);
};

class NetworkWifiDetailedView : public NetworkDetailedView {
 public:
  explicit NetworkWifiDetailedView(SystemTrayItem* owner)
      : NetworkDetailedView(owner) {
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
                                          kTrayPopupPaddingHorizontal, 10,
                                          kTrayPopupPaddingBetweenItems));
    image_view_ = new views::ImageView;
    AddChildView(image_view_);

    label_view_ = new views::Label();
    label_view_->SetMultiLine(true);
    label_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    AddChildView(label_view_);

    Update();
  }

  ~NetworkWifiDetailedView() override {}

  // Overridden from NetworkDetailedView:

  void Init() override {}

  NetworkDetailedView::DetailedViewType GetViewType() const override {
    return NetworkDetailedView::WIFI_VIEW;
  }

  void Layout() override {
    // Center both views vertically.
    views::View::Layout();
    image_view_->SetY((height() - image_view_->GetPreferredSize().height()) /
                      2);
    label_view_->SetY((height() - label_view_->GetPreferredSize().height()) /
                      2);
  }

  void Update() override {
    bool wifi_enabled =
        NetworkHandler::Get()->network_state_handler()->IsTechnologyEnabled(
            NetworkTypePattern::WiFi());
    const int image_id = wifi_enabled ? IDR_AURA_UBER_TRAY_WIFI_ENABLED
                                      : IDR_AURA_UBER_TRAY_WIFI_DISABLED;
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    image_view_->SetImage(bundle.GetImageNamed(image_id).ToImageSkia());

    const int string_id = wifi_enabled
                              ? IDS_ASH_STATUS_TRAY_NETWORK_WIFI_ENABLED
                              : IDS_ASH_STATUS_TRAY_NETWORK_WIFI_DISABLED;
    label_view_->SetText(bundle.GetLocalizedString(string_id));
    label_view_->SizeToFit(
        kTrayPopupMinWidth - kTrayPopupPaddingHorizontal * 2 -
        kTrayPopupPaddingBetweenItems - kTrayPopupDetailsIconWidth);
  }

 private:
  views::ImageView* image_view_;
  views::Label* label_view_;

  DISALLOW_COPY_AND_ASSIGN(NetworkWifiDetailedView);
};

}  // namespace tray

TrayNetwork::TrayNetwork(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      tray_(NULL),
      default_(NULL),
      detailed_(NULL),
      request_wifi_view_(false) {
  network_state_observer_.reset(new TrayNetworkStateObserver(this));
  SystemTrayNotifier* notifier = Shell::GetInstance()->system_tray_notifier();
  notifier->AddNetworkObserver(this);
  notifier->AddNetworkPortalDetectorObserver(this);
}

TrayNetwork::~TrayNetwork() {
  SystemTrayNotifier* notifier = Shell::GetInstance()->system_tray_notifier();
  notifier->RemoveNetworkObserver(this);
  notifier->RemoveNetworkPortalDetectorObserver(this);
}

views::View* TrayNetwork::CreateTrayView(user::LoginStatus status) {
  CHECK(tray_ == NULL);
  if (!chromeos::NetworkHandler::IsInitialized())
    return NULL;
  tray_ = new tray::NetworkTrayView(this);
  return tray_;
}

views::View* TrayNetwork::CreateDefaultView(user::LoginStatus status) {
  CHECK(default_ == NULL);
  if (!chromeos::NetworkHandler::IsInitialized())
    return NULL;
  CHECK(tray_ != NULL);
  default_ =
      new tray::NetworkDefaultView(this, status != user::LOGGED_IN_LOCKED);
  return default_;
}

views::View* TrayNetwork::CreateDetailedView(user::LoginStatus status) {
  CHECK(detailed_ == NULL);
  Shell::GetInstance()->metrics()->RecordUserMetricsAction(
      ash::UMA_STATUS_AREA_DETAILED_NETWORK_VIEW);
  if (!chromeos::NetworkHandler::IsInitialized())
    return NULL;
  if (request_wifi_view_) {
    detailed_ = new tray::NetworkWifiDetailedView(this);
    request_wifi_view_ = false;
  } else {
    detailed_ = new tray::NetworkStateListDetailedView(
        this, tray::NetworkStateListDetailedView::LIST_TYPE_NETWORK, status);
    detailed_->Init();
  }
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

void TrayNetwork::UpdateAfterShelfAlignmentChange(
    wm::ShelfAlignment alignment) {
  if (tray_) {
    SetTrayImageItemBorder(tray_, alignment);
    tray_->UpdateAlignment(alignment);
  }
}

void TrayNetwork::RequestToggleWifi() {
  // This will always be triggered by a user action (e.g. keyboard shortcut)
  if (!detailed_ ||
      detailed_->GetViewType() == tray::NetworkDetailedView::WIFI_VIEW) {
    request_wifi_view_ = true;
    PopupDetailedView(kTrayPopupAutoCloseDelayForTextInSeconds, false);
  }
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  bool enabled = handler->IsTechnologyEnabled(NetworkTypePattern::WiFi());
  Shell::GetInstance()->metrics()->RecordUserMetricsAction(
      enabled ? ash::UMA_STATUS_AREA_DISABLE_WIFI
              : ash::UMA_STATUS_AREA_ENABLE_WIFI);
  handler->SetTechnologyEnabled(NetworkTypePattern::WiFi(), !enabled,
                                chromeos::network_handler::ErrorCallback());
}

void TrayNetwork::OnCaptivePortalDetected(
    const std::string& /* service_path */) {
  NetworkStateChanged();
}

void TrayNetwork::NetworkStateChanged() {
  if (tray_)
    tray_->UpdateNetworkStateHandlerIcon();
  if (default_)
    default_->Update();
  if (detailed_)
    detailed_->Update();
}

}  // namespace ash
