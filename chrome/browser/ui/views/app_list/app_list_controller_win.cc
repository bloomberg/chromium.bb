// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/time.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "base/win/shortcut.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/app_list_service_win.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/util_constants.h"
#include "content/public/browser/browser_thread.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "grit/theme_resources.h"
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

#if defined(USE_ASH)
#include "ash/shell.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#endif

namespace {

// Offset from the cursor to the point of the bubble arrow. It looks weird
// if the arrow comes up right on top of the cursor, so it is offset by this
// amount.
static const int kAnchorOffset = 25;

static const wchar_t kTrayClassName[] = L"Shell_TrayWnd";

// Icons are added to the resources of the DLL using icon names. The icon index
// for the app list icon is named IDR_X_APP_LIST. Creating shortcuts needs to
// specify a resource index, which are different to icon names.  They are 0
// based and contiguous. As Google Chrome builds have extra icons the icon for
// Google Chrome builds need to be higher. Unfortunately these indexes are not
// in any generated header file.
#if defined(GOOGLE_CHROME_BUILD)
const int kAppListIconIndex = 5;
#else
const int kAppListIconIndex = 1;
#endif

CommandLine GetAppListCommandLine() {
  const char* const kSwitchesToCopy[] = { switches::kUserDataDir };
  CommandLine* current = CommandLine::ForCurrentProcess();
  CommandLine command_line(current->GetProgram());
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

class AppListControllerDelegateWin : public AppListControllerDelegate {
 public:
  AppListControllerDelegateWin();
  virtual ~AppListControllerDelegateWin();

 private:
  // AppListController overrides:
  virtual void DismissView() OVERRIDE;
  virtual void ViewClosing() OVERRIDE;
  virtual void ViewActivationChanged(bool active) OVERRIDE;
  virtual gfx::NativeWindow GetAppListWindow() OVERRIDE;
  virtual gfx::ImageSkia GetWindowIcon() OVERRIDE;
  virtual bool CanPin() OVERRIDE;
  virtual void OnShowExtensionPrompt() OVERRIDE;
  virtual void OnCloseExtensionPrompt() OVERRIDE;
  virtual bool CanShowCreateShortcutsDialog() OVERRIDE;
  virtual void ShowCreateShortcutsDialog(
      Profile* profile,
      const std::string& extension_id) OVERRIDE;
  virtual void ActivateApp(Profile* profile,
                           const extensions::Extension* extension,
                           int event_flags) OVERRIDE;
  virtual void LaunchApp(Profile* profile,
                         const extensions::Extension* extension,
                         int event_flags) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AppListControllerDelegateWin);
};

// The AppListController class manages global resources needed for the app
// list to operate, and controls when the app list is opened and closed.
// TODO(tapted): Rename this class to AppListServiceWin and move entire file to
// chrome/browser/ui/app_list/app_list_service_win.cc after removing
// chrome/browser/ui/views dependency.
class AppListController : public AppListService {
 public:
  virtual ~AppListController();

  static AppListController* GetInstance() {
    return Singleton<AppListController,
                     LeakySingletonTraits<AppListController> >::get();
  }

  void set_can_close(bool can_close) { can_close_app_list_ = can_close; }
  bool can_close() { return can_close_app_list_; }
  Profile* profile() const { return profile_; }

  // Creates the app list view and populates it from |profile|, but doesn't
  // show it.  Does nothing if the view already exists.
  void InitView(Profile* profile);

  void AppListClosing();
  void AppListActivationChanged(bool active);

  app_list::AppListView* GetView() { return current_view_; }

  // AppListService overrides:
  virtual void Init(Profile* initial_profile) OVERRIDE;

  // Activates the app list at the current mouse cursor location, creating the
  // app list if necessary.
  virtual void ShowAppList(Profile* profile) OVERRIDE;

  // Hides the app list.
  virtual void DismissAppList() OVERRIDE;

  // Update the profile path stored in local prefs, load it (if not already
  // loaded), and show the app list.
  virtual void SetAppListProfile(
      const base::FilePath& profile_file_path) OVERRIDE;

  virtual Profile* GetCurrentAppListProfile() OVERRIDE;

  virtual bool IsAppListVisible() const OVERRIDE;

  // ProfileInfoCacheObserver override:
  // We need to watch for profile removal to keep kAppListProfile updated.
  virtual void OnProfileWillBeRemoved(
      const base::FilePath& profile_path) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<AppListController>;

