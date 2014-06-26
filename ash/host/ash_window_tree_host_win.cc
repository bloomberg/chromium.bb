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
#include "base/command_line.h"
#include "base/win/windows_version.h"
#include "ui/aura/window_tree_host_win.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/transform.h"

namespace ash {
namespace {

class AshWindowTreeHostWin : public AshWindowTreeHost,
                             public aura::WindowTreeHostWin {
 public:
  explicit AshWindowTreeHostWin(const gfx::Rect& initial_bounds)
      : aura::WindowTreeHostWin(initial_bounds),
        fullscreen_(false),
        saved_window_style_(0),
        saved_window_ex_style_(0),
        transformer_helper_(this) {}
  virtual ~AshWindowTreeHostWin() {}

 private:
  // AshWindowTreeHost:
  virtual void ToggleFullScreen() OVERRIDE {
    gfx::Rect target_rect;
    if (!fullscreen_) {
      fullscreen_ = true;
      saved_window_style_ = GetWindowLong(hwnd(), GWL_STYLE);
      saved_window_ex_style_ = GetWindowLong(hwnd(), GWL_EXSTYLE);
      GetWindowRect(hwnd(), &saved_window_rect_);
      SetWindowLong(hwnd(),
                    GWL_STYLE,
                    saved_window_style_ & ~(WS_CAPTION | WS_THICKFRAME));
      SetWindowLong(
          hwnd(),
          GWL_EXSTYLE,
          saved_window_ex_style_ & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE |
                                     WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

      MONITORINFO mi;
      mi.cbSize = sizeof(mi);
      GetMonitorInfo(MonitorFromWindow(hwnd(), MONITOR_DEFAULTTONEAREST), &mi);
      target_rect = gfx::Rect(mi.rcMonitor);
    } else {
      fullscreen_ = false;
      SetWindowLong(hwnd(), GWL_STYLE, saved_window_style_);
      SetWindowLong(hwnd(), GWL_EXSTYLE, saved_window_ex_style_);
      target_rect = gfx::Rect(saved_window_rect_);
    }
    SetWindowPos(hwnd(),
                 NULL,
                 target_rect.x(),
                 target_rect.y(),
                 target_rect.width(),
                 target_rect.height(),
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
  }
  virtual bool ConfineCursorToRootWindow() OVERRIDE { return false; }
  virtual void UnConfineCursor() OVERRIDE { NOTIMPLEMENTED(); }
  virtual void SetRootWindowTransformer(
      scoped_ptr<RootWindowTransformer> transformer) {
    transformer_helper_.SetRootWindowTransformer(transformer.Pass());
  }
  virtual gfx::Insets GetHostInsets() const OVERRIDE {
    return transformer_helper_.GetHostInsets();
  }
  virtual aura::WindowTreeHost* AsWindowTreeHost() OVERRIDE { return this; }

  // WindowTreeHostWin:
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE {
    if (fullscreen_) {
      saved_window_rect_.right = saved_window_rect_.left + bounds.width();
      saved_window_rect_.bottom = saved_window_rect_.top + bounds.height();
      return;
    }
    WindowTreeHostWin::SetBounds(bounds);
  }
  virtual void SetRootTransform(const gfx::Transform& transform) OVERRIDE {
    transformer_helper_.SetTransform(transform);
  }
  gfx::Transform GetRootTransform() const {
    return transformer_helper_.GetTransform();
  }
  virtual gfx::Transform GetInverseRootTransform() const OVERRIDE {
    return transformer_helper_.GetInverseTransform();
  }
  virtual void UpdateRootWindowSize(const gfx::Size& host_size) OVERRIDE {
    transformer_helper_.UpdateWindowSize(host_size);
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
      !CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kForceAshToDesktop))
    return new AshRemoteWindowTreeHostWin(init_params.remote_hwnd);

  return new AshWindowTreeHostWin(init_params.initial_bounds);
}

}  // namespace ash
