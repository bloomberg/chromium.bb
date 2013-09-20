// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dwmapi.h>
#include <sstream>

#include "apps/pref_names.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/win/shortcut.h"
#include "base/win/windows_version.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_service_impl.h"
#include "chrome/browser/ui/app_list/app_list_service_win.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/apps/app_metro_infobar_delegate_win.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/launcher_support/chrome_launcher_support.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/util_constants.h"
#include "content/public/browser/browser_thread.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/win/shell.h"
#include "ui/gfx/display.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/screen.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/widget/widget.h"
#include "win8/util/win8_util.h"

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/installer/util/install_util.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#endif

namespace {

// Offset from the cursor to the point of the bubble arrow. It looks weird
// if the arrow comes up right on top of the cursor, so it is offset by this
// amount.
static const int kAnchorOffset = 25;

static const wchar_t kTrayClassName[] = L"Shell_TrayWnd";

// Migrate chrome::kAppLauncherIsEnabled pref to
// chrome::kAppLauncherHasBeenEnabled pref.
void MigrateAppLauncherEnabledPref() {
  PrefService* prefs = g_browser_process->local_state();
  if (prefs->HasPrefPath(apps::prefs::kAppLauncherIsEnabled)) {
    prefs->SetBoolean(apps::prefs::kAppLauncherHasBeenEnabled,
                      prefs->GetBoolean(apps::prefs::kAppLauncherIsEnabled));
    prefs->ClearPref(apps::prefs::kAppLauncherIsEnabled);
  }
}

// Icons are added to the resources of the DLL using icon names. The icon index
// for the app list icon is named IDR_X_APP_LIST or (for official builds)
// IDR_X_APP_LIST_SXS for Chrome Canary. Creating shortcuts needs to specify a
// resource index, which are different to icon names.  They are 0 based and
// contiguous. As Google Chrome builds have extra icons the icon for Google
// Chrome builds need to be higher. Unfortunately these indexes are not in any
// generated header file.
int GetAppListIconIndex() {
  const int kAppListIconIndex = 5;
  const int kAppListIconIndexSxS = 6;
  const int kAppListIconIndexChromium = 1;
#if defined(GOOGLE_CHROME_BUILD)
  if (InstallUtil::IsChromeSxSProcess())
    return kAppListIconIndexSxS;
  return kAppListIconIndex;
#else
  return kAppListIconIndexChromium;
#endif
}

string16 GetAppListIconPath() {
  base::FilePath icon_path;
  if (!PathService::Get(base::FILE_EXE, &icon_path)) {
    NOTREACHED();
    return string16();
  }

  std::stringstream ss;
  ss << "," << GetAppListIconIndex();
  string16 result = icon_path.value();
  result.append(UTF8ToUTF16(ss.str()));
  return result;
}

// Utility methods for showing the app list.
// Attempts to find the bounds of the Windows taskbar. Returns true on success.
// |rect| is in screen coordinates. If the taskbar is in autohide mode and is
// not visible, |rect| will be outside the current monitor's bounds, except for
// one pixel of overlap where the edge of the taskbar is shown.
bool GetTaskbarRect(gfx::Rect* rect) {
  HWND taskbar_hwnd = FindWindow(kTrayClassName, NULL);
  if (!taskbar_hwnd)
    return false;

  RECT win_rect;
  if (!GetWindowRect(taskbar_hwnd, &win_rect))
    return false;

  *rect = gfx::Rect(win_rect);
  return true;
}

gfx::Point FindReferencePoint(const gfx::Display& display,
                              const gfx::Point& cursor) {
  const int kSnapDistance = 50;

  // If we can't find the taskbar, snap to the bottom left.
  // If the display size is the same as the work area, and does not contain the
  // taskbar, either the taskbar is hidden or on another monitor, so just snap
  // to the bottom left.
  gfx::Rect taskbar_rect;
  if (!GetTaskbarRect(&taskbar_rect) ||
      (display.work_area() == display.bounds() &&
          !display.work_area().Contains(taskbar_rect))) {
    return display.work_area().bottom_left();
  }

  // Snap to the taskbar edge. If the cursor is greater than kSnapDistance away,
  // also move to the left (for horizontal taskbars) or top (for vertical).
  const gfx::Rect& screen_rect = display.bounds();
  // First handle taskbar on bottom.
  // Note on Windows 8 the work area won't include split windows on the left or
  // right, and neither will |taskbar_rect|.
  if (taskbar_rect.width() == display.work_area().width()) {
    if (taskbar_rect.bottom() == screen_rect.bottom()) {
      if (taskbar_rect.y() - cursor.y() > kSnapDistance)
        return gfx::Point(screen_rect.x(), taskbar_rect.y());

      return gfx::Point(cursor.x(), taskbar_rect.y());
    }

    // Now try on the top.
    if (cursor.y() - taskbar_rect.bottom() > kSnapDistance)
      return gfx::Point(screen_rect.x(), taskbar_rect.bottom());

    return gfx::Point(cursor.x(), taskbar_rect.bottom());
  }

  // Now try the left.
  if (taskbar_rect.x() == screen_rect.x()) {
    if (cursor.x() - taskbar_rect.right() > kSnapDistance)
      return gfx::Point(taskbar_rect.right(), screen_rect.y());

    return gfx::Point(taskbar_rect.right(), cursor.y());
  }

  // Finally, try the right.
  if (taskbar_rect.x() - cursor.x() > kSnapDistance)
    return gfx::Point(taskbar_rect.x(), screen_rect.y());

  return gfx::Point(taskbar_rect.x(), cursor.y());
}

gfx::Point FindAnchorPoint(
    const gfx::Size view_size,
    const gfx::Display& display,
    const gfx::Point& cursor) {
  const int kSnapOffset = 3;

  gfx::Rect bounds_rect(display.work_area());
  // Always subtract the taskbar area since work_area() will not subtract it
  // if the taskbar is set to auto-hide, and the app list should never overlap
  // the taskbar.
  gfx::Rect taskbar_rect;
  if (GetTaskbarRect(&taskbar_rect))
    bounds_rect.Subtract(taskbar_rect);

  bounds_rect.Inset(view_size.width() / 2 + kSnapOffset,
                    view_size.height() / 2 + kSnapOffset);

  gfx::Point anchor = FindReferencePoint(display, cursor);
  anchor.SetToMax(bounds_rect.origin());
  anchor.SetToMin(bounds_rect.bottom_right());
  return anchor;
}
string16 GetAppListShortcutName() {
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_CANARY)
    return l10n_util::GetStringUTF16(IDS_APP_LIST_SHORTCUT_NAME_CANARY);
  return l10n_util::GetStringUTF16(IDS_APP_LIST_SHORTCUT_NAME);
}

