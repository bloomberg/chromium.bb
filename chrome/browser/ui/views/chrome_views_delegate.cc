// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_views_delegate.h"

#include <memory>

#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/keep_alive_types.h"
#include "chrome/browser/lifetime/scoped_keep_alive.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/browser/ui/views/harmony/harmony_layout_delegate.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/context_factory.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include <dwmapi.h>
#include <shellapi.h>
#include "base/profiler/scoped_tracker.h"
#include "base/task_runner_util.h"
#include "base/win/windows_version.h"
#include "chrome/browser/win/app_icon.h"
#include "ui/base/win/shell.h"
#endif

#if defined(USE_AURA)
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#endif

#if defined(USE_AURA) && !defined(OS_CHROMEOS)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/native_widget_aura.h"
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "ui/views/linux_ui/linux_ui.h"
#endif

#if defined(USE_ASH)
#include "ash/common/accelerators/accelerator_controller.h"  // nogncheck
#include "ash/common/wm/window_state.h"  // nogncheck
#include "ash/common/wm_shell.h"  // nogncheck
#include "ash/shell.h"  // nogncheck
#include "ash/wm/window_state_aura.h"  // nogncheck
#include "chrome/browser/ui/ash/ash_init.h"  // nogncheck
#endif

// Helpers --------------------------------------------------------------------

namespace {

Profile* GetProfileForWindow(const views::Widget* window) {
  if (!window)
    return NULL;
  return reinterpret_cast<Profile*>(
      window->GetNativeWindowProperty(Profile::kProfileKey));
}

// If the given window has a profile associated with it, use that profile's
// preference service. Otherwise, store and retrieve the data from Local State.
// This function may return NULL if the necessary pref service has not yet
// been initialized.
// TODO(mirandac): This function will also separate windows by profile in a
// multi-profile environment.
PrefService* GetPrefsForWindow(const views::Widget* window) {
  Profile* profile = GetProfileForWindow(window);
  if (!profile) {
    // Use local state for windows that have no explicit profile.
    return g_browser_process->local_state();
  }
  return profile->GetPrefs();
}

#if defined(OS_WIN)
bool MonitorHasTopmostAutohideTaskbarForEdge(UINT edge, HMONITOR monitor) {
  APPBARDATA taskbar_data = { sizeof(APPBARDATA), NULL, 0, edge };
  taskbar_data.hWnd = ::GetForegroundWindow();

  // TODO(robliao): Remove ScopedTracker below once crbug.com/462368 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "462368 MonitorHasTopmostAutohideTaskbarForEdge"));

  // MSDN documents an ABM_GETAUTOHIDEBAREX, which supposedly takes a monitor
  // rect and returns autohide bars on that monitor.  This sounds like a good
  // idea for multi-monitor systems.  Unfortunately, it appears to not work at
  // least some of the time (erroneously returning NULL) and there's almost no
  // online documentation or other sample code using it that suggests ways to
  // address this problem. We do the following:-
  // 1. Use the ABM_GETAUTOHIDEBAR message. If it works, i.e. returns a valid
  //    window we are done.
  // 2. If the ABM_GETAUTOHIDEBAR message does not work we query the auto hide
  //    state of the taskbar and then retrieve its position. That call returns
  //    the edge on which the taskbar is present. If it matches the edge we
  //    are looking for, we are done.
  // NOTE: This call spins a nested message loop.
  HWND taskbar = reinterpret_cast<HWND>(SHAppBarMessage(ABM_GETAUTOHIDEBAR,
                                                        &taskbar_data));
  if (!::IsWindow(taskbar)) {
    APPBARDATA taskbar_data = { sizeof(APPBARDATA), 0, 0, 0};
    unsigned int taskbar_state = SHAppBarMessage(ABM_GETSTATE,
                                                 &taskbar_data);
    if (!(taskbar_state & ABS_AUTOHIDE))
      return false;

    taskbar_data.hWnd = ::FindWindow(L"Shell_TrayWnd", NULL);
    if (!::IsWindow(taskbar_data.hWnd))
      return false;

    SHAppBarMessage(ABM_GETTASKBARPOS, &taskbar_data);
    if (taskbar_data.uEdge == edge)
      taskbar = taskbar_data.hWnd;
  }

  if (::IsWindow(taskbar) &&
      (GetWindowLong(taskbar, GWL_EXSTYLE) & WS_EX_TOPMOST)) {
    if (MonitorFromWindow(taskbar, MONITOR_DEFAULTTONEAREST) == monitor)
      return true;
    // In some cases like when the autohide taskbar is on the left of the
    // secondary monitor, the MonitorFromWindow call above fails to return the
    // correct monitor the taskbar is on. We fallback to MonitorFromPoint for
    // the cursor position in that case, which seems to work well.
    POINT cursor_pos = {0};
    GetCursorPos(&cursor_pos);
    if (MonitorFromPoint(cursor_pos, MONITOR_DEFAULTTONEAREST) == monitor)
      return true;
  }
  return false;
}

int GetAppbarAutohideEdgesOnWorkerThread(HMONITOR monitor) {
  DCHECK(monitor);

  int edges = 0;
  if (MonitorHasTopmostAutohideTaskbarForEdge(ABE_LEFT, monitor))
    edges |= views::ViewsDelegate::EDGE_LEFT;
  if (MonitorHasTopmostAutohideTaskbarForEdge(ABE_TOP, monitor))
    edges |= views::ViewsDelegate::EDGE_TOP;
  if (MonitorHasTopmostAutohideTaskbarForEdge(ABE_RIGHT, monitor))
    edges |= views::ViewsDelegate::EDGE_RIGHT;
  if (MonitorHasTopmostAutohideTaskbarForEdge(ABE_BOTTOM, monitor))
    edges |= views::ViewsDelegate::EDGE_BOTTOM;
  return edges;
}
#endif

#if defined(USE_ASH)
void ProcessAcceleratorNow(const ui::Accelerator& accelerator) {
  // TODO(afakhry): See if we need here to send the accelerator to the
  // FocusManager of the active window in a follow-up CL.
  ash::WmShell::Get()->accelerator_controller()->Process(accelerator);
}
#endif  // defined(USE_ASH)

}  // namespace


