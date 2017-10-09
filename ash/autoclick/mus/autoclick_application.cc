// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/autoclick/mus/autoclick_application.h"

#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/mus/pointer_watcher_event_router.h"
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
  explicit AutoclickUI(AutoclickControllerCommon* autoclick_controller_common)
      : autoclick_controller_common_(autoclick_controller_common) {
    views::MusClient::Get()->pointer_watcher_event_router()->AddPointerWatcher(
        this, true /* want_moves */);
  }
  ~AutoclickUI() override {
    views::MusClient::Get()
        ->pointer_watcher_event_router()
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
                              gfx::NativeView target) override {
    // AutoclickControllerCommon won't work correctly with a target.
    DCHECK(!event.target());
    if (event.IsTouchPointerEvent()) {
      autoclick_controller_common_->CancelAutoclick();
    } else if (event.IsMousePointerEvent()) {
      if (event.type() == ui::ET_POINTER_WHEEL_CHANGED) {
        autoclick_controller_common_->HandleMouseEvent(
            ui::MouseWheelEvent(event));
      } else {
        ui::MouseEvent mouse_event(event);
        // AutoclickControllerCommon wants screen coordinates when there isn't a
        // target.
        mouse_event.set_location(location_in_screen);
        autoclick_controller_common_->HandleMouseEvent(mouse_event);
      }
    }
  }

  AutoclickControllerCommon* autoclick_controller_common_;

  DISALLOW_COPY_AND_ASSIGN(AutoclickUI);
};

AutoclickApplication::AutoclickApplication()
    : launchable_binding_(this), autoclick_binding_(this) {
  registry_.AddInterface<mash::mojom::Launchable>(base::Bind(
      &AutoclickApplication::BindLaunchableRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::AutoclickController>(
      base::Bind(&AutoclickApplication::BindAutoclickControllerRequest,
                 base::Unretained(this)));
}

AutoclickApplication::~AutoclickApplication() {}

void AutoclickApplication::OnStart() {
  const bool register_path_provider = running_standalone_;
  aura_init_ = views::AuraInit::Create(
      context()->connector(), context()->identity(), "views_mus_resources.pak",
      std::string(), nullptr, views::AuraInit::Mode::AURA_MUS,
      register_path_provider);
  if (!aura_init_) {
    context()->QuitNow();
    return;
  }
  autoclick_controller_common_.reset(new AutoclickControllerCommon(
      base::TimeDelta::FromMilliseconds(kDefaultAutoclickDelayMs), this));
}

void AutoclickApplication::OnBindInterface(
    const service_manager::BindSourceInfo& remote_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
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
    params.delegate = new AutoclickUI(autoclick_controller_common_.get());

    params.mus_properties[ui::mojom::WindowManager::kContainerId_InitProperty] =
        mojo::ConvertTo<std::vector<uint8_t>>(
            static_cast<int32_t>(ash::kShellWindowId_OverlayContainer));
    params.show_state = ui::SHOW_STATE_FULLSCREEN;
    widget_->Init(params);
  } else {
    widget_->Close();
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }
}

void AutoclickApplication::SetAutoclickDelay(uint32_t delay_in_milliseconds) {
  autoclick_controller_common_->SetAutoclickDelay(
      base::TimeDelta::FromMilliseconds(delay_in_milliseconds));
}

void AutoclickApplication::BindLaunchableRequest(
    mash::mojom::LaunchableRequest request) {
  launchable_binding_.Close();
  launchable_binding_.Bind(std::move(request));
}

void AutoclickApplication::BindAutoclickControllerRequest(
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
