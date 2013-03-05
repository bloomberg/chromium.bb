// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/shell_factory.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/window_properties.h"
#include "chrome/test/base/ui_controls.h"
#include "chrome/test/base/ui_controls_aura.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window_property.h"
#include "ui/gfx/screen.h"

DECLARE_WINDOW_PROPERTY_TYPE(ui_controls::UIControlsAura*)

namespace ui_controls {
namespace {

DEFINE_OWNED_WINDOW_PROPERTY_KEY(UIControlsAura, kUIControlsKey, NULL);

// Returns the UIControls object for RootWindow.
// kUIControlsKey is owned property and UIControls object
// will be deleted when the root window is deleted.
UIControlsAura* GetUIControlsForRootWindow(aura::RootWindow* root_window) {
  UIControlsAura* native_ui_control =
      root_window->GetProperty(kUIControlsKey);
  if (!native_ui_control) {
    native_ui_control = CreateUIControlsAura(root_window);
    // Pass the ownership to the |root_window|.
    root_window->SetProperty(kUIControlsKey, native_ui_control);
  }
  return native_ui_control;
}

// Returns the UIControls object for the RootWindow at the |point_in_screen|
// in virtual screen coordinates, and updates the |point| relative to the
// UIControlsAura's root window.  NULL if there is no RootWindow under
// the |point_in_screen|.
UIControlsAura* GetUIControlsAt(gfx::Point* point_in_screen) {
  // TODO(mazda): Support the case passive grab is taken.
  aura::RootWindow* root = ash::wm::GetRootWindowAt(*point_in_screen);

  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(root);
  if (screen_position_client)
    screen_position_client->ConvertPointFromScreen(root, point_in_screen);

  return GetUIControlsForRootWindow(root);
}

}  // namespace

class UIControlsAsh : public UIControlsAura {
 public:
  UIControlsAsh() {
  }
  virtual ~UIControlsAsh() {
  }

  // UIControslAura overrides:
  virtual bool SendKeyPress(gfx::NativeWindow window,
                            ui::KeyboardCode key,
                            bool control,
                            bool shift,
                            bool alt,
                            bool command) OVERRIDE {
    return SendKeyPressNotifyWhenDone(
        window, key, control, shift, alt, command, base::Closure());
  }

  virtual bool SendKeyPressNotifyWhenDone(
      gfx::NativeWindow window,
      ui::KeyboardCode key,
      bool control,
      bool shift,
      bool alt,
      bool command,
      const base::Closure& closure) OVERRIDE {
    aura::RootWindow* root =
        window ? window->GetRootWindow() : ash::Shell::GetActiveRootWindow();
    UIControlsAura* ui_controls = GetUIControlsForRootWindow(root);
    return ui_controls && ui_controls->SendKeyPressNotifyWhenDone(
        window, key, control, shift, alt, command, closure);
  }

  virtual bool SendMouseMove(long x, long y) OVERRIDE {
    gfx::Point p(x, y);
    UIControlsAura* ui_controls = GetUIControlsAt(&p);
    return ui_controls && ui_controls->SendMouseMove(p.x(), p.y());
  }

  virtual bool SendMouseMoveNotifyWhenDone(
      long x,
      long y,
      const base::Closure& closure) OVERRIDE {
    gfx::Point p(x, y);
    UIControlsAura* ui_controls = GetUIControlsAt(&p);
    return ui_controls &&
        ui_controls->SendMouseMoveNotifyWhenDone(p.x(), p.y(), closure);
  }

  virtual bool SendMouseEvents(MouseButton type, int state) OVERRIDE {
    gfx::Point p(ash::Shell::GetScreen()->GetCursorScreenPoint());
    UIControlsAura* ui_controls = GetUIControlsAt(&p);
    return ui_controls && ui_controls->SendMouseEvents(type, state);
  }

  virtual bool SendMouseEventsNotifyWhenDone(
      MouseButton type, int state, const base::Closure& closure) OVERRIDE {
    gfx::Point p(aura::Env::GetInstance()->last_mouse_location());
    UIControlsAura* ui_controls = GetUIControlsAt(&p);
    return ui_controls && ui_controls->SendMouseEventsNotifyWhenDone(
        type, state, closure);
  }

  virtual bool SendMouseClick(MouseButton type) OVERRIDE {
    gfx::Point p(ash::Shell::GetScreen()->GetCursorScreenPoint());
    UIControlsAura* ui_controls = GetUIControlsAt(&p);
    return ui_controls && ui_controls->SendMouseClick(type);
  }

  virtual void RunClosureAfterAllPendingUIEvents(
      const base::Closure& closure) OVERRIDE {
    UIControlsAura* ui_controls = GetUIControlsForRootWindow(
        ash::Shell::GetActiveRootWindow());
    if (ui_controls)
      ui_controls->RunClosureAfterAllPendingUIEvents(closure);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UIControlsAsh);
};

UIControlsAura* CreateAshUIControls() {
  return new UIControlsAsh();
}

}  // namespace ui_controls
