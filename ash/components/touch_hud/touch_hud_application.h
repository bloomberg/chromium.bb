// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_TOUCH_HUD_TOUCH_HUD_APPLICATION_H_
#define ASH_COMPONENTS_TOUCH_HUD_TOUCH_HUD_APPLICATION_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/macros.h"
#include "mash/public/mojom/launchable.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "ui/display/display_observer.h"
#include "ui/views/pointer_watcher.h"

namespace views {
class AuraInit;
}  // namespace views

namespace touch_hud {

class TouchHudRenderer;

// Application that paints touch tap points as circles. Creates a fullscreen
// transparent widget on each display to draw the taps.
class TouchHudApplication : public service_manager::Service,
                            public mash::mojom::Launchable,
                            public views::PointerWatcher,
                            public display::DisplayObserver {
 public:
  TouchHudApplication();
  ~TouchHudApplication() override;

  void set_running_standalone(bool value) { running_standalone_ = value; }

 private:
  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  // mojom::Launchable:
  void Launch(uint32_t what, mash::mojom::LaunchMode how) override;

  // views::PointerWatcher:
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              const gfx::Point& location_in_screen,
                              gfx::NativeView target) override;

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;

  // Creates the touch HUD widget for a display.
  void CreateWidgetForDisplay(int64_t display_id);

  void Create(mash::mojom::LaunchableRequest request);

  service_manager::BinderRegistry registry_;
  mojo::Binding<mash::mojom::Launchable> binding_;

  // Maps display::Display::id() to the renderer for that display.
  std::map<int64_t, std::unique_ptr<TouchHudRenderer>> display_id_to_renderer_;

  std::unique_ptr<views::AuraInit> aura_init_;

  bool running_standalone_ = false;

  DISALLOW_COPY_AND_ASSIGN(TouchHudApplication);
};

}  // namespace touch_hud

#endif  // ASH_COMPONENTS_TOUCH_HUD_TOUCH_HUD_APPLICATION_H_