// ChromeViewsDelegate --------------------------------------------------------

#if defined(OS_WIN)
ChromeViewsDelegate::ChromeViewsDelegate()
    : in_autohide_edges_callback_(false),
      weak_factory_(this) {
#else
ChromeViewsDelegate::ChromeViewsDelegate() {
#endif
}

ChromeViewsDelegate::~ChromeViewsDelegate() {
  DCHECK_EQ(0u, ref_count_);
}

void ChromeViewsDelegate::SaveWindowPlacement(const views::Widget* window,
                                              const std::string& window_name,
                                              const gfx::Rect& bounds,
                                              ui::WindowShowState show_state) {
  PrefService* prefs = GetPrefsForWindow(window);
  if (!prefs)
    return;

  std::unique_ptr<DictionaryPrefUpdate> pref_update =
      chrome::GetWindowPlacementDictionaryReadWrite(window_name, prefs);
  base::DictionaryValue* window_preferences = pref_update->Get();
  window_preferences->SetInteger("left", bounds.x());
  window_preferences->SetInteger("top", bounds.y());
  window_preferences->SetInteger("right", bounds.right());
  window_preferences->SetInteger("bottom", bounds.bottom());
  window_preferences->SetBoolean("maximized",
                                 show_state == ui::SHOW_STATE_MAXIMIZED);
  window_preferences->SetBoolean("docked", show_state == ui::SHOW_STATE_DOCKED);
  gfx::Rect work_area(display::Screen::GetScreen()
                          ->GetDisplayNearestWindow(window->GetNativeView())
                          .work_area());
  window_preferences->SetInteger("work_area_left", work_area.x());
  window_preferences->SetInteger("work_area_top", work_area.y());
  window_preferences->SetInteger("work_area_right", work_area.right());
  window_preferences->SetInteger("work_area_bottom", work_area.bottom());
}

bool ChromeViewsDelegate::GetSavedWindowPlacement(
    const views::Widget* widget,
    const std::string& window_name,
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  PrefService* prefs = g_browser_process->local_state();
  if (!prefs)
    return false;

  DCHECK(prefs->FindPreference(window_name));
  const base::DictionaryValue* dictionary = prefs->GetDictionary(window_name);
  int left = 0;
  int top = 0;
  int right = 0;
  int bottom = 0;
  if (!dictionary || !dictionary->GetInteger("left", &left) ||
      !dictionary->GetInteger("top", &top) ||
      !dictionary->GetInteger("right", &right) ||
      !dictionary->GetInteger("bottom", &bottom))
    return false;

  bounds->SetRect(left, top, right - left, bottom - top);

  bool maximized = false;
  if (dictionary)
    dictionary->GetBoolean("maximized", &maximized);
  *show_state = maximized ? ui::SHOW_STATE_MAXIMIZED : ui::SHOW_STATE_NORMAL;

#if defined(USE_ASH)
  // On Ash environment, a window won't span across displays.  Adjust
  // the bounds to fit the work area.
  gfx::NativeView window = widget->GetNativeView();
  display::Display display =
      display::Screen::GetScreen()->GetDisplayMatching(*bounds);
  bounds->AdjustToFit(display.work_area());
  ash::wm::GetWindowState(window)->set_minimum_visibility(true);
#endif
  return true;
}

void ChromeViewsDelegate::NotifyAccessibilityEvent(
    views::View* view, ui::AXEvent event_type) {
#if defined(USE_AURA)
  AutomationManagerAura::GetInstance()->HandleEvent(
      GetProfileForWindow(view->GetWidget()), view, event_type);
#endif
}

views::ViewsDelegate::ProcessMenuAcceleratorResult
ChromeViewsDelegate::ProcessAcceleratorWhileMenuShowing(
    const ui::Accelerator& accelerator) {
#if defined(USE_ASH)
  DCHECK(base::MessageLoopForUI::IsCurrent());

  // Early return because mash chrome does not have access to ash::Shell
  if (chrome::IsRunningInMash())
    return views::ViewsDelegate::ProcessMenuAcceleratorResult::LEAVE_MENU_OPEN;

  ash::AcceleratorController* accelerator_controller =
      ash::WmShell::Get()->accelerator_controller();

  accelerator_controller->accelerator_history()->StoreCurrentAccelerator(
      accelerator);
  if (accelerator_controller->ShouldCloseMenuAndRepostAccelerator(
          accelerator)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(ProcessAcceleratorNow, accelerator));
    return views::ViewsDelegate::ProcessMenuAcceleratorResult::CLOSE_MENU;
  }

  ProcessAcceleratorNow(accelerator);
  return views::ViewsDelegate::ProcessMenuAcceleratorResult::LEAVE_MENU_OPEN;
#else
  return views::ViewsDelegate::ProcessMenuAcceleratorResult::LEAVE_MENU_OPEN;
#endif  // defined(USE_ASH)
}