  AppListController();

  // Loads a profile asynchronously and calls OnProfileLoaded() when done.
  void LoadProfileAsync(const base::FilePath& profile_file_path);

  // Callback for asynchronous profile load.
  void OnProfileLoaded(int profile_load_sequence_id,
                       Profile* profile,
                       Profile::CreateStatus status);

  // We need to keep the browser alive while we are loading a profile as that
  // shows intent to show the app list. These two functions track our pending
  // profile loads and start or end browser keep alive accordingly.
  void IncrementPendingProfileLoads();
  void DecrementPendingProfileLoads();

  // Create or recreate, and initialize |current_view_| from |profile|.
  void PopulateViewFromProfile(Profile* profile);

  // Save |profile_file_path| as the app list profile in local state.
  void SaveProfilePathToLocalState(const base::FilePath& profile_file_path);

  // Utility methods for showing the app list.
  void SnapArrowLocationToTaskbarEdge(
      const gfx::Display& display,
      gfx::Point* anchor);
  void UpdateArrowPositionAndAnchorPoint(const gfx::Point& cursor);
  string16 GetAppListIconPath();

  // Check if the app list or the taskbar has focus. The app list is kept
  // visible whenever either of these have focus, which allows it to be
  // pinned but will hide it if it otherwise loses focus. This is checked
  // periodically whenever the app list does not have focus.
  void CheckTaskbarOrViewHasFocus();

  // Returns the underlying HWND for the AppList.
  HWND GetAppListHWND() const;

  // Weak pointer. The view manages its own lifetime.
  app_list::AppListView* current_view_;

  // Weak pointer. The view owns the view delegate.
  AppListViewDelegate* view_delegate_;

  // Timer used to check if the taskbar or app list is active. Using a timer
  // means we don't need to hook Windows, which is apparently not possible
  // since Vista (and is not nice at any time).
  base::RepeatingTimer<AppListController> timer_;

  app_list::PaginationModel pagination_model_;

  // The profile the AppList is currently displaying.
  Profile* profile_;

  // True if the controller can close the app list.
  bool can_close_app_list_;

  // True if the app list is showing. Used to ensure we only ever have 0 or 1
  // browser process keep-alives active.
  bool app_list_is_showing_;

  // Incremented to indicate that pending profile loads are no longer valid.
  int profile_load_sequence_id_;

  // How many profile loads are pending.
  int pending_profile_loads_;

  base::WeakPtrFactory<AppListController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppListController);
};

AppListControllerDelegateWin::AppListControllerDelegateWin() {}

AppListControllerDelegateWin::~AppListControllerDelegateWin() {}

void AppListControllerDelegateWin::DismissView() {
  AppListController::GetInstance()->DismissAppList();
}

void AppListControllerDelegateWin::ViewActivationChanged(bool active) {
  AppListController::GetInstance()->AppListActivationChanged(active);
}

void AppListControllerDelegateWin::ViewClosing() {
  AppListController::GetInstance()->AppListClosing();
}

gfx::NativeWindow AppListControllerDelegateWin::GetAppListWindow() {
  app_list::AppListView* view = AppListController::GetInstance()->GetView();
  return view ? view->GetWidget()->GetNativeWindow() : NULL;
}

gfx::ImageSkia AppListControllerDelegateWin::GetWindowIcon() {
  gfx::ImageSkia* resource = ResourceBundle::GetSharedInstance().
      GetImageSkiaNamed(IDR_APP_LIST);
  return *resource;
}

bool AppListControllerDelegateWin::CanPin() {
  return false;
}

void AppListControllerDelegateWin::OnShowExtensionPrompt() {
  AppListController::GetInstance()->set_can_close(false);
}

void AppListControllerDelegateWin::OnCloseExtensionPrompt() {
  AppListController::GetInstance()->set_can_close(true);
}

bool AppListControllerDelegateWin::CanShowCreateShortcutsDialog() {
  return true;
}

