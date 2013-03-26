// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "apps/switches.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "base/win/shortcut.h"
#include "base/win/windows_version.h"
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
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/extensions/app_metro_infobar_delegate_win.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/launcher_support/chrome_launcher_support.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_settings.h"
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
#include "win8/util/win8_util.h"

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

void SetDidRunForNDayActiveStats() {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  chrome_launcher_support::InstallationState launcher_state =
      chrome_launcher_support::GetAppLauncherInstallationState();
  if (launcher_state != chrome_launcher_support::NOT_INSTALLED) {
    BrowserDistribution* dist = BrowserDistribution::GetSpecificDistribution(
        BrowserDistribution::CHROME_APP_HOST);
    GoogleUpdateSettings::UpdateDidRunStateForDistribution(
        dist,
        true /* did_run */,
        launcher_state == chrome_launcher_support::INSTALLED_AT_SYSTEM_LEVEL);
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
  virtual void CreateNewWindow(Profile* profile, bool incognito) OVERRIDE;
  virtual void ActivateApp(Profile* profile,
                           const extensions::Extension* extension,
                           int event_flags) OVERRIDE;
  virtual void LaunchApp(Profile* profile,
                         const extensions::Extension* extension,
                         int event_flags) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AppListControllerDelegateWin);
};

class ScopedKeepAlive {
 public:
  ScopedKeepAlive();
  ~ScopedKeepAlive();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedKeepAlive);
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
  void ShowAppListDuringModeSwitch(Profile* profile);

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

  virtual AppListControllerDelegate* CreateControllerDelegate() OVERRIDE;

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
  gfx::Point FindAnchorPoint(const gfx::Display& display,
                             const gfx::Point& cursor);
  void UpdateArrowPositionAndAnchorPoint(const gfx::Point& cursor);
  string16 GetAppListIconPath();

  // Check if the app list or the taskbar has focus. The app list is kept
  // visible whenever either of these have focus, which allows it to be
  // pinned but will hide it if it otherwise loses focus. This is checked
  // periodically whenever the app list does not have focus.
  void CheckTaskbarOrViewHasFocus();

  // Returns the underlying HWND for the AppList.
  HWND GetAppListHWND() const;

  // Utilities to manage browser process keep alive for the view itself. Note
  // keep alives are also used when asynchronously loading profiles.
  void EnsureHaveKeepAliveForView();
  void FreeAnyKeepAliveForView();

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

  // Used to keep the browser process alive while the app list is visible.
  scoped_ptr<ScopedKeepAlive> keep_alive_;

  // True if we are anticipating that the app list will lose focus, and we want
  // to take it back. This is used when switching out of Metro mode, and the
  // browser regains focus after showing the app list.
  bool regain_first_lost_focus_;

  // Incremented to indicate that pending profile loads are no longer valid.
  int profile_load_sequence_id_;

  // How many profile loads are pending.
  int pending_profile_loads_;

  // When the context menu on the app list's taskbar icon is brought up the
  // app list should not be hidden, but it should be if the taskbar is clicked
  // on. There can be a period of time when the taskbar gets focus between a
  // right mouse click and the menu showing; to prevent hiding the app launcher
  // when this happens it is kept visible if the taskbar is seen briefly without
  // the right mouse button down, but not if this happens twice in a row.
  bool preserving_focus_for_taskbar_menu_;

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
  chrome::OpenApplication(chrome::AppLaunchParams(
      profile, extension, NEW_FOREGROUND_TAB));
}

ScopedKeepAlive::ScopedKeepAlive() {
  chrome::StartKeepAlive();
}

ScopedKeepAlive::~ScopedKeepAlive() {
  chrome::EndKeepAlive();
}

AppListController::AppListController()
    : current_view_(NULL),
      view_delegate_(NULL),
      profile_(NULL),
      can_close_app_list_(true),
      regain_first_lost_focus_(false),
      profile_load_sequence_id_(0),
      pending_profile_loads_(0),
      preserving_focus_for_taskbar_menu_(false),
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

