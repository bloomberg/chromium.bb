// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_aura_ash.h"

#include "apps/ui/views/app_window_frame_view.h"
#include "ash/ash_constants.h"
#include "ash/frame/custom_frame_view_ash.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/aura/wm_window_aura.h"
#include "ash/wm/immersive_fullscreen_controller.h"
#include "ash/wm/panels/panel_frame_view.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_aura.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_state_observer.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_context_menu.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

using extensions::AppWindow;

namespace {

// This class handles a user's fullscreen request (Shift+F4/F4).
class NativeAppWindowStateDelegate : public ash::wm::WindowStateDelegate,
                                     public ash::wm::WindowStateObserver,
                                     public aura::WindowObserver {
 public:
  NativeAppWindowStateDelegate(AppWindow* app_window,
                               extensions::NativeAppWindow* native_app_window)
      : app_window_(app_window),
        window_state_(
            ash::wm::GetWindowState(native_app_window->GetNativeWindow())) {
    // Add a window state observer to exit fullscreen properly in case
    // fullscreen is exited without going through AppWindow::Restore(). This
    // is the case when exiting immersive fullscreen via the "Restore" window
    // control.
    // TODO(pkotwicz): This is a hack. Remove ASAP. http://crbug.com/319048
    window_state_->AddObserver(this);
    ash::wm::WmWindowAura::GetAuraWindow(window_state_->window())
        ->AddObserver(this);
  }
  ~NativeAppWindowStateDelegate() override {
    if (window_state_) {
      window_state_->RemoveObserver(this);
      ash::wm::WmWindowAura::GetAuraWindow(window_state_->window())
          ->RemoveObserver(this);
    }
  }

 private:
  // Overridden from ash::wm::WindowStateDelegate.
  bool ToggleFullscreen(ash::wm::WindowState* window_state) override {
    // Windows which cannot be maximized should not be fullscreened.
    DCHECK(window_state->IsFullscreen() || window_state->CanMaximize());
    if (window_state->IsFullscreen())
      app_window_->Restore();
    else if (window_state->CanMaximize())
      app_window_->OSFullscreen();
    return true;
  }

  // Overridden from ash::wm::WindowStateDelegate.
  bool RestoreAlwaysOnTop(ash::wm::WindowState* window_state) override {
    app_window_->RestoreAlwaysOnTop();
    return true;
  }

  // Overridden from ash::wm::WindowStateObserver:
  void OnPostWindowStateTypeChange(ash::wm::WindowState* window_state,
                                   ash::wm::WindowStateType old_type) override {
    // Since the window state might get set by a window manager, it is possible
    // to come here before the application set its |BaseWindow|.
    if (!window_state->IsFullscreen() && !window_state->IsMinimized() &&
        app_window_->GetBaseWindow() &&
        app_window_->GetBaseWindow()->IsFullscreenOrPending()) {
      app_window_->Restore();
      // Usually OnNativeWindowChanged() is called when the window bounds are
      // changed as a result of a state type change. Because the change in state
      // type has already occurred, we need to call OnNativeWindowChanged()
      // explicitly.
      app_window_->OnNativeWindowChanged();
    }
  }

  // Overridden from aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    window_state_->RemoveObserver(this);
    ash::wm::WmWindowAura::GetAuraWindow(window_state_->window())
        ->RemoveObserver(this);
    window_state_ = NULL;
  }

  // Not owned.
  AppWindow* app_window_;
  ash::wm::WindowState* window_state_;

  DISALLOW_COPY_AND_ASSIGN(NativeAppWindowStateDelegate);
};

}  // namespace

ChromeNativeAppWindowViewsAuraAsh::ChromeNativeAppWindowViewsAuraAsh() {
}

ChromeNativeAppWindowViewsAuraAsh::~ChromeNativeAppWindowViewsAuraAsh() {
}

void ChromeNativeAppWindowViewsAuraAsh::InitializeWindow(
    AppWindow* app_window,
    const AppWindow::CreateParams& create_params) {
  ChromeNativeAppWindowViewsAura::InitializeWindow(app_window, create_params);
  // Restore docked state on ash desktop.
  if (create_params.state == ui::SHOW_STATE_DOCKED) {
    widget()->GetNativeWindow()->SetProperty(aura::client::kShowStateKey,
                                             create_params.state);
  }
}

