// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_ash.h"

#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/views/view.h"

using aura::Window;

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameAsh::WindowPropertyWatcher

class BrowserFrameAsh::WindowPropertyWatcher : public aura::WindowObserver {
 public:
  explicit WindowPropertyWatcher(BrowserFrameAsh* browser_frame_ash,
                                 BrowserFrame* browser_frame)
      : browser_frame_ash_(browser_frame_ash),
        browser_frame_(browser_frame) {}

  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE {
    if (key != aura::client::kShowStateKey)
      return;

    ui::WindowShowState old_state = static_cast<ui::WindowShowState>(old);
    ui::WindowShowState new_state =
        window->GetProperty(aura::client::kShowStateKey);

    // Allow the frame to be replaced when entering or exiting the maximized
    // state.
    if (browser_frame_->non_client_view() &&
        browser_frame_ash_->browser_view()->browser()->is_app() &&
        (old_state == ui::SHOW_STATE_MAXIMIZED ||
         new_state == ui::SHOW_STATE_MAXIMIZED)) {
      // Defer frame layout when replacing the frame. Layout will occur when the
      // window's bounds are updated. The window maximize/restore animations
      // clone the window's layers and rely on the subsequent layout to set
      // the layer sizes.
      // If the window is minimized, the frame view needs to be updated via
      // an OnBoundsChanged event so that the frame will change its size
      // properly.
      browser_frame_->non_client_view()->UpdateFrame(
          old_state == ui::SHOW_STATE_MINIMIZED);
    }
  }

  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE {
    // Don't do anything if we don't have our non-client view yet.
    if (!browser_frame_->non_client_view())
      return;

    // If the window just moved to the top of the screen, or just moved away
    // from it, invoke Layout() so the header size can change.
    if ((old_bounds.y() == 0 && new_bounds.y() != 0) ||
        (old_bounds.y() != 0 && new_bounds.y() == 0))
      browser_frame_->non_client_view()->Layout();
  }

 private:
  BrowserFrameAsh* browser_frame_ash_;
  BrowserFrame* browser_frame_;

  DISALLOW_COPY_AND_ASSIGN(WindowPropertyWatcher);
};

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameAsh, public:

// static
const char BrowserFrameAsh::kWindowName[] = "BrowserFrameAsh";

BrowserFrameAsh::BrowserFrameAsh(BrowserFrame* browser_frame,
                                 BrowserView* browser_view)
    : views::NativeWidgetAura(browser_frame),
      browser_view_(browser_view),
      window_property_watcher_(new WindowPropertyWatcher(this, browser_frame)) {
  GetNativeWindow()->SetName(kWindowName);
  GetNativeWindow()->AddObserver(window_property_watcher_.get());
  if (browser_view->browser()->is_type_tabbed())
    ash::wm::SetAnimateToFullscreen(GetNativeWindow(), false);

  // Turn on auto window management if we don't need an explicit bounds.
  // This way the requested bounds are honored.
  if (!browser_view->browser()->bounds_overridden() &&
      !browser_view->browser()->is_session_restore())
    SetWindowAutoManaged();
#if defined(OS_CHROMEOS)
  // For legacy reasons v1 apps (like Secure Shell) are allowed to consume keys
  // like brightness, volume, etc. Otherwise these keys are handled by the
  // Ash window manager.
  if (browser_view->browser()->is_app()) {
    ash::wm::GetWindowState(GetNativeWindow())->
        set_can_consume_system_keys(true);
  }
#endif  // defined(OS_CHROMEOS)
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameAsh, views::NativeWidgetAura overrides:

void BrowserFrameAsh::OnWindowDestroying() {
  // Window is destroyed before our destructor is called, so clean up our
  // observer here.
  GetNativeWindow()->RemoveObserver(window_property_watcher_.get());
  views::NativeWidgetAura::OnWindowDestroying();
}

void BrowserFrameAsh::OnWindowTargetVisibilityChanged(bool visible) {
  if (visible) {
    // Once the window has been shown we know the requested bounds
    // (if provided) have been honored and we can switch on window management.
    SetWindowAutoManaged();
  }
  views::NativeWidgetAura::OnWindowTargetVisibilityChanged(visible);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameAsh, NativeBrowserFrame implementation:

views::NativeWidget* BrowserFrameAsh::AsNativeWidget() {
  return this;
}

const views::NativeWidget* BrowserFrameAsh::AsNativeWidget() const {
  return this;
}

bool BrowserFrameAsh::UsesNativeSystemMenu() const {
  return false;
}

int BrowserFrameAsh::GetMinimizeButtonOffset() const {
  return 0;
}

void BrowserFrameAsh::TabStripDisplayModeChanged() {
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameAsh, private:

BrowserFrameAsh::~BrowserFrameAsh() {
}

void BrowserFrameAsh::SetWindowAutoManaged() {
  if (browser_view_->browser()->type() != Browser::TYPE_POPUP ||
      browser_view_->browser()->is_app()) {
    ash::wm::GetWindowState(GetNativeWindow())->
        set_window_position_managed(true);
  }
}
