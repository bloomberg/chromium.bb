// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/system/status_icon_container_view.h"

#include "athena/resources/athena_resources.h"
#include "athena/system/network_selector.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/update_engine_client.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/network/network_type_pattern.h"
#include "extensions/shell/common/version.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace athena {
namespace {

views::Label* CreateLabel(SystemUI::ColorScheme color_scheme,
                          const std::string& text) {
  views::Label* label = new views::Label(base::UTF8ToUTF16(text));
  label->SetEnabledColor((color_scheme == SystemUI::COLOR_SCHEME_LIGHT)
                             ? SK_ColorWHITE
                             : SK_ColorDKGRAY);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetSubpixelRenderingEnabled(false);
  label->SetFontList(gfx::FontList().DeriveWithStyle(gfx::Font::BOLD));
  return label;
}

}  // namespace

class StatusIconContainerView::PowerStatus
    : public chromeos::PowerManagerClient::Observer {
 public:
  PowerStatus(SystemUI::ColorScheme color_scheme,
              views::ImageView* icon)
      : color_scheme_(color_scheme),
        icon_(icon) {
    chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
        this);
    chromeos::DBusThreadManager::Get()
        ->GetPowerManagerClient()
        ->RequestStatusUpdate();
  }

  virtual ~PowerStatus() {
    chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(
        this);
  }

 private:
  const gfx::ImageSkia GetPowerIcon(
      const power_manager::PowerSupplyProperties& proto) const {
    // Width and height of battery images.
    const int kBatteryImageHeight = 25;
    const int kBatteryImageWidth = 25;

    // Number of different power states.
    const int kNumPowerImages = 15;

    gfx::Image all = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        (color_scheme_ == SystemUI::COLOR_SCHEME_LIGHT)
            ? IDR_AURA_UBER_TRAY_POWER_SMALL
            : IDR_AURA_UBER_TRAY_POWER_SMALL_DARK);
    int horiz_offset = IsCharging(proto) ? 1 : 0;
    int vert_offset = -1;
    if (proto.battery_percent() >= 100) {
      vert_offset = kNumPowerImages - 1;
    } else {
      vert_offset = static_cast<int>((kNumPowerImages - 1) *
                                     proto.battery_percent() / 100);
      vert_offset = std::max(std::min(vert_offset, kNumPowerImages - 2), 0);
    }
    gfx::Rect region(horiz_offset * kBatteryImageWidth,
                     vert_offset * kBatteryImageHeight,
                     kBatteryImageWidth,
                     kBatteryImageHeight);
    return gfx::ImageSkiaOperations::ExtractSubset(*all.ToImageSkia(), region);
  }

  bool IsCharging(const power_manager::PowerSupplyProperties& proto) const {
    return proto.external_power() !=
           power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED;
  }

  // chromeos::PowerManagerClient::Observer:
  virtual void PowerChanged(
      const power_manager::PowerSupplyProperties& proto) OVERRIDE {
    icon_->SetImage(GetPowerIcon(proto));
  }

  SystemUI::ColorScheme color_scheme_;
  views::ImageView* icon_;

  DISALLOW_COPY_AND_ASSIGN(PowerStatus);
};

class StatusIconContainerView::NetworkStatus
    : public chromeos::NetworkStateHandlerObserver {
 public:
  explicit NetworkStatus(views::Label* label) : label_(label) {
    chromeos::NetworkStateHandler* handler =
        chromeos::NetworkHandler::Get()->network_state_handler();
    handler->AddObserver(this, FROM_HERE);
  }

  virtual ~NetworkStatus() {
    chromeos::NetworkStateHandler* handler =
        chromeos::NetworkHandler::Get()->network_state_handler();
    handler->RemoveObserver(this, FROM_HERE);
  }

 private:
  void Update() {
    std::string status = "<unknown>";
    chromeos::NetworkStateHandler* handler =
        chromeos::NetworkHandler::Get()->network_state_handler();
    const chromeos::NetworkState* network = handler->DefaultNetwork();
    if (!network) {
      network = handler->ConnectedNetworkByType(
          chromeos::NetworkTypePattern::NonVirtual());
    }
    if (network) {
      status = base::StringPrintf(
          "%s (%s)", network->ip_address().c_str(), network->name().c_str());
    }
    label_->SetText(base::UTF8ToUTF16(status));
  }

  // chromeos::NetworkStateHandlerObserver:
  virtual void DefaultNetworkChanged(
      const chromeos::NetworkState* network) OVERRIDE {
    Update();
  }

  virtual void NetworkConnectionStateChanged(
      const chromeos::NetworkState* network) OVERRIDE {
    Update();
  }

  virtual void NetworkPropertiesUpdated(
      const chromeos::NetworkState* network) OVERRIDE {
    Update();
  }

  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(NetworkStatus);
};

