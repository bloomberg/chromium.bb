// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_ash.h"

#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_shutdown.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/views/view.h"

using aura::Window;

namespace {

// BrowserWindowStateDelegate class handles a user's fullscreen
// request (Shift+F4/F4).
class BrowserWindowStateDelegate : public ash::wm::WindowStateDelegate {
 public:
  explicit BrowserWindowStateDelegate(Browser* browser)
      : browser_(browser) {
    DCHECK(browser_);
  }
  virtual ~BrowserWindowStateDelegate(){}

  // Overridden from ash::wm::WindowStateDelegate.
  virtual bool ToggleFullscreen(ash::wm::WindowState* window_state) OVERRIDE {
    DCHECK(window_state->IsFullscreen() || window_state->CanMaximize());
    // Windows which cannot be maximized should not be fullscreened.
    if (!window_state->IsFullscreen() && !window_state->CanMaximize())
      return true;
    chrome::ToggleFullscreenMode(browser_);
    return true;
  }
 private:
  Browser* browser_;  // not owned.

  DISALLOW_COPY_AND_ASSIGN(BrowserWindowStateDelegate);
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameAsh, public:

// static
const char BrowserFrameAsh::kWindowName[] = "BrowserFrameAsh";

BrowserFrameAsh::BrowserFrameAsh(BrowserFrame* browser_frame,
                                 BrowserView* browser_view)
    : views::NativeWidgetAura(browser_frame),
      browser_view_(browser_view) {
  GetNativeWindow()->SetName(kWindowName);
  Browser* browser = browser_view->browser();
  ash::wm::WindowState* window_state =
      ash::wm::GetWindowState(GetNativeWindow());
  window_state->SetDelegate(
      scoped_ptr<ash::wm::WindowStateDelegate>(
          new BrowserWindowStateDelegate(browser)).Pass());

  // Turn on auto window management if we don't need an explicit bounds.
  // This way the requested bounds are honored.
  if (!browser->bounds_overridden() && !browser->is_session_restore())
    SetWindowAutoManaged();
#if defined(OS_CHROMEOS)
  // For legacy reasons v1 apps (like Secure Shell) are allowed to consume keys
  // like brightness, volume, etc. Otherwise these keys are handled by the
  // Ash window manager.
  window_state->set_can_consume_system_keys(browser->is_app());
#endif  // defined(OS_CHROMEOS)
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameAsh, views::NativeWidgetAura overrides:

void BrowserFrameAsh::OnWindowDestroying(aura::Window* window) {
  // Destroy any remaining WebContents early on. Doing so may result in
  // calling back to one of the Views/LayoutManagers or supporting classes of
  // BrowserView. By destroying here we ensure all said classes are valid.
  DestroyBrowserWebContents(browser_view_->browser());
  NativeWidgetAura::OnWindowDestroying(window);
}

void BrowserFrameAsh::OnWindowTargetVisibilityChanged(bool visible) {
  if (visible) {
    // Once the window has been shown we know the requested bounds
    // (if provided) have been honored and we can switch on window management.
    SetWindowAutoManaged();
  }
  views::NativeWidgetAura::OnWindowTargetVisibilityChanged(visible);
}

bool BrowserFrameAsh::ShouldSaveWindowPlacement() const {
  return NULL == GetWidget()->GetNativeWindow()->GetProperty(
                     ash::kRestoreBoundsOverrideKey);
}

void BrowserFrameAsh::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  gfx::Rect* override_bounds = GetWidget()->GetNativeWindow()->GetProperty(
                                   ash::kRestoreBoundsOverrideKey);
  if (override_bounds && !override_bounds->IsEmpty()) {
    *bounds = *override_bounds;
    *show_state = GetWidget()->GetNativeWindow()->GetProperty(
                      ash::kRestoreShowStateOverrideKey);
  } else {
    *bounds = GetWidget()->GetRestoredBounds();
    *show_state = GetWidget()->GetNativeWindow()->GetProperty(
                      aura::client::kShowStateKey);
  }

  if (*show_state != ui::SHOW_STATE_MAXIMIZED &&
      *show_state != ui::SHOW_STATE_MINIMIZED) {
    *show_state = ui::SHOW_STATE_NORMAL;
  }
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

BrowserFrameAsh::~BrowserFrameAsh() {
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameAsh, private:

void BrowserFrameAsh::SetWindowAutoManaged() {
  if (!browser_view_->browser()->is_type_popup() ||
      browser_view_->browser()->is_app()) {
    ash::wm::GetWindowState(GetNativeWindow())->
        set_window_position_managed(true);
  }
}