void ChromeNativeAppWindowViewsAuraAsh::OnBeforeWidgetInit(
    const AppWindow::CreateParams& create_params,
    views::Widget::InitParams* init_params,
    views::Widget* widget) {
  ChromeNativeAppWindowViewsAura::OnBeforeWidgetInit(create_params, init_params,
                                                     widget);
  if (create_params.is_ime_window) {
    // Puts ime windows into ime window container.
    init_params->parent =
        ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                                 ash::kShellWindowId_ImeWindowParentContainer);
  }
}

void ChromeNativeAppWindowViewsAuraAsh::OnBeforePanelWidgetInit(
    bool use_default_bounds,
    views::Widget::InitParams* init_params,
    views::Widget* widget) {
  ChromeNativeAppWindowViewsAura::OnBeforePanelWidgetInit(use_default_bounds,
                                                          init_params,
                                                          widget);

  if (ash::Shell::HasInstance() && use_default_bounds) {
    // Open a new panel on the target root.
    init_params->bounds = ash::ScreenUtil::ConvertRectToScreen(
        ash::Shell::GetTargetRootWindow(), gfx::Rect(GetPreferredSize()));
  }
}

views::NonClientFrameView*
ChromeNativeAppWindowViewsAuraAsh::CreateNonStandardAppFrame() {
  apps::AppWindowFrameView* frame = new apps::AppWindowFrameView(widget(), this,
      HasFrameColor(), ActiveFrameColor(), InactiveFrameColor());
  frame->Init();

// For Aura windows on the Ash desktop the sizes are different and the user
// can resize the window from slightly outside the bounds as well.
  frame->SetResizeSizes(ash::kResizeInsideBoundsSize,
                        ash::kResizeOutsideBoundsSize,
                        ash::kResizeAreaCornerSize);
  return frame;
}

gfx::Rect ChromeNativeAppWindowViewsAuraAsh::GetRestoredBounds() const {
  gfx::Rect* bounds =
      widget()->GetNativeWindow()->GetProperty(ash::kRestoreBoundsOverrideKey);
  if (bounds && !bounds->IsEmpty())
    return *bounds;

  return ChromeNativeAppWindowViewsAura::GetRestoredBounds();
}

ui::WindowShowState
ChromeNativeAppWindowViewsAuraAsh::GetRestoredState() const {
  // Use kRestoreShowStateKey in case a window is minimized/hidden.
  ui::WindowShowState restore_state = widget()->GetNativeWindow()->GetProperty(
      aura::client::kRestoreShowStateKey);

  if (widget()->GetNativeWindow()->GetProperty(
          ash::kRestoreBoundsOverrideKey)) {
    // If an override is given, we use that restore state (after filtering).
    restore_state = widget()->GetNativeWindow()->GetProperty(
        ash::kRestoreShowStateOverrideKey);
  } else {
    // Otherwise first normal states are checked.
    if (IsMaximized())
      return ui::SHOW_STATE_MAXIMIZED;
    if (IsFullscreen()) {
      if (immersive_fullscreen_controller_.get() &&
          immersive_fullscreen_controller_->IsEnabled()) {
        // Restore windows which were previously in immersive fullscreen to
        // maximized. Restoring the window to a different fullscreen type
        // makes for a bad experience.
        return ui::SHOW_STATE_MAXIMIZED;
      }
      return ui::SHOW_STATE_FULLSCREEN;
    }
    if (widget()->GetNativeWindow()->GetProperty(
            aura::client::kShowStateKey) == ui::SHOW_STATE_DOCKED ||
        widget()->GetNativeWindow()->GetProperty(
            aura::client::kRestoreShowStateKey) == ui::SHOW_STATE_DOCKED) {
      return ui::SHOW_STATE_DOCKED;
    }
  }

  return GetRestorableState(restore_state);
}

