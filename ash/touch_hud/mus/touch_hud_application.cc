// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch_hud/mus/touch_hud_application.h"

#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/touch_hud/touch_hud_renderer.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/mus/pointer_watcher_event_router.h"
#include "ui/views/pointer_watcher.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace touch_hud {

// TouchHudUI handles events on the widget of the touch-hud app. After
// receiving touch events from PointerWatcher, it calls ash::TouchHudRenderer to
// draw the touch points.
class TouchHudUI : public views::WidgetDelegateView,
                   public views::PointerWatcher {
 public:
  explicit TouchHudUI(views::Widget* widget)
      : touch_hud_renderer_(new TouchHudRenderer(widget)) {
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
    if (event.IsTouchPointerEvent())
      touch_hud_renderer_->HandleTouchEvent(event);
  }

  TouchHudRenderer* touch_hud_renderer_;

  DISALLOW_COPY_AND_ASSIGN(TouchHudUI);
};

TouchHudApplication::TouchHudApplication() : binding_(this) {
  registry_.AddInterface<mash::mojom::Launchable>(
      base::Bind(&TouchHudApplication::Create, base::Unretained(this)));
}
TouchHudApplication::~TouchHudApplication() {}

void TouchHudApplication::OnStart() {
  const bool register_path_provider = running_standalone_;
  aura_init_ = views::AuraInit::Create(
      context()->connector(), context()->identity(), "views_mus_resources.pak",
      std::string(), nullptr, views::AuraInit::Mode::AURA_MUS,
      register_path_provider);
  if (!aura_init_)
    context()->QuitNow();
}

void TouchHudApplication::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void TouchHudApplication::Launch(uint32_t what, mash::mojom::LaunchMode how) {
  if (!widget_) {
    widget_ = new views::Widget;
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
    params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
    params.accept_events = false;
    params.delegate = new TouchHudUI(widget_);
    params.mus_properties[ui::mojom::WindowManager::kContainerId_InitProperty] =
        mojo::ConvertTo<std::vector<uint8_t>>(
            static_cast<int32_t>(ash::kShellWindowId_OverlayContainer));
    params.show_state = ui::SHOW_STATE_FULLSCREEN;
    widget_->Init(params);
    widget_->Show();
  } else {
    widget_->Close();
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }
}

void TouchHudApplication::Create(
    mash::mojom::LaunchableRequest request) {
  binding_.Close();
  binding_.Bind(std::move(request));
}

}  // namespace touch_hud
}  // namespace ash
