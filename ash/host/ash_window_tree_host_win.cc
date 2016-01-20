// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/ash_window_tree_host.h"

#include "ash/ash_export.h"
#include "ash/ash_switches.h"
#include "ash/host/ash_remote_window_tree_host_win.h"
#include "ash/host/ash_window_tree_host_init_params.h"
#include "ash/host/root_window_transformer.h"
#include "ash/host/transformer_helper.h"
#include "ash/ime/input_method_event_handler.h"
#include "base/command_line.h"
#include "base/win/windows_version.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/events/event_processor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/transform.h"

namespace ash {
namespace {

class AshWindowTreeHostWin : public AshWindowTreeHost,
                             public aura::WindowTreeHostPlatform {
 public:
  explicit AshWindowTreeHostWin(const gfx::Rect& initial_bounds)
      : aura::WindowTreeHostPlatform(initial_bounds),
        fullscreen_(false),
        saved_window_style_(0),
        saved_window_ex_style_(0),
        transformer_helper_(this) {
    transformer_helper_.Init();
  }
  ~AshWindowTreeHostWin() override {}

 private:
  // AshWindowTreeHost:
  void ToggleFullScreen() override {
    gfx::Rect target_rect;
    gfx::AcceleratedWidget hwnd = GetAcceleratedWidget();
    if (!fullscreen_) {
      fullscreen_ = true;
      saved_window_style_ = GetWindowLong(hwnd, GWL_STYLE);
      saved_window_ex_style_ = GetWindowLong(hwnd, GWL_EXSTYLE);
      GetWindowRect(hwnd, &saved_window_rect_);
      SetWindowLong(hwnd, GWL_STYLE,
                    saved_window_style_ & ~(WS_CAPTION | WS_THICKFRAME));
      SetWindowLong(hwnd, GWL_EXSTYLE,
                    saved_window_ex_style_ &
                        ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE |
                          WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

      MONITORINFO mi;
      mi.cbSize = sizeof(mi);
      GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
      target_rect = gfx::Rect(mi.rcMonitor);
    } else {
      fullscreen_ = false;
      SetWindowLong(hwnd, GWL_STYLE, saved_window_style_);
      SetWindowLong(hwnd, GWL_EXSTYLE, saved_window_ex_style_);
      target_rect = gfx::Rect(saved_window_rect_);
    }
    SetWindowPos(hwnd, NULL, target_rect.x(), target_rect.y(),
                 target_rect.width(), target_rect.height(),
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
  }
  bool ConfineCursorToRootWindow() override { return false; }
  void UnConfineCursor() override { NOTIMPLEMENTED(); }
  void SetRootWindowTransformer(
      scoped_ptr<RootWindowTransformer> transformer) override {
    transformer_helper_.SetRootWindowTransformer(transformer.Pass());
  }
  gfx::Insets GetHostInsets() const override {
    return transformer_helper_.GetHostInsets();
  }
  aura::WindowTreeHost* AsWindowTreeHost() override { return this; }

  // WindowTreeHost:
  void SetBounds(const gfx::Rect& bounds) override {
    if (fullscreen_) {
      saved_window_rect_.right = saved_window_rect_.left + bounds.width();
      saved_window_rect_.bottom = saved_window_rect_.top + bounds.height();
      return;
    }
    aura::WindowTreeHostPlatform::SetBounds(bounds);
  }
  void SetRootTransform(const gfx::Transform& transform) override {
    transformer_helper_.SetTransform(transform);
  }
  gfx::Transform GetRootTransform() const override {
    return transformer_helper_.GetTransform();
  }
  gfx::Transform GetInverseRootTransform() const override {
    return transformer_helper_.GetInverseTransform();
  }
  void UpdateRootWindowSize(const gfx::Size& host_size) override {
    transformer_helper_.UpdateWindowSize(host_size);
  }

  // ui::internal::InputMethodDelegate:
  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* event) override {
    input_method_handler()->SetPostIME(true);
    ui::EventDispatchDetails details =
        event_processor()->OnEventFromSource(event);
    if (!details.dispatcher_destroyed)
      input_method_handler()->SetPostIME(false);
    return details;
  }

  bool fullscreen_;
  RECT saved_window_rect_;
  DWORD saved_window_style_;
  DWORD saved_window_ex_style_;

  TransformerHelper transformer_helper_;

  DISALLOW_COPY_AND_ASSIGN(AshWindowTreeHostWin);
};

}  // namespace

AshWindowTreeHost* AshWindowTreeHost::Create(
    const AshWindowTreeHostInitParams& init_params) {
  if (base::win::GetVersion() >= base::win::VERSION_WIN7 &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kForceAshToDesktop))
    return new AshRemoteWindowTreeHostWin(init_params.remote_hwnd);

  return new AshWindowTreeHostWin(init_params.initial_bounds);
}

}  // namespace ash
