// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/root_window_finder.h"
#include "ash/common/wm_window.h"
#include "ash/shell.h"
#include "ash/wm/window_properties.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/ui_controls_factory_aura.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/class_property.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/test/ui_controls_aura.h"
#include "ui/display/screen.h"

DECLARE_UI_CLASS_PROPERTY_TYPE(ui_controls::UIControlsAura*)

namespace ash {
namespace test {
namespace {

using ui_controls::UIControlsAura;
using ui_controls::MouseButton;

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(UIControlsAura, kUIControlsKey, NULL);

// Returns the UIControls object for RootWindow.
// kUIControlsKey is owned property and UIControls object
// will be deleted when the root window is deleted.
UIControlsAura* GetUIControlsForRootWindow(aura::Window* root_window) {
  UIControlsAura* native_ui_control = root_window->GetProperty(kUIControlsKey);
  if (!native_ui_control) {
    native_ui_control =
        aura::test::CreateUIControlsAura(root_window->GetHost());
    // Pass the ownership to the |root_window|.
    root_window->SetProperty(kUIControlsKey, native_ui_control);
  }
  return native_ui_control;
}

// Returns the UIControls object for the RootWindow at |point_in_screen|.
UIControlsAura* GetUIControlsAt(const gfx::Point& point_in_screen) {
  // TODO(mazda): Support the case passive grab is taken.
  return GetUIControlsForRootWindow(
      WmWindow::GetAuraWindow(ash::wm::GetRootWindowAt(point_in_screen)));
}

}  // namespace

class UIControlsAsh : public UIControlsAura {
 public:
  UIControlsAsh() {}
  ~UIControlsAsh() override {}

  // UIControslAura overrides:
  bool SendKeyPress(gfx::NativeWindow window,
                    ui::KeyboardCode key,
                    bool control,
                    bool shift,
                    bool alt,
                    bool command) override {
    return SendKeyPressNotifyWhenDone(window, key, control, shift, alt, command,
                                      base::Closure());
  }

  bool SendKeyPressNotifyWhenDone(gfx::NativeWindow window,
                                  ui::KeyboardCode key,
                                  bool control,
                                  bool shift,
                                  bool alt,
                                  bool command,
                                  const base::Closure& closure) override {
    aura::Window* root =
        window ? window->GetRootWindow() : ash::Shell::GetTargetRootWindow();
    UIControlsAura* ui_controls = GetUIControlsForRootWindow(root);
    return ui_controls &&
           ui_controls->SendKeyPressNotifyWhenDone(window, key, control, shift,
                                                   alt, command, closure);
  }

  bool SendMouseMove(long x, long y) override {
    gfx::Point p(x, y);
    UIControlsAura* ui_controls = GetUIControlsAt(p);
    return ui_controls && ui_controls->SendMouseMove(p.x(), p.y());
  }

  bool SendMouseMoveNotifyWhenDone(long x,
                                   long y,
                                   const base::Closure& closure) override {
    gfx::Point p(x, y);
    UIControlsAura* ui_controls = GetUIControlsAt(p);
    return ui_controls &&
           ui_controls->SendMouseMoveNotifyWhenDone(p.x(), p.y(), closure);
  }

  bool SendMouseEvents(MouseButton type, int state) override {
    gfx::Point p(display::Screen::GetScreen()->GetCursorScreenPoint());
    UIControlsAura* ui_controls = GetUIControlsAt(p);
    return ui_controls && ui_controls->SendMouseEvents(type, state);
  }

  bool SendMouseEventsNotifyWhenDone(MouseButton type,
                                     int state,
                                     const base::Closure& closure) override {
    gfx::Point p(aura::Env::GetInstance()->last_mouse_location());
    UIControlsAura* ui_controls = GetUIControlsAt(p);
    return ui_controls &&
           ui_controls->SendMouseEventsNotifyWhenDone(type, state, closure);
  }

  bool SendMouseClick(MouseButton type) override {
    gfx::Point p(display::Screen::GetScreen()->GetCursorScreenPoint());
    UIControlsAura* ui_controls = GetUIControlsAt(p);
    return ui_controls && ui_controls->SendMouseClick(type);
  }

  void RunClosureAfterAllPendingUIEvents(
      const base::Closure& closure) override {
    UIControlsAura* ui_controls =
        GetUIControlsForRootWindow(ash::Shell::GetTargetRootWindow());
    if (ui_controls)
      ui_controls->RunClosureAfterAllPendingUIEvents(closure);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UIControlsAsh);
};

ui_controls::UIControlsAura* CreateAshUIControls() {
  return new ash::test::UIControlsAsh();
}

}  // namespace test
}  // namespace ash