void AppListControllerDelegateWin::ShowCreateShortcutsDialog(
    Profile* profile,
    const std::string& extension_id) {
  ExtensionService* service = profile->GetExtensionService();
  DCHECK(service);
  const extensions::Extension* extension = service->GetInstalledExtension(
      extension_id);
  DCHECK(extension);

  app_list::AppListView* view = AppListController::GetInstance()->GetView();
  if (!view)
    return;

  gfx::NativeWindow parent_hwnd =
      view->GetWidget()->GetTopLevelWidget()->GetNativeWindow();
  chrome::ShowCreateChromeAppShortcutsDialog(parent_hwnd, profile, extension);
}

void AppListControllerDelegateWin::ActivateApp(
    Profile* profile, const extensions::Extension* extension, int event_flags) {
  LaunchApp(profile, extension, event_flags);
}

void AppListControllerDelegateWin::LaunchApp(
    Profile* profile, const extensions::Extension* extension, int event_flags) {
  chrome::OpenApplication(chrome::AppLaunchParams(
      profile, extension, NEW_FOREGROUND_TAB));
}

AppListController::AppListController()
    : current_view_(NULL),
      view_delegate_(NULL),
      profile_(NULL),
      can_close_app_list_(true),
      app_list_is_showing_(false),
      profile_load_sequence_id_(0),
      pending_profile_loads_(0),
      weak_factory_(this) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  profile_manager->GetProfileInfoCache().AddObserver(this);
}

AppListController::~AppListController() {
}

void AppListController::OnProfileWillBeRemoved(
    const base::FilePath& profile_path) {
  // If the profile the app list uses just got deleted, reset it to the last
  // used profile.
  PrefService* local_state = g_browser_process->local_state();
  std::string app_list_last_profile = local_state->GetString(
      prefs::kAppListProfile);
  if (profile_path.BaseName().MaybeAsASCII() == app_list_last_profile) {
    local_state->SetString(prefs::kAppListProfile,
        local_state->GetString(prefs::kProfileLastUsed));
  }
}

void AppListController::SetAppListProfile(
    const base::FilePath& profile_file_path) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetProfileByPath(profile_file_path);

  if (!profile) {
    LoadProfileAsync(profile_file_path);
    return;
  }

  ShowAppList(profile);
}

void AppListController::LoadProfileAsync(
    const base::FilePath& profile_file_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Invalidate any pending profile path loads.
  profile_load_sequence_id_++;

  IncrementPendingProfileLoads();

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  profile_manager->CreateProfileAsync(
      profile_file_path,
      base::Bind(&AppListController::OnProfileLoaded,
                 weak_factory_.GetWeakPtr(), profile_load_sequence_id_),
      string16(), string16(), false);
}

void AppListController::OnProfileLoaded(int profile_load_sequence_id,
                                        Profile* profile,
                                        Profile::CreateStatus status) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  switch (status) {
    case Profile::CREATE_STATUS_CREATED:
      break;
    case Profile::CREATE_STATUS_INITIALIZED:
      // Only show if there has been no other profile shown since this load
      // started.
      if (profile_load_sequence_id == profile_load_sequence_id_)
        ShowAppList(profile);
      DecrementPendingProfileLoads();
      break;
    case Profile::CREATE_STATUS_FAIL:
      DecrementPendingProfileLoads();
      break;
  }

}

void AppListController::IncrementPendingProfileLoads() {
  pending_profile_loads_++;
  if (pending_profile_loads_ == 1)
    chrome::StartKeepAlive();
}

void AppListController::DecrementPendingProfileLoads() {
  pending_profile_loads_--;
  if (pending_profile_loads_ == 0)
    chrome::EndKeepAlive();
}

void AppListController::ShowAppList(Profile* profile) {
  DCHECK(profile);

  // Invalidate any pending profile path loads.
  profile_load_sequence_id_++;

  // If the app list is already displaying |profile| just activate it (in case
  // we have lost focus).
  if (app_list_is_showing_ && (profile == profile_)) {
    current_view_->Show();
    current_view_->GetWidget()->Activate();
    return;
  }

  SaveProfilePathToLocalState(profile->GetPath());

  DismissAppList();
  PopulateViewFromProfile(profile);

  if (!app_list_is_showing_) {
    app_list_is_showing_ = true;
    chrome::StartKeepAlive();
  }

  DCHECK(current_view_);
  DCHECK(app_list_is_showing_);
  gfx::Point cursor = gfx::Screen::GetNativeScreen()->GetCursorScreenPoint();
  UpdateArrowPositionAndAnchorPoint(cursor);
  current_view_->Show();
  current_view_->GetWidget()->GetTopLevelWidget()->UpdateWindowIcon();
  current_view_->GetWidget()->Activate();
}