#if defined(OS_WIN)
HICON ChromeViewsDelegate::GetDefaultWindowIcon() const {
  return GetAppIcon();
}

HICON ChromeViewsDelegate::GetSmallWindowIcon() const {
  return GetSmallAppIcon();
}

#elif defined(OS_LINUX) && !defined(OS_CHROMEOS)
gfx::ImageSkia* ChromeViewsDelegate::GetDefaultWindowIcon() const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetImageSkiaNamed(IDR_PRODUCT_LOGO_64);
}
#endif

#if defined(USE_ASH)
views::NonClientFrameView* ChromeViewsDelegate::CreateDefaultNonClientFrameView(
    views::Widget* widget) {
  return ash::Shell::GetInstance()->CreateDefaultNonClientFrameView(widget);
}
#endif

void ChromeViewsDelegate::AddRef() {
  if (ref_count_ == 0u) {
    keep_alive_.reset(
        new ScopedKeepAlive(KeepAliveOrigin::CHROME_VIEWS_DELEGATE,
                            KeepAliveRestartOption::DISABLED));
  }

  ++ref_count_;
}

void ChromeViewsDelegate::ReleaseRef() {
  DCHECK_NE(0u, ref_count_);

  if (--ref_count_ == 0u)
    keep_alive_.reset();
}

