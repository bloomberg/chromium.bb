// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/main/debug/debug_window.h"

#include "athena/common/container_priorities.h"
#include "athena/main/debug/network_selector.h"
#include "athena/resources/athena_resources.h"
#include "athena/screen/public/screen_manager.h"
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
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

views::Label* CreateDebugLabel(const std::string& text) {
  views::Label* label = new views::Label(base::UTF8ToUTF16(text));
  label->SetEnabledColor(SK_ColorWHITE);
  label->SetBackgroundColor(SK_ColorTRANSPARENT);
  label->SetFontList(gfx::FontList().Derive(-2, gfx::Font::BOLD));
  return label;
}

class PowerStatus : public chromeos::PowerManagerClient::Observer {
 public:
  PowerStatus(views::ImageView* icon, const base::Closure& closure)
      : icon_(icon), closure_(closure) {
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
        IDR_AURA_UBER_TRAY_POWER_SMALL);
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
    if (!closure_.is_null())
      closure_.Run();
  }

  views::ImageView* icon_;
  base::Closure closure_;

  DISALLOW_COPY_AND_ASSIGN(PowerStatus);
};

class NetworkStatus : public chromeos::NetworkStateHandlerObserver {
 public:
  NetworkStatus(views::Label* label, const base::Closure& closure)
      : label_(label), closure_(closure) {
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
    if (!closure_.is_null())
      closure_.Run();
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
  base::Closure closure_;
};

void StartUpdateCallback(
    chromeos::UpdateEngineClient::UpdateCheckResult result) {
  VLOG(1) << "Callback from RequestUpdateCheck, result " << result;
}

class UpdateStatus : public chromeos::UpdateEngineClient::Observer {
 public:
  UpdateStatus(views::ImageView* icon, const base::Closure& closure)
      : icon_(icon), closure_(closure) {
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
    icon_->SetImage(ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_AURA_UBER_TRAY_UPDATE));
    if (!closure_.is_null())
      closure_.Run();
  }

 private:
  views::ImageView* icon_;
  base::Closure closure_;

  DISALLOW_COPY_AND_ASSIGN(UpdateStatus);
};

// Processes user input to show the detailed network-list.
class DetailViewHandler : public ui::EventHandler {
 public:
  explicit DetailViewHandler(aura::Window* container) : container_(container) {}
  virtual ~DetailViewHandler() {}

 private:
  // ui::EventHandler:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    if (event->type() == ui::ET_MOUSE_PRESSED) {
      debug::CreateNetworkSelector(container_);
      event->SetHandled();
    }
  }

  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    if (event->type() == ui::ET_GESTURE_TAP) {
      debug::CreateNetworkSelector(container_);
      event->SetHandled();
    }
  }

  aura::Window* container_;

  DISALLOW_COPY_AND_ASSIGN(DetailViewHandler);
};

class DebugWidget {
 public:
  DebugWidget() : container_(NULL), widget_(NULL) {
    CreateContainer();
    CreateWidget();

    CreateBatteryView();
    CreateUpdateView();
    CreateNetworkView();

    UpdateSize();
  }

  virtual ~DebugWidget() {}

 private:
  void CreateContainer() {
    athena::ScreenManager::ContainerParams params("DebugContainer",
                                                  athena::CP_DEBUG);
    params.can_activate_children = true;
    container_ = athena::ScreenManager::Get()->CreateContainer(params);
  }

  void CreateWidget() {
    views::Widget::InitParams params;
    params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
    params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
    params.accept_events = true;
    params.bounds = gfx::Rect(200, 0, 100, 105);
    params.parent = container_;
    widget_ = new views::Widget();
    widget_->Init(params);

    event_handler_.reset(new DetailViewHandler(container_));

    const int kHorizontalSpacing = 10;
    const int kBorderVerticalSpacing = 3;
    const int kBetweenChildSpacing = 10;
    const int kBackgroundColor = SkColorSetARGB(0x7f, 0, 0, 0);
    views::View* container = new views::View;
    container->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kHorizontal,
                             kHorizontalSpacing,
                             kBorderVerticalSpacing,
                             kBetweenChildSpacing));
    container->set_background(
        views::Background::CreateSolidBackground(kBackgroundColor));
    container->SetBorder(views::Border::CreateSolidBorder(1, kBackgroundColor));
    container->set_target_handler(event_handler_.get());
    widget_->SetContentsView(container);
    widget_->StackAtTop();
    widget_->Show();

    widget_->SetBounds(gfx::Rect(600, 0, 300, 25));
  }

  void CreateBatteryView() {
    views::View* container = widget_->GetContentsView();
    views::ImageView* icon = new views::ImageView();
    container->AddChildView(icon);
    container->Layout();

    power_status_.reset(new PowerStatus(
        icon, base::Bind(&DebugWidget::UpdateSize, base::Unretained(this))));
  }

  void CreateNetworkView() {
    views::View* container = widget_->GetContentsView();
    container->AddChildView(CreateDebugLabel("Network:"));

    views::Label* label = CreateDebugLabel(std::string());
    container->AddChildView(label);
    container->Layout();

    network_status_.reset(new NetworkStatus(
        label, base::Bind(&DebugWidget::UpdateSize, base::Unretained(this))));
  }

  void CreateUpdateView() {
    views::View* container = widget_->GetContentsView();
    views::ImageView* icon = new views::ImageView();
    container->AddChildView(icon);
    container->Layout();

    update_status_.reset(new UpdateStatus(
        icon, base::Bind(&DebugWidget::UpdateSize, base::Unretained(this))));
  }

  const gfx::Rect GetPositionForSize(const gfx::Size& size) {
    int right = container_->bounds().right();
    int x = right - size.width();
    return gfx::Rect(x, 0, size.width(), size.height());
  }

  void UpdateSize() {
    views::View* container = widget_->GetContentsView();
    container->Layout();
    gfx::Size size = container->GetPreferredSize();
    widget_->SetBounds(GetPositionForSize(size));
  }

  aura::Window* container_;
  views::Widget* widget_;
  scoped_ptr<PowerStatus> power_status_;
  scoped_ptr<NetworkStatus> network_status_;
  scoped_ptr<UpdateStatus> update_status_;
  scoped_ptr<ui::EventHandler> event_handler_;

  DISALLOW_COPY_AND_ASSIGN(DebugWidget);
};

}  // namespace

void CreateDebugWindow() {
  new DebugWidget();
}