void AppListController::InitView(Profile* profile) {
  if (current_view_)
    return;
  PopulateViewFromProfile(profile);
}

void AppListController::PopulateViewFromProfile(Profile* profile) {
#if !defined(USE_AURA)
  if (profile == profile_)
    return;
#endif

  profile_ = profile;
  gfx::NativeWindow parent = NULL;

#if !defined(USE_AURA)
  parent = ::GetDesktopWindow();
#endif
  // The controller will be owned by the view delegate, and the delegate is
  // owned by the app list view. The app list view manages it's own lifetime.
  view_delegate_ = new AppListViewDelegate(new AppListControllerDelegateWin(),
                                           profile_);
  current_view_ = new app_list::AppListView(view_delegate_);
  gfx::Point cursor = gfx::Screen::GetNativeScreen()->GetCursorScreenPoint();
  current_view_->InitAsBubble(parent,
                              &pagination_model_,
                              NULL,
                              cursor,
                              views::BubbleBorder::FLOAT,
                              false /* border_accepts_events */);
  HWND hwnd = GetAppListHWND();

  ui::win::SetAppIdForWindow(GetAppModelId(), hwnd);
  CommandLine relaunch = GetAppListCommandLine();
  string16 app_name(l10n_util::GetStringUTF16(IDS_APP_LIST_SHORTCUT_NAME));
  ui::win::SetRelaunchDetailsForWindow(
      relaunch.GetCommandLineString(), app_name, hwnd);
  ::SetWindowText(hwnd, app_name.c_str());
  string16 icon_path = GetAppListIconPath();
  ui::win::SetAppIconForWindow(icon_path, hwnd);
}

void AppListController::SaveProfilePathToLocalState(
    const base::FilePath& profile_file_path) {
  g_browser_process->local_state()->SetString(
      prefs::kAppListProfile,
      profile_file_path.BaseName().MaybeAsASCII());
}

void AppListController::DismissAppList() {
  if (current_view_ && app_list_is_showing_ && can_close_app_list_) {
    current_view_->GetWidget()->Hide();
    timer_.Stop();
    chrome::EndKeepAlive();
    app_list_is_showing_ = false;
  }
}

void AppListController::AppListClosing() {
  current_view_ = NULL;
  view_delegate_ = NULL;
  timer_.Stop();
}

void AppListController::AppListActivationChanged(bool active) {
  const int kFocusCheckIntervalMS = 250;
  if (active) {
    timer_.Stop();
    return;
  }

  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(kFocusCheckIntervalMS), this,
               &AppListController::CheckTaskbarOrViewHasFocus);
}

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

// Used to position the view relative to a point, which requires |anchor| to be
// in the center of the desired view location. This helper function updates
// |anchor| thus, using the location of the point in |point|, the distance
// to move the view from |point| in |offset|, and the direction to move
// represented in |direction|.
// To position relative to a screen corner, the absolute values of |direction|.x
// and |direction.y| should both be 1. To position relative to some point on the
// taskbar, one of the absolute values of |direction.x| and |direction.y| should
// be 1 and the other 0.
void FloatFromPoint(const gfx::Point& point,
                    const gfx::Size& offset,
                    const gfx::Point& direction,
                    gfx::Point* anchor) {
  anchor->set_x(point.x() + direction.x() * offset.width());
  anchor->set_y(point.y() + direction.y() * offset.height());
}

