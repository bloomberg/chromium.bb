// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_aura.h"

#include "apps/ui/views/app_window_frame_view.h"
#include "ash/ash_constants.h"
#include "ash/frame/custom_frame_view_ash.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/immersive_fullscreen_controller.h"
#include "ash/wm/panels/panel_frame_view.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_state_observer.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_context_menu.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/views/apps/shaped_app_window_targeter.h"
#include "chrome/browser/web_applications/web_app.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/easy_resize_window_targeter.h"

#if defined(OS_CHROMEOS)
#include "ash/shell_window_ids.h"
#endif

#if defined(OS_LINUX)
#include "chrome/browser/shell_integration_linux.h"
#endif

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
    window_state_->window()->AddObserver(this);
  }
  ~NativeAppWindowStateDelegate() override {
    if (window_state_) {
      window_state_->RemoveObserver(this);
      window_state_->window()->RemoveObserver(this);
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
    window_state_->window()->RemoveObserver(this);
    window_state_ = NULL;
  }

  // Not owned.
  AppWindow* app_window_;
  ash::wm::WindowState* window_state_;

  DISALLOW_COPY_AND_ASSIGN(NativeAppWindowStateDelegate);
};

}  // namespace

ChromeNativeAppWindowViewsAura::ChromeNativeAppWindowViewsAura() {
}

ChromeNativeAppWindowViewsAura::~ChromeNativeAppWindowViewsAura() {
}

void ChromeNativeAppWindowViewsAura::OnBeforeWidgetInit(
    const AppWindow::CreateParams& create_params,
    views::Widget::InitParams* init_params,
    views::Widget* widget) {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  std::string app_name = web_app::GenerateApplicationNameFromExtensionId(
      app_window()->extension_id());
  // Set up a custom WM_CLASS for app windows. This allows task switchers in
  // X11 environments to distinguish them from main browser windows.
  init_params->wm_class_name = web_app::GetWMClassFromAppName(app_name);
  init_params->wm_class_class = shell_integration_linux::GetProgramClassName();
  const char kX11WindowRoleApp[] = "app";
  init_params->wm_role_name = std::string(kX11WindowRoleApp);
#endif

  ChromeNativeAppWindowViews::OnBeforeWidgetInit(create_params, init_params,
                                                 widget);

#if defined(OS_CHROMEOS)
  if (create_params.is_ime_window) {
    // Puts ime windows into ime window container.
    init_params->parent =
        ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                                 ash::kShellWindowId_ImeWindowParentContainer);
  }
#endif
}

void ChromeNativeAppWindowViewsAura::OnBeforePanelWidgetInit(
    views::Widget::InitParams* init_params,
    views::Widget* widget) {
  ChromeNativeAppWindowViews::OnBeforePanelWidgetInit(init_params, widget);

  if (ash::Shell::HasInstance()) {
    // Open a new panel on the target root.
    init_params->bounds = ash::ScreenUtil::ConvertRectToScreen(
        ash::Shell::GetTargetRootWindow(), gfx::Rect(GetPreferredSize()));
  }
}

apps::AppWindowFrameView*
ChromeNativeAppWindowViewsAura::CreateNonStandardAppFrame() {
  apps::AppWindowFrameView* frame =
      ChromeNativeAppWindowViews::CreateNonStandardAppFrame();

  // For Aura windows on the Ash desktop the sizes are different and the user
  // can resize the window from slightly outside the bounds as well.
  if (chrome::IsNativeWindowInAsh(widget()->GetNativeWindow())) {
    frame->SetResizeSizes(ash::kResizeInsideBoundsSize,
                          ash::kResizeOutsideBoundsSize,
                          ash::kResizeAreaCornerSize);
  }

#if !defined(OS_CHROMEOS)
  // For non-Ash windows, install an easy resize window targeter, which ensures
  // that the root window (not the app) receives mouse events on the edges.
  if (chrome::GetHostDesktopTypeForNativeWindow(widget()->GetNativeWindow()) !=
      chrome::HOST_DESKTOP_TYPE_ASH) {
    aura::Window* window = widget()->GetNativeWindow();
    int resize_inside = frame->resize_inside_bounds_size();
    gfx::Insets inset(resize_inside, resize_inside, resize_inside,
                      resize_inside);
    // Add the EasyResizeWindowTargeter on the window, not its root window. The
    // root window does not have a delegate, which is needed to handle the event
    // in Linux.
    window->SetEventTargeter(scoped_ptr<ui::EventTargeter>(
        new wm::EasyResizeWindowTargeter(window, inset, inset)));
  }
#endif

  return frame;
}