bool ChromeNativeAppWindowViewsAuraAsh::IsAlwaysOnTop() const {
  if (app_window()->window_type_is_panel()) {
    return
        ash::wm::GetWindowState(widget()->GetNativeWindow())->panel_attached();
  }
  return widget()->IsAlwaysOnTop();
}

void ChromeNativeAppWindowViewsAuraAsh::ShowContextMenuForView(
    views::View* source,
    const gfx::Point& p,
    ui::MenuSourceType source_type) {
  std::unique_ptr<ui::MenuModel> model =
      CreateMultiUserContextMenu(app_window()->GetNativeWindow());
  if (!model.get())
    return;

  // Only show context menu if point is in caption.
  gfx::Point point_in_view_coords(p);
  views::View::ConvertPointFromScreen(widget()->non_client_view(),
                                      &point_in_view_coords);
  int hit_test =
      widget()->non_client_view()->NonClientHitTest(point_in_view_coords);
  if (hit_test == HTCAPTION) {
    menu_runner_.reset(new views::MenuRunner(
        model.get(),
        views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU));
    if (menu_runner_->RunMenuAt(source->GetWidget(), NULL,
                                gfx::Rect(p, gfx::Size(0, 0)),
                                views::MENU_ANCHOR_TOPLEFT, source_type) ==
        views::MenuRunner::MENU_DELETED) {
      return;
    }
  }
}

views::NonClientFrameView*
ChromeNativeAppWindowViewsAuraAsh::CreateNonClientFrameView(
    views::Widget* widget) {
  // Set the delegate now because CustomFrameViewAsh sets the
  // WindowStateDelegate if one is not already set.
  ash::wm::GetWindowState(GetNativeWindow())
      ->SetDelegate(std::unique_ptr<ash::wm::WindowStateDelegate>(
          new NativeAppWindowStateDelegate(app_window(), this)));

  if (IsFrameless())
    return CreateNonStandardAppFrame();

  if (app_window()->window_type_is_panel()) {
    ash::PanelFrameView* frame_view =
        new ash::PanelFrameView(widget, ash::PanelFrameView::FRAME_ASH);
    frame_view->set_context_menu_controller(this);
    if (HasFrameColor())
      frame_view->SetFrameColors(ActiveFrameColor(), InactiveFrameColor());
    return frame_view;
  }

  ash::CustomFrameViewAsh* custom_frame_view =
      new ash::CustomFrameViewAsh(widget);
  // Non-frameless app windows can be put into immersive fullscreen.
  immersive_fullscreen_controller_.reset(
      new ash::ImmersiveFullscreenController());
  custom_frame_view->InitImmersiveFullscreenControllerForView(
      immersive_fullscreen_controller_.get());
  custom_frame_view->GetHeaderView()->set_context_menu_controller(this);

  if (HasFrameColor()) {
    custom_frame_view->SetFrameColors(ActiveFrameColor(),
                                      InactiveFrameColor());
  }

  return custom_frame_view;
}

void ChromeNativeAppWindowViewsAuraAsh::SetFullscreen(int fullscreen_types) {
  ChromeNativeAppWindowViewsAura::SetFullscreen(fullscreen_types);

  if (immersive_fullscreen_controller_.get()) {
    // |immersive_fullscreen_controller_| should only be set if immersive
    // fullscreen is the fullscreen type used by the OS.
    immersive_fullscreen_controller_->SetEnabled(
        ash::ImmersiveFullscreenController::WINDOW_TYPE_PACKAGED_APP,
        (fullscreen_types & AppWindow::FULLSCREEN_TYPE_OS) != 0);
    // Autohide the shelf instead of hiding the shelf completely when only in
    // OS fullscreen.
    ash::wm::WindowState* window_state =
        ash::wm::GetWindowState(widget()->GetNativeWindow());
    window_state->set_hide_shelf_when_fullscreen(fullscreen_types !=
                                                 AppWindow::FULLSCREEN_TYPE_OS);
    DCHECK(ash::Shell::HasInstance());
    ash::Shell::GetInstance()->UpdateShelfVisibility();
  }
}