CommandLine GetAppListCommandLine() {
  const char* const kSwitchesToCopy[] = { switches::kUserDataDir };
  CommandLine* current = CommandLine::ForCurrentProcess();
  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
     NOTREACHED();
     return CommandLine(CommandLine::NO_PROGRAM);
  }
  CommandLine command_line(chrome_exe);
  command_line.CopySwitchesFrom(*current, kSwitchesToCopy,
                                arraysize(kSwitchesToCopy));
  command_line.AppendSwitch(switches::kShowAppList);
  return command_line;
}

string16 GetAppModelId() {
  // The AppModelId should be the same for all profiles in a user data directory
  // but different for different user data directories, so base it on the
  // initial profile in the current user data directory.
  base::FilePath initial_profile_path;
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUserDataDir)) {
    initial_profile_path =
        command_line->GetSwitchValuePath(switches::kUserDataDir).AppendASCII(
            chrome::kInitialProfile);
  }
  return ShellIntegration::GetAppListAppModelIdForProfile(initial_profile_path);
}

void SetDidRunForNDayActiveStats() {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  base::FilePath exe_path;
  if (!PathService::Get(base::DIR_EXE, &exe_path)) {
    NOTREACHED();
    return;
  }
  bool system_install =
      !InstallUtil::IsPerUserInstall(exe_path.value().c_str());
  // Using Chrome Binary dist: Chrome dist may not exist for the legacy
  // App Launcher, and App Launcher dist may be "shadow", which does not
  // contain the information needed to determine multi-install.
  // Edge case involving Canary: crbug/239163.
  BrowserDistribution* chrome_binaries_dist =
      BrowserDistribution::GetSpecificDistribution(
          BrowserDistribution::CHROME_BINARIES);
  if (chrome_binaries_dist &&
      InstallUtil::IsMultiInstall(chrome_binaries_dist, system_install)) {
    BrowserDistribution* app_launcher_dist =
        BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_APP_HOST);
    GoogleUpdateSettings::UpdateDidRunStateForDistribution(
        app_launcher_dist,
        true /* did_run */,
        system_install);
  }
}