void AppListController::SnapArrowLocationToTaskbarEdge(
    const gfx::Display& display,
    gfx::Point* anchor) {
  const int kSnapDistance = 50;
  const int kSnapOffset = 5;

  gfx::Rect taskbar_rect;
  gfx::Size float_offset = current_view_->GetPreferredSize();
  float_offset.set_width(float_offset.width() / 2);
  float_offset.set_height(float_offset.height() / 2);
  float_offset.Enlarge(kSnapOffset, kSnapOffset);

  // If we can't find the taskbar, snap to the bottom left.
  // If the display size is the same as the work area, and does not contain the
  // taskbar, either the taskbar is hidden or on another monitor, so just snap
  // to the bottom left.
  if (!GetTaskbarRect(&taskbar_rect) ||
      (display.work_area() == display.bounds() &&
          !display.work_area().Contains(taskbar_rect))) {
    FloatFromPoint(display.work_area().bottom_left(),
                   float_offset,
                   gfx::Point(1, -1),
                   anchor);
    return;
  }

  const gfx::Rect& screen_rect = display.bounds();

  // Snap to the taskbar edge. If the cursor is greater than kSnapDistance away,
  // also move to the left (for horizontal taskbars) or top (for vertical).

  // First handle taskbar on bottom.
  if (taskbar_rect.width() == screen_rect.width()) {
    if (taskbar_rect.bottom() == screen_rect.bottom()) {
      if (taskbar_rect.y() - anchor->y() > kSnapDistance) {
        FloatFromPoint(gfx::Point(screen_rect.x(), taskbar_rect.y()),
                       float_offset,
                       gfx::Point(1, -1),
                       anchor);
        return;
      }

      FloatFromPoint(gfx::Point(anchor->x(), taskbar_rect.y()),
                     float_offset,
                     gfx::Point(0, -1),
                     anchor);
      return;
    }

    // Now try on the top.
    if (anchor->y() - taskbar_rect.bottom() > kSnapDistance) {
      FloatFromPoint(gfx::Point(screen_rect.x(), taskbar_rect.bottom()),
                     float_offset,
                     gfx::Point(1, 1),
                     anchor);
      return;
    }

    FloatFromPoint(gfx::Point(anchor->x(), taskbar_rect.bottom()),
                   float_offset,
                   gfx::Point(0, 1),
                   anchor);
    return;
  }

  // Now try the left.
  if (taskbar_rect.x() == screen_rect.x()) {
    if (anchor->x() - taskbar_rect.right() > kSnapDistance) {
      FloatFromPoint(gfx::Point(taskbar_rect.right(), screen_rect.y()),
                     float_offset,
                     gfx::Point(1, 1),
                     anchor);
      return;
    }

    FloatFromPoint(gfx::Point(taskbar_rect.right(), anchor->y()),
                   float_offset,
                   gfx::Point(1, 0),
                   anchor);
    return;
  }

  // Finally, try the right.
  if (taskbar_rect.x() - anchor->x() > kSnapDistance) {
    FloatFromPoint(gfx::Point(taskbar_rect.x(), screen_rect.y()),
                   float_offset,
                   gfx::Point(-1, 1),
                   anchor);
    return;
  }

  FloatFromPoint(gfx::Point(taskbar_rect.x(), anchor->y()),
                 float_offset,
                 gfx::Point(-1, 0),
                 anchor);
}

void AppListController::UpdateArrowPositionAndAnchorPoint(
    const gfx::Point& cursor) {
  gfx::Point anchor(cursor);
  gfx::Screen* screen =
      gfx::Screen::GetScreenFor(current_view_->GetWidget()->GetNativeView());
  gfx::Display display = screen->GetDisplayNearestPoint(anchor);

  SnapArrowLocationToTaskbarEdge(display, &anchor);

  current_view_->SetBubbleArrowLocation(views::BubbleBorder::FLOAT);
  current_view_->SetAnchorPoint(anchor);
}

string16 AppListController::GetAppListIconPath() {
  base::FilePath icon_path;
  if (!PathService::Get(base::FILE_EXE, &icon_path)) {
    NOTREACHED();
    return string16();
  }

  std::stringstream ss;
  ss << "," << kAppListIconIndex;
  string16 result = icon_path.value();
  result.append(UTF8ToUTF16(ss.str()));
  return result;
}

void AppListController::CheckTaskbarOrViewHasFocus() {
  // Don't bother checking if the view has been closed.
  if (!current_view_)
    return;

  // First get the taskbar and jump lists windows (the jump list is the
  // context menu which the taskbar uses).
  HWND jump_list_hwnd = FindWindow(L"DV2ControlHost", NULL);
  HWND taskbar_hwnd = FindWindow(kTrayClassName, NULL);

  HWND app_list_hwnd = GetAppListHWND();

  // Get the focused window, and check if it is one of these windows. Keep
  // checking it's parent until either we find one of these windows, or there
  // is no parent left.
  HWND focused_hwnd = GetForegroundWindow();
  while (focused_hwnd) {
    if (focused_hwnd == jump_list_hwnd ||
        focused_hwnd == taskbar_hwnd ||
        focused_hwnd == app_list_hwnd) {
      return;
    }
    focused_hwnd = GetParent(focused_hwnd);
  }

  // If we get here, the focused window is not the taskbar, it's context menu,
  // or the app list, so close the app list.
  DismissAppList();
}