void StartUpdateCallback(
    chromeos::UpdateEngineClient::UpdateCheckResult result) {
  VLOG(1) << "Callback from RequestUpdateCheck, result " << result;
}

class StatusIconContainerView::UpdateStatus
    : public chromeos::UpdateEngineClient::Observer {
 public:
  UpdateStatus(SystemUI::ColorScheme color_scheme, views::ImageView* icon)
      : color_scheme_(color_scheme),
        icon_(icon) {
    chromeos::DBusThreadManager::Get()->GetUpdateEngineClient()->AddObserver(
        this);
    chromeos::DBusThreadManager::Get()->GetUpdateEngineClient()->
        RequestUpdateCheck(base::Bind(StartUpdateCallback));
  }

  virtual ~UpdateStatus() {
    chromeos::DBusThreadManager::Get()->GetUpdateEngineClient()->RemoveObserver(
        this);
  }

  // chromeos::UpdateEngineClient::Observer:
  virtual void UpdateStatusChanged(
      const chromeos::UpdateEngineClient::Status& status) OVERRIDE {
    if (status.status !=
        chromeos::UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT) {
      return;
    }
    int image_id = (color_scheme_ == SystemUI::COLOR_SCHEME_LIGHT)
                       ? IDR_AURA_UBER_TRAY_UPDATE
                       : IDR_AURA_UBER_TRAY_UPDATE_DARK;
    icon_->SetImage(
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(image_id));
  }

 private:
  SystemUI::ColorScheme color_scheme_;
  views::ImageView* icon_;

  DISALLOW_COPY_AND_ASSIGN(UpdateStatus);
};

StatusIconContainerView::StatusIconContainerView(
    SystemUI::ColorScheme color_scheme,
    aura::Window* system_modal_container)
    : system_modal_container_(system_modal_container) {
  const int kHorizontalSpacing = 10;
  const int kVerticalSpacing = 3;
  const int kBetweenChildSpacing = 10;
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
                                        kHorizontalSpacing,
                                        kVerticalSpacing,
                                        kBetweenChildSpacing));

  std::string version_text =
      base::StringPrintf("%s (Build %s)", PRODUCT_VERSION, LAST_CHANGE);
  AddChildView(CreateLabel(color_scheme, version_text));

  AddChildView(CreateLabel(color_scheme, "Network:"));
  views::Label* network_label = CreateLabel(color_scheme, std::string());
  AddChildView(network_label);
  network_status_.reset(new NetworkStatus(network_label));

  views::ImageView* battery_view = new views::ImageView();
  AddChildView(battery_view);
  power_status_.reset(new PowerStatus(color_scheme, battery_view));

  views::ImageView* update_view = new views::ImageView();
  AddChildView(update_view);
  update_status_.reset(new UpdateStatus(color_scheme, update_view));
}

StatusIconContainerView::~StatusIconContainerView() {
}

bool StatusIconContainerView::OnMousePressed(const ui::MouseEvent& event) {
  CreateNetworkSelector(system_modal_container_);
  return true;
}

void StatusIconContainerView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    CreateNetworkSelector(system_modal_container_);
    event->SetHandled();
  }
}

void StatusIconContainerView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

}  // namespace athena