// The start menu shortcut is created on first run by users that are
// upgrading. The desktop and taskbar shortcuts are created the first time the
// user enables the app list. The taskbar shortcut is created in
// |user_data_dir| and will use a Windows Application Model Id of
// |app_model_id|. This runs on the FILE thread and not in the blocking IO
// thread pool as there are other tasks running (also on the FILE thread)
// which fiddle with shortcut icons
// (ShellIntegration::MigrateWin7ShortcutsOnPath). Having different threads
// fiddle with the same shortcuts could cause race issues.
void CreateAppListShortcuts(
    const base::FilePath& user_data_dir,
    const string16& app_model_id,
    const ShellIntegration::ShortcutLocations& creation_locations) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  // Shortcut paths under which to create shortcuts.
  std::vector<base::FilePath> shortcut_paths =
      web_app::internals::GetShortcutPaths(creation_locations);

  bool pin_to_taskbar = creation_locations.in_quick_launch_bar &&
                        (base::win::GetVersion() >= base::win::VERSION_WIN7);

  // Create a shortcut in the |user_data_dir| for taskbar pinning.
  if (pin_to_taskbar)
    shortcut_paths.push_back(user_data_dir);
  bool success = true;

  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
    return;
  }

  string16 app_list_shortcut_name = GetAppListShortcutName();

  string16 wide_switches(GetAppListCommandLine().GetArgumentsString());

  base::win::ShortcutProperties shortcut_properties;
  shortcut_properties.set_target(chrome_exe);
  shortcut_properties.set_working_dir(chrome_exe.DirName());
  shortcut_properties.set_arguments(wide_switches);
  shortcut_properties.set_description(app_list_shortcut_name);
  shortcut_properties.set_icon(chrome_exe, GetAppListIconIndex());
  shortcut_properties.set_app_id(app_model_id);

  for (size_t i = 0; i < shortcut_paths.size(); ++i) {
    base::FilePath shortcut_file =
        shortcut_paths[i].Append(app_list_shortcut_name).
            AddExtension(installer::kLnkExt);
    if (!base::PathExists(shortcut_file.DirName()) &&
        !file_util::CreateDirectory(shortcut_file.DirName())) {
      NOTREACHED();
      return;
    }
    success = success && base::win::CreateOrUpdateShortcutLink(
        shortcut_file, shortcut_properties,
        base::win::SHORTCUT_CREATE_ALWAYS);
  }

  if (success && pin_to_taskbar) {
    base::FilePath shortcut_to_pin =
        user_data_dir.Append(app_list_shortcut_name).
            AddExtension(installer::kLnkExt);
    success = base::win::TaskbarPinShortcutLink(
        shortcut_to_pin.value().c_str()) && success;
  }
}

class AppListControllerDelegateWin : public AppListControllerDelegate {
 public:
  AppListControllerDelegateWin();
  virtual ~AppListControllerDelegateWin();

 private:
  // AppListController overrides:
  virtual void DismissView() OVERRIDE;
  virtual void ViewClosing() OVERRIDE;
  virtual gfx::NativeWindow GetAppListWindow() OVERRIDE;
  virtual gfx::ImageSkia GetWindowIcon() OVERRIDE;
  virtual bool CanPin() OVERRIDE;
  virtual void OnShowExtensionPrompt() OVERRIDE;
  virtual void OnCloseExtensionPrompt() OVERRIDE;
  virtual bool CanDoCreateShortcutsFlow(bool is_platform_app) OVERRIDE;
  virtual void DoCreateShortcutsFlow(Profile* profile,
                                     const std::string& extension_id) OVERRIDE;
  virtual void CreateNewWindow(Profile* profile, bool incognito) OVERRIDE;
  virtual void ActivateApp(Profile* profile,
                           const extensions::Extension* extension,
                           int event_flags) OVERRIDE;
  virtual void LaunchApp(Profile* profile,
                         const extensions::Extension* extension,
                         int event_flags) OVERRIDE;
  virtual void ShowForProfileByPath(
      const base::FilePath& profile_path) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AppListControllerDelegateWin);
};

// Customizes the app list |hwnd| for Windows (eg: disable aero peek, set up
// restart params).
void SetWindowAttributes(HWND hwnd) {
  // Vista and lower do not offer pinning to the taskbar, which makes any
  // presence on the taskbar useless. So, hide the window on the taskbar
  // for these versions of Windows.
  if (base::win::GetVersion() <= base::win::VERSION_VISTA) {
    LONG_PTR ex_styles = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    ex_styles |= WS_EX_TOOLWINDOW;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex_styles);
  }

  if (base::win::GetVersion() > base::win::VERSION_VISTA) {
    // Disable aero peek. Without this, hovering over the taskbar popup puts
    // Windows into a mode for switching between windows in the same
    // application. The app list has just one window, so it is just distracting.
    BOOL disable_value = TRUE;
    ::DwmSetWindowAttribute(hwnd,
                            DWMWA_DISALLOW_PEEK,
                            &disable_value,
                            sizeof(disable_value));
  }

  ui::win::SetAppIdForWindow(GetAppModelId(), hwnd);
  CommandLine relaunch = GetAppListCommandLine();
  string16 app_name(GetAppListShortcutName());
  ui::win::SetRelaunchDetailsForWindow(
      relaunch.GetCommandLineString(), app_name, hwnd);
  ::SetWindowText(hwnd, app_name.c_str());
  string16 icon_path = GetAppListIconPath();
  ui::win::SetAppIconForWindow(icon_path, hwnd);
}

class ScopedKeepAlive {
 public:
  ScopedKeepAlive() { chrome::StartKeepAlive(); }
  ~ScopedKeepAlive() { chrome::EndKeepAlive(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedKeepAlive);
};

class ActivationTracker : public app_list::AppListView::Observer {
 public:
  ActivationTracker(app_list::AppListView* view,
                    const base::Closure& on_should_dismiss)
      : view_(view),
        on_should_dismiss_(on_should_dismiss),
        regain_next_lost_focus_(false),
        preserving_focus_for_taskbar_menu_(false) {
    view_->AddObserver(this);
  }