HWND AppListController::GetAppListHWND() const {
#if defined(USE_AURA)
  gfx::NativeWindow window =
      current_view_->GetWidget()->GetTopLevelWidget()->GetNativeWindow();
  return window->GetRootWindow()->GetAcceleratedWidget();
#else
  return current_view_->GetWidget()->GetTopLevelWidget()->GetNativeWindow();
#endif
}

// Check that a taskbar shortcut exists if it should, or does not exist if
// it should not. A taskbar shortcut should exist if the switch
// kShowAppListShortcut is set. The shortcut will be created or deleted in
// |user_data_dir| and will use a Windows Application Model Id of
// |app_model_id|.
// This runs on the FILE thread and not in the blocking IO thread pool as there
// are other tasks running (also on the FILE thread) which fiddle with shortcut
// icons (ShellIntegration::MigrateWin7ShortcutsOnPath). Having different
// threads fiddle with the same shortcuts could cause race issues.
void CheckAppListTaskbarShortcutOnFileThread(
    const base::FilePath& user_data_dir,
    const string16& app_model_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  const string16 shortcut_name = l10n_util::GetStringUTF16(
      IDS_APP_LIST_SHORTCUT_NAME);
  const base::FilePath shortcut_path(user_data_dir.Append(shortcut_name)
      .AddExtension(installer::kLnkExt));
  const bool should_show =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kShowAppListShortcut);

  // This will not reshow a shortcut if it has been unpinned manually by the
  // user, as that will not delete the shortcut file.
  if (should_show && !file_util::PathExists(shortcut_path)) {
    base::FilePath chrome_exe;
    if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
      NOTREACHED();
      return;
    }

    base::win::ShortcutProperties shortcut_properties;
    shortcut_properties.set_target(chrome_exe);
    shortcut_properties.set_working_dir(chrome_exe.DirName());

    string16 wide_switches(GetAppListCommandLine().GetArgumentsString());
    shortcut_properties.set_arguments(wide_switches);
    shortcut_properties.set_description(shortcut_name);

    shortcut_properties.set_icon(chrome_exe, kAppListIconIndex);
    shortcut_properties.set_app_id(app_model_id);

    base::win::CreateOrUpdateShortcutLink(shortcut_path, shortcut_properties,
                                          base::win::SHORTCUT_CREATE_ALWAYS);
    base::win::TaskbarPinShortcutLink(shortcut_path.value().c_str());
    return;
  }

  if (!should_show && file_util::PathExists(shortcut_path)) {
    base::win::TaskbarUnpinShortcutLink(shortcut_path.value().c_str());
    file_util::Delete(shortcut_path, false);
  }
}

void InitView(Profile* profile) {
  AppListController::GetInstance()->InitView(profile);
}

void AppListController::Init(Profile* initial_profile) {
  // Check that the app list shortcut matches the flag kShowAppListShortcut.
  // This will either create or delete a shortcut file in the user data
  // directory.
  // TODO(benwells): Remove this and the flag once the app list installation
  // is implemented.
  static bool checked_shortcut = false;
  if (!checked_shortcut) {
    checked_shortcut = true;
    base::FilePath user_data_dir(
        g_browser_process->profile_manager()->user_data_dir());
    content::BrowserThread::PostTask(
        content::BrowserThread::FILE, FROM_HERE,
        base::Bind(&CheckAppListTaskbarShortcutOnFileThread, user_data_dir,
                   GetAppModelId()));
  }

  // Instantiate AppListController so it listens for profile deletions.
  AppListController::GetInstance();

  // Post a task to create the app list. This is posted to not impact startup
  // time.
  const int kInitWindowDelay = 5;
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&::InitView, initial_profile),
      base::TimeDelta::FromSeconds(kInitWindowDelay));
}

Profile* AppListController::GetCurrentAppListProfile() {
  return profile();
}

bool AppListController::IsAppListVisible() const {
  return app_list_is_showing_;
}

}  // namespace

namespace chrome {

AppListService* GetAppListServiceWin() {
  return AppListController::GetInstance();
}

}  // namespace chrome
