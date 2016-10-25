// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/autoclick/mus/autoclick_application.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/mus/pointer_watcher_event_router.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/pointer_watcher.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace autoclick {

// The default wait time between last mouse movement and sending
// the autoclick.
const int kDefaultAutoclickDelayMs = 1000;

// AutoclickUI handles events to the autoclick app.
class AutoclickUI : public views::WidgetDelegateView,
                    public views::PointerWatcher {
 public:
  AutoclickUI(views::WindowManagerConnection* window_manager_connection,
              AutoclickControllerCommon* autoclick_controller_common)
      : window_manager_connection_(window_manager_connection),
        autoclick_controller_common_(autoclick_controller_common) {
    window_manager_connection_->pointer_watcher_event_router()
        ->AddPointerWatcher(this, true /* want_moves */);
  }
  ~AutoclickUI() override {
    window_manager_connection_->pointer_watcher_event_router()
        ->RemovePointerWatcher(this);
  }

 private:
  // Overridden from views::WidgetDelegate:
  base::string16 GetWindowTitle() const override {
    // TODO(beng): use resources.
    return base::ASCIIToUTF16("Autoclick");
  }

  // Overridden from views::PointerWatcher:
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              const gfx::Point& location_in_screen,
                              views::Widget* target) override {
    if (event.IsTouchPointerEvent()) {
      autoclick_controller_common_->CancelAutoclick();
    } else if (event.IsMousePointerEvent()) {
      if (event.type() == ui::ET_POINTER_WHEEL_CHANGED) {
        autoclick_controller_common_->HandleMouseEvent(
            ui::MouseWheelEvent(event));
      } else {
        autoclick_controller_common_->HandleMouseEvent(ui::MouseEvent(event));
      }
    }
  }

  views::WindowManagerConnection* window_manager_connection_;
  AutoclickControllerCommon* autoclick_controller_common_;

  DISALLOW_COPY_AND_ASSIGN(AutoclickUI);
};

AutoclickApplication::AutoclickApplication()
    : launchable_binding_(this), autoclick_binding_(this) {}

AutoclickApplication::~AutoclickApplication() {}

void AutoclickApplication::OnStart(const service_manager::ServiceInfo& info) {
  aura_init_.reset(new views::AuraInit(connector(), "views_mus_resources.pak"));
  window_manager_connection_ =
      views::WindowManagerConnection::Create(connector(), info.identity);
  autoclick_controller_common_.reset(new AutoclickControllerCommon(
      base::TimeDelta::FromMilliseconds(kDefaultAutoclickDelayMs), this));
}

bool AutoclickApplication::OnConnect(
    const service_manager::ServiceInfo& remote_info,
    service_manager::InterfaceRegistry* registry) {
  registry->AddInterface<mash::mojom::Launchable>(this);
  registry->AddInterface<mojom::AutoclickController>(this);
  return true;
}

void AutoclickApplication::Launch(uint32_t what, mash::mojom::LaunchMode how) {
  if (!widget_) {
    widget_.reset(new views::Widget);
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
    params.accept_events = false;
    params.delegate = new AutoclickUI(window_manager_connection_.get(),
                                      autoclick_controller_common_.get());

    std::map<std::string, std::vector<uint8_t>> properties;
    properties[ui::mojom::WindowManager::kInitialContainerId_Property] =
        mojo::ConvertTo<std::vector<uint8_t>>(
            ash::kShellWindowId_OverlayContainer);
    properties[ui::mojom::WindowManager::kShowState_Property] =
        mojo::ConvertTo<std::vector<uint8_t>>(
            static_cast<int32_t>(ui::mojom::ShowState::FULLSCREEN));
    ui::Window* window =
        window_manager_connection_.get()->NewTopLevelWindow(properties);
    params.native_widget = new views::NativeWidgetMus(
        widget_.get(), window, ui::mojom::CompositorFrameSinkType::DEFAULT);
    widget_->Init(params);
  } else {
    widget_->Close();
    base::MessageLoop::current()->QuitWhenIdle();
  }
}

void AutoclickApplication::SetAutoclickDelay(uint32_t delay_in_milliseconds) {
  autoclick_controller_common_->SetAutoclickDelay(
      base::TimeDelta::FromMilliseconds(delay_in_milliseconds));
}

void AutoclickApplication::Create(
    const service_manager::Identity& remote_identity,
    mash::mojom::LaunchableRequest request) {
  launchable_binding_.Close();
  launchable_binding_.Bind(std::move(request));
}

void AutoclickApplication::Create(
    const service_manager::Identity& remote_identity,
    mojom::AutoclickControllerRequest request) {
  autoclick_binding_.Close();
  autoclick_binding_.Bind(std::move(request));
}

views::Widget* AutoclickApplication::CreateAutoclickRingWidget(
    const gfx::Point& event_location) {
  return widget_.get();
}

void AutoclickApplication::UpdateAutoclickRingWidget(
    views::Widget* widget,
    const gfx::Point& event_location) {
  // Not used in mus.
}

void AutoclickApplication::DoAutoclick(const gfx::Point& event_location,
                                       const int mouse_event_flags) {
  // TODO(riajiang): Currently not working. Need to know how to generate events
  // in mus world (crbug.com/628665).
}

void AutoclickApplication::OnAutoclickCanceled() {
  // Not used in mus.
}

}  // namespace autoclick
}  // namespace ash