  ~ActivationTracker() {
    view_->RemoveObserver(this);
  }

  void RegainNextLostFocus() {
    regain_next_lost_focus_ = true;
  }

  virtual void OnActivationChanged(
      views::Widget* widget, bool active) OVERRIDE {
    const int kFocusCheckIntervalMS = 250;
    if (active) {
      timer_.Stop();
      return;
    }

    preserving_focus_for_taskbar_menu_ = false;
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromMilliseconds(kFocusCheckIntervalMS), this,
                 &ActivationTracker::CheckTaskbarOrViewHasFocus);
  }

  void OnViewHidden() {
    timer_.Stop();
  }

  void CheckTaskbarOrViewHasFocus() {
    // Remember if the taskbar had focus without the right mouse button being
    // down.
    bool was_preserving_focus = preserving_focus_for_taskbar_menu_;
    preserving_focus_for_taskbar_menu_ = false;

    // First get the taskbar and jump lists windows (the jump list is the
    // context menu which the taskbar uses).
    HWND jump_list_hwnd = FindWindow(L"DV2ControlHost", NULL);
    HWND taskbar_hwnd = FindWindow(kTrayClassName, NULL);

    // This code is designed to hide the app launcher when it loses focus,
    // except for the cases necessary to allow the launcher to be pinned or
    // closed via the taskbar context menu.
    // First work out if the left or right button is currently down.
    int swapped = GetSystemMetrics(SM_SWAPBUTTON);
    int left_button = swapped ? VK_RBUTTON : VK_LBUTTON;
    bool left_button_down = GetAsyncKeyState(left_button) < 0;
    int right_button = swapped ? VK_LBUTTON : VK_RBUTTON;
    bool right_button_down = GetAsyncKeyState(right_button) < 0;

    // Now get the window that currently has focus.
    HWND focused_hwnd = GetForegroundWindow();
    if (!focused_hwnd) {
      // Sometimes the focused window is NULL. This can happen when the focus is
      // changing due to a mouse button press. If the button is still being
      // pressed the launcher should not be hidden.
      if (right_button_down || left_button_down)
        return;

      // If the focused window is NULL, and the mouse button is not being
      // pressed, then the launcher no longer has focus.
      on_should_dismiss_.Run();
      return;
    }

    while (focused_hwnd) {
      // If the focused window is the right click menu (called a jump list) or
      // the app list, don't hide the launcher.
      if (focused_hwnd == jump_list_hwnd ||
          focused_hwnd == view_->GetHWND()) {
        return;
      }

      if (focused_hwnd == taskbar_hwnd) {
        // If the focused window is the taskbar, and the right button is down,
        // don't hide the launcher as the user might be bringing up the menu.
        if (right_button_down)
          return;

        // There is a short period between the right mouse button being down
        // and the menu gaining focus, where the taskbar has focus and no button
        // is down. If the taskbar is observed in this state once the launcher
        // is not dismissed. If it happens twice in a row it is dismissed.
        if (!was_preserving_focus) {
          preserving_focus_for_taskbar_menu_ = true;
          return;
        }

        break;
      }
      focused_hwnd = GetParent(focused_hwnd);
    }

    if (regain_next_lost_focus_) {
      regain_next_lost_focus_ = false;
      view_->GetWidget()->Activate();
      return;
    }

    // If we get here, the focused window is not the taskbar, it's context menu,
    // or the app list.
    on_should_dismiss_.Run();
  }

 private:
  // The window to track the active state of.
  app_list::AppListView* view_;

  // Called to request |view_| be closed.
  base::Closure on_should_dismiss_;

  // True if we are anticipating that the app list will lose focus, and we want
  // to take it back. This is used when switching out of Metro mode, and the
  // browser regains focus after showing the app list.
  bool regain_next_lost_focus_;

  // When the context menu on the app list's taskbar icon is brought up the
  // app list should not be hidden, but it should be if the taskbar is clicked
  // on. There can be a period of time when the taskbar gets focus between a
  // right mouse click and the menu showing; to prevent hiding the app launcher
  // when this happens it is kept visible if the taskbar is seen briefly without
  // the right mouse button down, but not if this happens twice in a row.
  bool preserving_focus_for_taskbar_menu_;

  // Timer used to check if the taskbar or app list is active. Using a timer
  // means we don't need to hook Windows, which is apparently not possible
  // since Vista (and is not nice at any time).
  base::RepeatingTimer<ActivationTracker> timer_;
};

// Responsible for positioning, hiding and showing an AppListView on Windows.
// This includes watching window activation/deactivation messages to determine
// if the user has clicked away from it.
class AppListViewWin {
 public:
  AppListViewWin(app_list::AppListView* view,
                 const base::Closure& on_should_dismiss)
    : view_(view),
      activation_tracker_(view, on_should_dismiss),
      window_icon_updated_(false) {
  }

