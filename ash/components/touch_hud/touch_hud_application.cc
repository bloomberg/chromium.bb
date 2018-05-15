// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/touch_hud/touch_hud_application.h"

#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/touch_hud/touch_hud_renderer.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/mus/pointer_watcher_event_router.h"
#include "ui/views/pointer_watcher.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace touch_hud {
namespace {

// TouchHudUI handles events on the widget of the touch-hud app. After
// receiving touch events from PointerWatcher, it calls ash::TouchHudRenderer to
// draw the touch points.
class TouchHudUI : public views::WidgetDelegateView,
                   public views::PointerWatcher {
 public:
  explicit TouchHudUI(views::Widget* widget)
      : touch_hud_renderer_(std::make_unique<ash::TouchHudRenderer>(widget)) {
    // Watches moves so the user can drag around a touch point.
    views::MusClient::Get()->pointer_watcher_event_router()->AddPointerWatcher(
        this, true /* want_moves */);
  }
  ~TouchHudUI() override {
    views::MusClient::Get()
        ->pointer_watcher_event_router()
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
                              gfx::NativeView target) override {
    if (!event.IsTouchPointerEvent())
      return;

    // Ignore taps on other displays.
    if (!GetWidget()->GetWindowBoundsInScreen().Contains(location_in_screen))
      return;

    touch_hud_renderer_->HandleTouchEvent(event);
  }

  std::unique_ptr<ash::TouchHudRenderer> touch_hud_renderer_;

  DISALLOW_COPY_AND_ASSIGN(TouchHudUI);
};

}  // namespace

TouchHudApplication::TouchHudApplication() : binding_(this) {
  registry_.AddInterface<mash::mojom::Launchable>(base::BindRepeating(
      &TouchHudApplication::Create, base::Unretained(this)));
}

TouchHudApplication::~TouchHudApplication() {
  display::Screen::GetScreen()->RemoveObserver(this);
}

void TouchHudApplication::OnStart() {
  const bool register_path_provider = running_standalone_;
  aura_init_ = views::AuraInit::Create(
      context()->connector(), context()->identity(), "views_mus_resources.pak",
      std::string(), nullptr, views::AuraInit::Mode::AURA_MUS2,
      register_path_provider);
  if (!aura_init_) {
    context()->QuitNow();
    return;
  }
  display::Screen::GetScreen()->AddObserver(this);
  Launch(mash::mojom::kWindow, mash::mojom::LaunchMode::DEFAULT);
}

void TouchHudApplication::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void TouchHudApplication::Launch(uint32_t what, mash::mojom::LaunchMode how) {
  for (const display::Display& display :
       display::Screen::GetScreen()->GetAllDisplays()) {
    CreateWidgetForDisplay(display.id());
  }
}

void TouchHudApplication::OnDisplayAdded(const display::Display& new_display) {
  CreateWidgetForDisplay(new_display.id());
}

void TouchHudApplication::OnDisplayRemoved(
    const display::Display& old_display) {
  // Deletes the widget.
  display_id_to_widget_.erase(old_display.id());
}

void TouchHudApplication::CreateWidgetForDisplay(int64_t display_id) {
  std::unique_ptr<views::Widget> widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.accept_events = false;
  params.delegate = new TouchHudUI(widget.get());
  params.mus_properties[ui::mojom::WindowManager::kContainerId_InitProperty] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<int32_t>(ash::kShellWindowId_OverlayContainer));
  params.mus_properties[ui::mojom::WindowManager::kDisplayId_InitProperty] =
      mojo::ConvertTo<std::vector<uint8_t>>(display_id);
  params.show_state = ui::SHOW_STATE_FULLSCREEN;
  params.name = "TouchHud";
  widget->Init(params);
  widget->Show();
  display_id_to_widget_[display_id] = std::move(widget);
}

void TouchHudApplication::Create(mash::mojom::LaunchableRequest request) {
  binding_.Close();
  binding_.Bind(std::move(request));
}

}  // namespace touch_hud