gfx::Rect ChromeNativeAppWindowViewsAura::GetRestoredBounds() const {
  gfx::Rect* bounds =
      widget()->GetNativeWindow()->GetProperty(ash::kRestoreBoundsOverrideKey);
  if (bounds && !bounds->IsEmpty())
    return *bounds;

  return ChromeNativeAppWindowViews::GetRestoredBounds();
}

ui::WindowShowState ChromeNativeAppWindowViewsAura::GetRestoredState() const {
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
  }
  // Whitelist states to return so that invalid and transient states
  // are not saved and used to restore windows when they are recreated.
  switch (restore_state) {
    case ui::SHOW_STATE_NORMAL:
    case ui::SHOW_STATE_MAXIMIZED:
    case ui::SHOW_STATE_FULLSCREEN:
      return restore_state;

    case ui::SHOW_STATE_DEFAULT:
    case ui::SHOW_STATE_MINIMIZED:
    case ui::SHOW_STATE_INACTIVE:
    case ui::SHOW_STATE_END:
      return ui::SHOW_STATE_NORMAL;
  }

  return ui::SHOW_STATE_NORMAL;
}

bool ChromeNativeAppWindowViewsAura::IsAlwaysOnTop() const {
  return app_window()->window_type_is_panel()
             ? ash::wm::GetWindowState(widget()->GetNativeWindow())
                   ->panel_attached()
             : widget()->IsAlwaysOnTop();
}

void ChromeNativeAppWindowViewsAura::ShowContextMenuForView(
    views::View* source,
    const gfx::Point& p,
    ui::MenuSourceType source_type) {
#if defined(OS_CHROMEOS)
  scoped_ptr<ui::MenuModel> model =
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
#endif
}

views::NonClientFrameView*
ChromeNativeAppWindowViewsAura::CreateNonClientFrameView(
    views::Widget* widget) {
  if (chrome::IsNativeViewInAsh(widget->GetNativeView())) {
    // Set the delegate now because CustomFrameViewAsh sets the
    // WindowStateDelegate if one is not already set.
    ash::wm::GetWindowState(GetNativeWindow())
        ->SetDelegate(
            scoped_ptr<ash::wm::WindowStateDelegate>(
                new NativeAppWindowStateDelegate(app_window(), this)).Pass());

    if (IsFrameless())
      return CreateNonStandardAppFrame();

    if (app_window()->window_type_is_panel()) {
      views::NonClientFrameView* frame_view =
          new ash::PanelFrameView(widget, ash::PanelFrameView::FRAME_ASH);
      frame_view->set_context_menu_controller(this);
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

  return ChromeNativeAppWindowViews::CreateNonClientFrameView(widget);
}

void ChromeNativeAppWindowViewsAura::SetFullscreen(int fullscreen_types) {
  ChromeNativeAppWindowViews::SetFullscreen(fullscreen_types);

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

void ChromeNativeAppWindowViewsAura::UpdateShape(scoped_ptr<SkRegion> region) {
  bool had_shape = !!shape();

  ChromeNativeAppWindowViews::UpdateShape(region.Pass());

  aura::Window* native_window = widget()->GetNativeWindow();
  if (shape() && !had_shape) {
    native_window->SetEventTargeter(scoped_ptr<ui::EventTargeter>(
        new ShapedAppWindowTargeter(native_window, this)));
  } else if (!shape() && had_shape) {
    native_window->SetEventTargeter(scoped_ptr<ui::EventTargeter>());
  }
}