  app_list::AppListView* view() { return view_; }

  void Show() {
    view_->GetWidget()->Show();
    if (!window_icon_updated_) {
      view_->GetWidget()->GetTopLevelWidget()->UpdateWindowIcon();
      window_icon_updated_ = true;
    }
    view_->GetWidget()->Activate();
  }

  void ShowNearCursor(const gfx::Point& cursor) {
    UpdateArrowPositionAndAnchorPoint(cursor);
    Show();
  }

  void Hide() {
    view_->GetWidget()->Hide();
    activation_tracker_.OnViewHidden();
  }

  bool IsVisible() {
    return view_->GetWidget()->IsVisible();
  }

  void Prerender() {
    view_->Prerender();
  }

  void RegainNextLostFocus() {
    activation_tracker_.RegainNextLostFocus();
  }

  gfx::NativeWindow GetWindow() {
    return view_->GetWidget()->GetNativeWindow();
  }

  void OnSigninStatusChanged() {
    view_->OnSigninStatusChanged();
  }

  void SetProfileByPath(const base::FilePath profile_path) {
    view_->SetProfileByPath(profile_path);
  }

 private:
  void UpdateArrowPositionAndAnchorPoint(const gfx::Point& cursor) {
    gfx::Screen* screen =
        gfx::Screen::GetScreenFor(view_->GetWidget()->GetNativeView());
    gfx::Display display = screen->GetDisplayNearestPoint(cursor);

    view_->SetBubbleArrow(views::BubbleBorder::FLOAT);
    view_->SetAnchorPoint(FindAnchorPoint(
        view_->GetPreferredSize(), display, cursor));
  }


  // Weak pointer. The view manages its own lifetime.
  app_list::AppListView* view_;
  ActivationTracker activation_tracker_;
  bool window_icon_updated_;

  DISALLOW_COPY_AND_ASSIGN(AppListViewWin);
};

// Factory for AppListViews. Used to allow us to create fake views in tests.
class AppListViewFactory {
 public:
  AppListViewFactory() {}
  virtual ~AppListViewFactory() {}

  AppListViewWin* CreateAppListView(
      Profile* profile,
      app_list::PaginationModel* pagination_model,
      const base::Closure& on_should_dismiss) {
    // The controller will be owned by the view delegate, and the delegate is
    // owned by the app list view. The app list view manages it's own lifetime.
    // TODO(koz): Make AppListViewDelegate take a scoped_ptr.
    AppListViewDelegate* view_delegate = new AppListViewDelegate(
        new AppListControllerDelegateWin, profile);
    app_list::AppListView* view = new app_list::AppListView(view_delegate);
    gfx::Point cursor = gfx::Screen::GetNativeScreen()->GetCursorScreenPoint();
    view->InitAsBubbleAtFixedLocation(NULL,
                                      pagination_model,
                                      cursor,
                                      views::BubbleBorder::FLOAT,
                                      false /* border_accepts_events */);
    SetWindowAttributes(view->GetHWND());
    return new AppListViewWin(view, on_should_dismiss);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListViewFactory);
};

// Creates and shows AppListViews as needed.
class AppListShower {
 public:
  explicit AppListShower(scoped_ptr<AppListViewFactory> factory)
      : factory_(factory.Pass()),
        profile_(NULL),
        can_close_app_list_(true) {
  }

  void set_can_close(bool can_close) {
    can_close_app_list_ = can_close;
  }

  void ShowAndReacquireFocus(Profile* requested_profile) {
    ShowForProfile(requested_profile);
    view_->RegainNextLostFocus();
  }

  void ShowForProfile(Profile* requested_profile) {
    // If the app list is already displaying |profile| just activate it (in case
    // we have lost focus).
    if (IsAppListVisible() && (requested_profile == profile_)) {
      view_->Show();
      return;
    }

    // If the current view exists, switch the delegate's profile and rebuild the
    // model.
    if (!view_) {
      CreateViewForProfile(requested_profile);
    } else if (requested_profile != profile_) {
      profile_ = requested_profile;
      view_->SetProfileByPath(requested_profile->GetPath());
    }

    DCHECK(view_);
    EnsureHaveKeepAliveForView();
    // If the app list isn't visible, move the app list to the cursor position
    // before showing it.
    if (!IsAppListVisible()) {
      gfx::Point cursor =
          gfx::Screen::GetNativeScreen()->GetCursorScreenPoint();
      view_->ShowNearCursor(cursor);
    } else {
      view_->Show();
    }
  }

  gfx::NativeWindow GetWindow() {
    if (!IsAppListVisible())
      return NULL;
    return view_->GetWindow();
  }

  app_list::AppListModel* GetGurrentModel() {
    if (!view_)
      return NULL;
    return view_->view()->model();
  }

  void OnSigninStatusChanged() {
    if (view_)
      view_->OnSigninStatusChanged();
  }