void ChromeViewsDelegate::OnBeforeWidgetInit(
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {
  // We need to determine opacity if it's not already specified.
  if (params->opacity == views::Widget::InitParams::INFER_OPACITY)
    params->opacity = GetOpacityForInitParams(*params);

  // If we already have a native_widget, we don't have to try to come
  // up with one.
  if (params->native_widget)
    return;

  if (!native_widget_factory().is_null()) {
    params->native_widget = native_widget_factory().Run(*params, delegate);
    if (params->native_widget)
      return;
  }

#if defined(USE_AURA) && !defined(OS_CHROMEOS)
  bool use_non_toplevel_window =
      params->parent &&
#if defined(OS_WIN)
      // Check the force_software_compositing flag only on Windows. If this
      // flag is on, it means that the widget being created wants to use the
      // software compositor which requires a top level window. We cannot have
      // a mixture of compositors active in one view hierarchy.
      !params->force_software_compositing &&
#else
      params->type != views::Widget::InitParams::TYPE_MENU &&
#endif
      params->type != views::Widget::InitParams::TYPE_TOOLTIP;

#if defined(OS_WIN)
  // On desktop Linux Chrome must run in an environment that supports a variety
  // of window managers, some of which do not play nicely with parts of our UI
  // that have specific expectations about window sizing and placement. For this
  // reason windows opened as top level (!params.child) are always constrained
  // by the browser frame, so we can position them correctly. This has some
  // negative side effects, like dialogs being clipped by the browser frame, but
  // the side effects are not as bad as the poor window manager interactions. On
  // Windows however these WM interactions are not an issue, so we open windows
  // requested as top_level as actual top level windows on the desktop.
  use_non_toplevel_window = use_non_toplevel_window && params->child;

  if (!ui::win::IsAeroGlassEnabled()) {
    // If we don't have composition (either because Glass is not enabled or
    // because it was disabled at the command line), anything that requires
    // transparency will be broken with a toplevel window, so force the use of
    // a non toplevel window.
    if (params->opacity == views::Widget::InitParams::TRANSLUCENT_WINDOW &&
        !params->force_software_compositing)
      use_non_toplevel_window = true;
  } else {
    // If we're on Vista+ with composition enabled, then we can use toplevel
    // windows for most things (they get blended via WS_EX_COMPOSITED, which
    // allows for animation effects, but also exceeding the bounds of the parent
    // window).
    if (params->parent &&
        params->type != views::Widget::InitParams::TYPE_CONTROL &&
        params->type != views::Widget::InitParams::TYPE_WINDOW) {
      // When we set this to false, we get a DesktopNativeWidgetAura from the
      // default case (not handled in this function).
      use_non_toplevel_window = false;
    }
  }
#endif  // OS_WIN

  if (!use_non_toplevel_window && !native_widget_factory().is_null()) {
    params->native_widget = native_widget_factory().Run(*params, delegate);
    return;
  }
#endif  // USE_AURA

#if defined(OS_CHROMEOS) || defined(USE_ASH)
  // When we are doing straight chromeos builds, we still need to handle the
  // toplevel window case.
  // There may be a few remaining widgets in Chrome OS that are not top level,
  // but have neither a context nor a parent. Provide a fallback context so
  // users don't crash. Developers will hit the DCHECK and should provide a
  // context.
  if (params->context)
    params->context = params->context->GetRootWindow();
  DCHECK(params->parent || params->context || !params->child)
      << "Please provide a parent or context for this widget.";
  if (!params->parent && !params->context)
    params->context = ash::Shell::GetPrimaryRootWindow();
#elif defined(USE_AURA)
  // While the majority of the time, context wasn't plumbed through due to the
  // existence of a global WindowParentingClient, if this window is toplevel,
  // it's possible that there is no contextual state that we can use.
  if (params->parent == NULL && params->context == NULL && !params->child) {
    params->native_widget = new views::DesktopNativeWidgetAura(delegate);
  } else if (use_non_toplevel_window) {
    views::NativeWidgetAura* native_widget =
        new views::NativeWidgetAura(delegate);
    if (params->parent) {
      Profile* parent_profile = reinterpret_cast<Profile*>(
          params->parent->GetNativeWindowProperty(Profile::kProfileKey));
      native_widget->SetNativeWindowProperty(Profile::kProfileKey,
                                             parent_profile);
    }
    params->native_widget = native_widget;
  } else {
    params->native_widget = new views::DesktopNativeWidgetAura(delegate);
  }
#endif
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
bool ChromeViewsDelegate::WindowManagerProvidesTitleBar(bool maximized) {
  // On Ubuntu Unity, the system always provides a title bar for maximized
  // windows.
  views::LinuxUI* ui = views::LinuxUI::instance();
  return maximized && ui && ui->UnityIsRunning();
}
#endif

ui::ContextFactory* ChromeViewsDelegate::GetContextFactory() {
  return content::GetContextFactory();
}

std::string ChromeViewsDelegate::GetApplicationName() {
  return version_info::GetProductName();
}

#if defined(OS_WIN)
int ChromeViewsDelegate::GetAppbarAutohideEdges(HMONITOR monitor,
                                                const base::Closure& callback) {
  // Initialize the map with EDGE_BOTTOM. This is important, as if we return an
  // initial value of 0 (no auto-hide edges) then we'll go fullscreen and
  // windows will automatically remove WS_EX_TOPMOST from the appbar resulting
  // in us thinking there is no auto-hide edges. By returning at least one edge
  // we don't initially go fullscreen until we figure out the real auto-hide
  // edges.
  if (!appbar_autohide_edge_map_.count(monitor))
    appbar_autohide_edge_map_[monitor] = EDGE_BOTTOM;
  if (monitor && !in_autohide_edges_callback_) {
    base::PostTaskAndReplyWithResult(
        content::BrowserThread::GetBlockingPool(),
        FROM_HERE,
        base::Bind(&GetAppbarAutohideEdgesOnWorkerThread,
                   monitor),
        base::Bind(&ChromeViewsDelegate::OnGotAppbarAutohideEdges,
                   weak_factory_.GetWeakPtr(),
                   callback,
                   monitor,
                   appbar_autohide_edge_map_[monitor]));
  }
  return appbar_autohide_edge_map_[monitor];
}

void ChromeViewsDelegate::OnGotAppbarAutohideEdges(
    const base::Closure& callback,
    HMONITOR monitor,
    int returned_edges,
    int edges) {
  appbar_autohide_edge_map_[monitor] = edges;
  if (returned_edges == edges)
    return;

  base::AutoReset<bool> in_callback_setter(&in_autohide_edges_callback_, true);
  callback.Run();
}
#endif

scoped_refptr<base::TaskRunner>
ChromeViewsDelegate::GetBlockingPoolTaskRunner() {
  return content::BrowserThread::GetBlockingPool();
}

gfx::Insets ChromeViewsDelegate::GetDialogButtonInsets() {
  if (ui::MaterialDesignController::IsSecondaryUiMaterial())
    return gfx::Insets(HarmonyLayoutDelegate::kHarmonyLayoutUnit);
  return ViewsDelegate::GetDialogButtonInsets();
}

int ChromeViewsDelegate::GetDialogRelatedButtonHorizontalSpacing() {
  if (ui::MaterialDesignController::IsSecondaryUiMaterial())
    return HarmonyLayoutDelegate::kHarmonyLayoutUnit / 2;
  return ViewsDelegate::GetDialogRelatedButtonHorizontalSpacing();
}

#if !defined(USE_ASH)
views::Widget::InitParams::WindowOpacity
ChromeViewsDelegate::GetOpacityForInitParams(
    const views::Widget::InitParams& params) {
  return views::Widget::InitParams::OPAQUE_WINDOW;
}
#endif
