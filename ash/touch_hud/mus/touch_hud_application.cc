// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch_hud/mus/touch_hud_application.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/touch_hud/touch_hud_renderer.h"
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
namespace touch_hud {

// TouchHudUI handles events on the widget of the touch-hud app. After
// receiving touch events from ui::WindowManagerConnection, it calls
// ash::TouchHudRenderer to draw out touch points.
class TouchHudUI : public views::WidgetDelegateView,
                   public views::PointerWatcher {
 public:
  TouchHudUI(views::WindowManagerConnection* window_manager_connection,
             views::Widget* widget)
      : window_manager_connection_(window_manager_connection),
        touch_hud_renderer_(new TouchHudRenderer(widget)) {
    window_manager_connection_->pointer_watcher_event_router()
        ->AddPointerWatcher(this, true /* want_moves */);
  }
  ~TouchHudUI() override {
    window_manager_connection_->pointer_watcher_event_router()
        ->RemovePointerWatcher(this);
  }

 private:
  // Overridden from views::WidgetDelegate:
  base::string16 GetWindowTitle() const override {
    // TODO(beng): use resources.
    return base::ASCIIToUTF16("TouchHud");
  }

  // Overridden from views::PointerWatcher:
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              const gfx::Point& location_in_screen,
                              views::Widget* target) override {
    if (event.IsTouchPointerEvent())
      touch_hud_renderer_->HandleTouchEvent(event);
  }

  views::WindowManagerConnection* window_manager_connection_;
  TouchHudRenderer* touch_hud_renderer_;

  DISALLOW_COPY_AND_ASSIGN(TouchHudUI);
};

TouchHudApplication::TouchHudApplication() : binding_(this) {}
TouchHudApplication::~TouchHudApplication() {}

void TouchHudApplication::OnStart(const service_manager::ServiceInfo& info) {
  aura_init_.reset(new views::AuraInit(connector(), "views_mus_resources.pak"));
  window_manager_connection_ =
      views::WindowManagerConnection::Create(connector(), info.identity);
}

bool TouchHudApplication::OnConnect(
    const service_manager::ServiceInfo& remote_info,
    service_manager::InterfaceRegistry* registry) {
  registry->AddInterface<mash::mojom::Launchable>(this);
  return true;
}

void TouchHudApplication::Launch(uint32_t what, mash::mojom::LaunchMode how) {
  if (!widget_) {
    widget_ = new views::Widget;
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
    params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
    params.accept_events = false;
    params.delegate = new TouchHudUI(window_manager_connection_.get(), widget_);

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
        widget_, window, ui::mojom::CompositorFrameSinkType::DEFAULT);
    widget_->Init(params);
    widget_->Show();
  } else {
    widget_->Close();
    base::MessageLoop::current()->QuitWhenIdle();
  }
}

void TouchHudApplication::Create(
    const service_manager::Identity& remote_identity,
    mash::mojom::LaunchableRequest request) {
  binding_.Close();
  binding_.Bind(std::move(request));
}

}  // namespace touch_hud
}  // namespace ash