  // Create or recreate, and initialize |view_| from |requested_profile|.
  void CreateViewForProfile(Profile* requested_profile) {
    // Aura has problems with layered windows and bubble delegates. The app
    // launcher has a trick where it only hides the window when it is dismissed,
    // reshowing it again later. This does not work with win aura for some
    // reason. This change temporarily makes it always get recreated, only on
    // win aura. See http://crbug.com/176186.
#if !defined(USE_AURA)
    if (requested_profile == profile_)
      return;
#endif

    profile_ = requested_profile;
    view_.reset(factory_->CreateAppListView(
        profile_, &pagination_model_,
        base::Bind(&AppListShower::DismissAppList, base::Unretained(this))));
  }

  void DismissAppList() {
    if (view_ && can_close_app_list_) {
      view_->Hide();
      FreeAnyKeepAliveForView();
    }
  }

  void CloseAppList() {
    view_.reset();
    profile_ = NULL;
    FreeAnyKeepAliveForView();
  }

  bool IsAppListVisible() const {
    return view_ && view_->IsVisible();
  }

  void WarmupForProfile(Profile* profile) {
    DCHECK(!profile_);
    CreateViewForProfile(profile);
    view_->Prerender();
  }

  bool HasView() const {
    return !!view_;
  }

 private:
  void EnsureHaveKeepAliveForView() {
    if (!keep_alive_)
      keep_alive_.reset(new ScopedKeepAlive());
  }

  void FreeAnyKeepAliveForView() {
    if (keep_alive_) {
      // We may end up here as the result of the OS deleting the AppList's
      // widget (WidgetObserver::OnWidgetDestroyed). If this happens and there
      // are no browsers around then deleting the keep alive will result in
      // deleting the Widget again (by way of CloseAllSecondaryWidgets). When
      // the stack unravels we end up back in the Widget that was deleted and
      // crash. By delaying deletion of the keep alive we ensure the Widget has
      // correctly been destroyed before ending the keep alive so that
      // CloseAllSecondaryWidgets() won't attempt to delete the AppList's Widget
      // again.
      base::MessageLoop::current()->DeleteSoon(FROM_HERE,
                                               keep_alive_.release());
    }
  }

  scoped_ptr<AppListViewFactory> factory_;
  scoped_ptr<AppListViewWin> view_;
  Profile* profile_;
  bool can_close_app_list_;

  // PaginationModel that is shared across all views.
  app_list::PaginationModel pagination_model_;

  // Used to keep the browser process alive while the app list is visible.
  scoped_ptr<ScopedKeepAlive> keep_alive_;

  DISALLOW_COPY_AND_ASSIGN(AppListShower);
};

// The AppListController class manages global resources needed for the app
// list to operate, and controls when the app list is opened and closed.
// TODO(tapted): Rename this class to AppListServiceWin and move entire file to
// chrome/browser/ui/app_list/app_list_service_win.cc after removing
// chrome/browser/ui/views dependency.
class AppListController : public AppListServiceWin {
 public:
  virtual ~AppListController();

  static AppListController* GetInstance() {
    return Singleton<AppListController,
                     LeakySingletonTraits<AppListController> >::get();
  }

  void set_can_close(bool can_close) {
    shower_->set_can_close(can_close);
  }

  void OnAppListClosing();

  // AppListService overrides:
  virtual void SetAppListNextPaintCallback(
      const base::Closure& callback) OVERRIDE;
  virtual void HandleFirstRun() OVERRIDE;
  virtual void Init(Profile* initial_profile) OVERRIDE;
  virtual void CreateForProfile(Profile* requested_profile) OVERRIDE;
  virtual void ShowForProfile(Profile* requested_profile) OVERRIDE;
  virtual void DismissAppList() OVERRIDE;
  virtual bool IsAppListVisible() const OVERRIDE;
  virtual gfx::NativeWindow GetAppListWindow() OVERRIDE;
  virtual AppListControllerDelegate* CreateControllerDelegate() OVERRIDE;

  // AppListServiceImpl overrides:
  virtual void CreateShortcut() OVERRIDE;

  // AppListServiceWin overrides:
  virtual app_list::AppListModel* GetAppListModelForTesting() OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<AppListController>;

  AppListController();

  bool IsWarmupNeeded();
  void ScheduleWarmup();

  // Loads the profile last used with the app list and populates the view from
  // it without showing it so that the next show is faster. Does nothing if the
  // view already exists, or another profile is in the middle of being loaded to
  // be shown.
  void LoadProfileForWarmup();
  void OnLoadProfileForWarmup(Profile* initial_profile);

  // Responsible for putting views on the screen.
  scoped_ptr<AppListShower> shower_;

  bool enable_app_list_on_next_init_;

  base::WeakPtrFactory<AppListController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppListController);
};

AppListControllerDelegateWin::AppListControllerDelegateWin() {}

AppListControllerDelegateWin::~AppListControllerDelegateWin() {}