AppListControllerDelegate* AppListController::CreateControllerDelegate() {
  return new AppListControllerDelegateWin();
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

  content::BrowserThread::PostBlockingPoolTask(
      FROM_HERE, base::Bind(SetDidRunForNDayActiveStats));

  if (win8::IsSingleWindowMetroMode()) {
    // This request came from Windows 8 in desktop mode, but chrome is currently
    // running in Metro mode.
    chrome::AppMetroInfoBarDelegateWin::Create(
        profile,
        chrome::AppMetroInfoBarDelegateWin::SHOW_APP_LIST,
        std::string());
    return;
  }

  // Invalidate any pending profile path loads.
  profile_load_sequence_id_++;

  // If the app list is already displaying |profile| just activate it (in case
  // we have lost focus).
  if (IsAppListVisible() && (profile == profile_)) {
    current_view_->GetWidget()->Show();
    current_view_->GetWidget()->Activate();
    return;
  }

  SaveProfilePathToLocalState(profile->GetPath());

  DismissAppList();
  PopulateViewFromProfile(profile);

  DCHECK(current_view_);
  EnsureHaveKeepAliveForView();
  gfx::Point cursor = gfx::Screen::GetNativeScreen()->GetCursorScreenPoint();
  UpdateArrowPositionAndAnchorPoint(cursor);
  current_view_->GetWidget()->Show();
  current_view_->GetWidget()->GetTopLevelWidget()->UpdateWindowIcon();
  current_view_->GetWidget()->Activate();
}

void AppListController::InitView(Profile* profile) {
  if (current_view_)
    return;
  PopulateViewFromProfile(profile);
  current_view_->Prerender();
}

void AppListController::ShowAppListDuringModeSwitch(Profile* profile) {
  regain_first_lost_focus_ = true;
  ShowAppList(profile);
}

void AppListController::PopulateViewFromProfile(Profile* profile) {
#if !defined(USE_AURA)
  if (profile == profile_)
    return;
#endif

  profile_ = profile;
  // The controller will be owned by the view delegate, and the delegate is
  // owned by the app list view. The app list view manages it's own lifetime.
  view_delegate_ = new AppListViewDelegate(CreateControllerDelegate(),
                                           profile_);
  current_view_ = new app_list::AppListView(view_delegate_);
  gfx::Point cursor = gfx::Screen::GetNativeScreen()->GetCursorScreenPoint();
  current_view_->InitAsBubble(NULL,
                              &pagination_model_,
                              NULL,
                              cursor,
                              views::BubbleBorder::FLOAT,
                              false /* border_accepts_events */);
  HWND hwnd = GetAppListHWND();

  // Vista and lower do not offer pinning to the taskbar, which makes any
  // presence on the taskbar useless. So, hide the window on the taskbar
  // for these versions of Windows.
  if (base::win::GetVersion() <= base::win::VERSION_VISTA) {
    LONG_PTR ex_styles = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    ex_styles |= WS_EX_TOOLWINDOW;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex_styles);
  }

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
  if (IsAppListVisible() && can_close_app_list_) {
    current_view_->GetWidget()->Hide();
    timer_.Stop();
    FreeAnyKeepAliveForView();
  }
}

void AppListController::AppListClosing() {
  FreeAnyKeepAliveForView();
  current_view_ = NULL;
  view_delegate_ = NULL;
  profile_ = NULL;
  timer_.Stop();
}