void AppListControllerDelegateWin::DismissView() {
  AppListController::GetInstance()->DismissAppList();
}

void AppListControllerDelegateWin::ViewClosing() {
  AppListController::GetInstance()->OnAppListClosing();
}

gfx::NativeWindow AppListControllerDelegateWin::GetAppListWindow() {
  return AppListController::GetInstance()->GetAppListWindow();
}

gfx::ImageSkia AppListControllerDelegateWin::GetWindowIcon() {
  gfx::ImageSkia* resource = ResourceBundle::GetSharedInstance().
      GetImageSkiaNamed(chrome::GetAppListIconResourceId());
  return *resource;
}

bool AppListControllerDelegateWin::CanPin() {
  return false;
}

void AppListControllerDelegateWin::ShowForProfileByPath(
    const base::FilePath& profile_path) {
  AppListService* service = AppListController::GetInstance();
  service->SetProfilePath(profile_path);
  service->Show();
}

void AppListControllerDelegateWin::OnShowExtensionPrompt() {
  AppListController::GetInstance()->set_can_close(false);
}

void AppListControllerDelegateWin::OnCloseExtensionPrompt() {
  AppListController::GetInstance()->set_can_close(true);
}

bool AppListControllerDelegateWin::CanDoCreateShortcutsFlow(
    bool is_platform_app) {
  return true;
}

void AppListControllerDelegateWin::DoCreateShortcutsFlow(
    Profile* profile,
    const std::string& extension_id) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  DCHECK(service);
  const extensions::Extension* extension = service->GetInstalledExtension(
      extension_id);
  DCHECK(extension);

  gfx::NativeWindow parent_hwnd = GetAppListWindow();
  if (!parent_hwnd)
    return;
  OnShowExtensionPrompt();
  chrome::ShowCreateChromeAppShortcutsDialog(
      parent_hwnd, profile, extension,
      base::Bind(&AppListControllerDelegateWin::OnCloseExtensionPrompt,
                 base::Unretained(this)));
}

void AppListControllerDelegateWin::CreateNewWindow(Profile* profile,
                                                   bool incognito) {
  Profile* window_profile = incognito ?
      profile->GetOffTheRecordProfile() : profile;
  chrome::NewEmptyWindow(window_profile, chrome::GetActiveDesktop());
}

void AppListControllerDelegateWin::ActivateApp(
    Profile* profile, const extensions::Extension* extension, int event_flags) {
  LaunchApp(profile, extension, event_flags);
}

void AppListControllerDelegateWin::LaunchApp(
    Profile* profile, const extensions::Extension* extension, int event_flags) {
  AppListServiceImpl::RecordAppListAppLaunch();
  chrome::OpenApplication(chrome::AppLaunchParams(
      profile, extension, NEW_FOREGROUND_TAB));
}

AppListController::AppListController()
    : enable_app_list_on_next_init_(false),
      shower_(new AppListShower(make_scoped_ptr(new AppListViewFactory))),
      weak_factory_(this) {}

AppListController::~AppListController() {
}

gfx::NativeWindow AppListController::GetAppListWindow() {
  return shower_->GetWindow();
}

AppListControllerDelegate* AppListController::CreateControllerDelegate() {
  return new AppListControllerDelegateWin();
}

app_list::AppListModel* AppListController::GetAppListModelForTesting() {
  return shower_->GetGurrentModel();
}

void AppListController::ShowForProfile(Profile* requested_profile) {
  DCHECK(requested_profile);
  if (requested_profile->IsManaged())
    return;

  ScopedKeepAlive show_app_list_keepalive;

  content::BrowserThread::PostBlockingPoolTask(
      FROM_HERE, base::Bind(SetDidRunForNDayActiveStats));

  if (win8::IsSingleWindowMetroMode()) {
    // This request came from Windows 8 in desktop mode, but chrome is currently
    // running in Metro mode.
    AppMetroInfoBarDelegateWin::Create(
        requested_profile, AppMetroInfoBarDelegateWin::SHOW_APP_LIST,
        std::string());
    return;
  }

  InvalidatePendingProfileLoads();
  // TODO(koz): Investigate making SetProfile() call SetProfilePath() itself.
  SetProfilePath(requested_profile->GetPath());
  SetProfile(requested_profile);
  shower_->ShowForProfile(requested_profile);
  RecordAppListLaunch();
}

void AppListController::DismissAppList() {
  shower_->DismissAppList();
}

void AppListController::OnAppListClosing() {
  shower_->CloseAppList();
  SetProfile(NULL);
}

void AppListController::OnLoadProfileForWarmup(Profile* initial_profile) {
  if (!IsWarmupNeeded())
    return;

  shower_->WarmupForProfile(initial_profile);
}

void AppListController::SetAppListNextPaintCallback(
    const base::Closure& callback) {
  app_list::AppListView::SetNextPaintCallback(callback);
}