void AppListController::AppListActivationChanged(bool active) {
  const int kFocusCheckIntervalMS = 250;
  if (active) {
    timer_.Stop();
    return;
  }

  preserving_focus_for_taskbar_menu_ = false;
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
  if (taskbar_rect.width() == screen_rect.width()) {
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

gfx::Point AppListController::FindAnchorPoint(
    const gfx::Display& display,
    const gfx::Point& cursor) {
  const int kSnapOffset = 3;

  gfx::Rect bounds_rect(display.work_area());
  gfx::Size view_size(current_view_->GetPreferredSize());
  bounds_rect.Inset(view_size.width() / 2 + kSnapOffset,
                    view_size.height() / 2 + kSnapOffset);

  gfx::Point anchor = FindReferencePoint(display, cursor);
  anchor.ClampToMin(bounds_rect.origin());
  anchor.ClampToMax(bounds_rect.bottom_right());
  return anchor;
}

void AppListController::UpdateArrowPositionAndAnchorPoint(
    const gfx::Point& cursor) {
  gfx::Screen* screen =
      gfx::Screen::GetScreenFor(current_view_->GetWidget()->GetNativeView());
  gfx::Display display = screen->GetDisplayNearestPoint(cursor);

  current_view_->SetBubbleArrowLocation(views::BubbleBorder::FLOAT);
  current_view_->SetAnchorPoint(FindAnchorPoint(display, cursor));
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
  // Remember if the taskbar had focus without the right mouse button being
  // down.
  bool was_preserving_focus = preserving_focus_for_taskbar_menu_;
  preserving_focus_for_taskbar_menu_ = false;

  // Don't bother checking if the view has been closed.
  if (!current_view_)
    return;

  // First get the taskbar and jump lists windows (the jump list is the
  // context menu which the taskbar uses).
  HWND jump_list_hwnd = FindWindow(L"DV2ControlHost", NULL);
  HWND taskbar_hwnd = FindWindow(kTrayClassName, NULL);

  HWND app_list_hwnd = GetAppListHWND();

  // This code is designed to hide the app launcher when it loses focus, except
  // for the cases necessary to allow the launcher to be pinned or closed via
  // the taskbar context menu.
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

    // If the focused window is NULL, and the mouse button is not being pressed,
    // then the launcher no longer has focus so hide it.
    DismissAppList();
    return;
  }

  while (focused_hwnd) {
    // If the focused window is the right click menu (called a jump list) or
    // the app list, don't hide the launcher.
    if (focused_hwnd == jump_list_hwnd ||
        focused_hwnd == app_list_hwnd) {
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

  if (regain_first_lost_focus_) {
    regain_first_lost_focus_ = false;
    current_view_->GetWidget()->Activate();
    return;
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

void AppListController::EnsureHaveKeepAliveForView() {
  if (!keep_alive_)
    keep_alive_.reset(new ScopedKeepAlive());
}

void AppListController::FreeAnyKeepAliveForView() {
  if (keep_alive_)
    keep_alive_.reset(NULL);
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
          apps::switches::kShowAppListShortcut);

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

    if (!base::win::TaskbarPinShortcutLink(shortcut_path.value().c_str()))
      LOG(WARNING) << "Failed to pin AppList using " << shortcut_path.value();

    return;
  }

  if (!should_show && file_util::PathExists(shortcut_path)) {
    base::win::TaskbarUnpinShortcutLink(shortcut_path.value().c_str());
    file_util::Delete(shortcut_path, false);
  }
}

void InitView(Profile* profile) {
  if (!g_browser_process || g_browser_process->IsShuttingDown())
    return;
  AppListController::GetInstance()->InitView(profile);
}

void AppListController::Init(Profile* initial_profile) {
  // In non-Ash metro mode, we can not show the app list for this process, so do
  // not bother performing Init tasks.
  if (win8::IsSingleWindowMetroMode())
    return;

  PrefService* prefs = g_browser_process->local_state();
  if (prefs->HasPrefPath(prefs::kRestartWithAppList) &&
      prefs->GetBoolean(prefs::kRestartWithAppList)) {
    prefs->SetBoolean(prefs::kRestartWithAppList, false);
    AppListController::GetInstance()->
        ShowAppListDuringModeSwitch(initial_profile);
  }

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
  return current_view_ && current_view_->GetWidget()->IsVisible();
}

}  // namespace

namespace chrome {

AppListService* GetAppListServiceWin() {
  return AppListController::GetInstance();
}

}  // namespace chrome