void AppListController::HandleFirstRun() {
  PrefService* local_state = g_browser_process->local_state();
  // If the app list is already enabled during first run, then the user had
  // opted in to the app launcher before uninstalling, so we re-enable to
  // restore shortcuts to the app list.
  // Note we can't directly create the shortcuts here because the IO thread
  // hasn't been created yet.
  enable_app_list_on_next_init_ = local_state->GetBoolean(
      apps::prefs::kAppLauncherHasBeenEnabled);
}

void AppListController::Init(Profile* initial_profile) {
  // In non-Ash metro mode, we can not show the app list for this process, so do
  // not bother performing Init tasks.
  if (win8::IsSingleWindowMetroMode())
    return;

  if (enable_app_list_on_next_init_) {
    enable_app_list_on_next_init_ = false;
    EnableAppList(initial_profile);
    CreateShortcut();
  }

  PrefService* prefs = g_browser_process->local_state();
  if (prefs->HasPrefPath(prefs::kRestartWithAppList) &&
      prefs->GetBoolean(prefs::kRestartWithAppList)) {
    prefs->SetBoolean(prefs::kRestartWithAppList, false);
    // If we are restarting in Metro mode we will lose focus straight away. We
    // need to reacquire focus when that happens.
    shower_->ShowAndReacquireFocus(initial_profile);
  }

  // Migrate from legacy app launcher if we are on a non-canary and non-chromium
  // build.
#if defined(GOOGLE_CHROME_BUILD)
  if (!InstallUtil::IsChromeSxSProcess() &&
      !chrome_launcher_support::GetAnyAppHostPath().empty()) {
    chrome_launcher_support::InstallationState state =
        chrome_launcher_support::GetAppLauncherInstallationState();
    if (state == chrome_launcher_support::NOT_INSTALLED) {
      // If app_host.exe is found but can't be located in the registry,
      // skip the migration as this is likely a developer build.
      return;
    } else if (state == chrome_launcher_support::INSTALLED_AT_SYSTEM_LEVEL) {
      chrome_launcher_support::UninstallLegacyAppLauncher(
          chrome_launcher_support::SYSTEM_LEVEL_INSTALLATION);
    } else if (state == chrome_launcher_support::INSTALLED_AT_USER_LEVEL) {
      chrome_launcher_support::UninstallLegacyAppLauncher(
          chrome_launcher_support::USER_LEVEL_INSTALLATION);
    }
    EnableAppList(initial_profile);
    CreateShortcut();
  }
#endif

  // Instantiate AppListController so it listens for profile deletions.
  AppListController::GetInstance();

  ScheduleWarmup();

  MigrateAppLauncherEnabledPref();
  HandleCommandLineFlags(initial_profile);
}

void AppListController::CreateForProfile(Profile* profile) {
  shower_->CreateViewForProfile(profile);
}

bool AppListController::IsAppListVisible() const {
  return shower_->IsAppListVisible();
}

void AppListController::CreateShortcut() {
  // Check if the app launcher shortcuts have ever been created before.
  // Shortcuts should only be created once. If the user unpins the taskbar
  // shortcut, they can restore it by pinning the start menu or desktop
  // shortcut.
  ShellIntegration::ShortcutLocations shortcut_locations;
  shortcut_locations.on_desktop = true;
  shortcut_locations.in_quick_launch_bar = true;
  shortcut_locations.in_applications_menu = true;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  shortcut_locations.applications_menu_subdir =
      dist->GetStartMenuShortcutSubfolder(
          BrowserDistribution::SUBFOLDER_CHROME);
  base::FilePath user_data_dir(
      g_browser_process->profile_manager()->user_data_dir());

  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&CreateAppListShortcuts,
                 user_data_dir, GetAppModelId(), shortcut_locations));
}

void AppListController::ScheduleWarmup() {
  // Post a task to create the app list. This is posted to not impact startup
  // time.
  const int kInitWindowDelay = 5;
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&AppListController::LoadProfileForWarmup,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kInitWindowDelay));

  // Send app list usage stats after a delay.
  const int kSendUsageStatsDelay = 5;
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&AppListController::SendAppListStats),
      base::TimeDelta::FromSeconds(kSendUsageStatsDelay));
}

bool AppListController::IsWarmupNeeded() {
  if (!g_browser_process || g_browser_process->IsShuttingDown())
    return false;

  // We only need to initialize the view if there's no view already created and
  // there's no profile loading to be shown.
  return !shower_->HasView() && !profile_loader().IsAnyProfileLoading();
}

void AppListController::LoadProfileForWarmup() {
  if (!IsWarmupNeeded())
    return;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path(GetProfilePath(profile_manager->user_data_dir()));

  profile_loader().LoadProfileInvalidatingOtherLoads(
      profile_path,
      base::Bind(&AppListController::OnLoadProfileForWarmup,
                 weak_factory_.GetWeakPtr()));
}

}  // namespace

namespace chrome {

AppListService* GetAppListServiceWin() {
  return AppListController::GetInstance();
}

}  // namespace chrome
