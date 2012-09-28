// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/app_list/app_list_controller.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "grit/generated_resources.h"
#include "ui/app_list/app_list_view.h"
#include "ui/app_list/pagination_model.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/win/shell.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/widget/widget.h"

namespace {

// Offset from the cursor to the point of the bubble arrow. It looks weird
// if the arrow comes up right on top of the cursor, so it is offset by this
// amount.
static const int kAnchorOffset = 25;

class AppListControllerWin : public AppListController {
 public:
  AppListControllerWin();
  virtual ~AppListControllerWin();

 private:
  // AppListController overrides:
  virtual void CloseView() OVERRIDE;
  virtual bool IsAppPinned(const std::string& extension_id) OVERRIDE;
  virtual void PinApp(const std::string& extension_id) OVERRIDE;
  virtual void UnpinApp(const std::string& extension_id) OVERRIDE;
  virtual bool CanPin() OVERRIDE;
  virtual void ActivateApp(Profile* profile,
                           const std::string& extension_id,
                           int event_flags) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AppListControllerWin);
};

AppListControllerWin::AppListControllerWin() {
  browser::StartKeepAlive();
}

AppListControllerWin::~AppListControllerWin() {
  browser::EndKeepAlive();
}

void AppListControllerWin::CloseView() {}

bool AppListControllerWin::IsAppPinned(const std::string& extension_id)  {
  return false;
}

void AppListControllerWin::PinApp(const std::string& extension_id) {}

void AppListControllerWin::UnpinApp(const std::string& extension_id) {}

bool AppListControllerWin::CanPin() {
  return false;
}

void AppListControllerWin::ActivateApp(Profile* profile,
                                       const std::string& extension_id,
                                       int event_flags) {
  ExtensionService* service = profile->GetExtensionService();
  DCHECK(service);
  const extensions::Extension* extension = service->GetInstalledExtension(
      extension_id);
  DCHECK(extension);
  application_launch::OpenApplication(application_launch::LaunchParams(
      profile, extension, extension_misc::LAUNCH_TAB, NEW_FOREGROUND_TAB));
}

// The AppListResources class manages global resources needed for the app
// list to operate.
class AppListResources {
 public:
  AppListResources::AppListResources() {}

  app_list::PaginationModel* pagination_model() { return &pagination_model_; }

 private:
  app_list::PaginationModel pagination_model_;

  DISALLOW_COPY_AND_ASSIGN(AppListResources);
};

void GetArrowLocationAndUpdateAnchor(const gfx::Rect& work_area,
                                     int min_space_x,
                                     int min_space_y,
                                     views::BubbleBorder::ArrowLocation* arrow,
                                     gfx::Point* anchor) {
  // Prefer the bottom as it is the most natural position.
  if (anchor->y() - work_area.y() >= min_space_y) {
    *arrow = views::BubbleBorder::BOTTOM_LEFT;
    anchor->Offset(0, -kAnchorOffset);
    return;
  }

  // The view won't fit above the cursor. Will it fit below?
  if (work_area.bottom() - anchor->y() >= min_space_y) {
    *arrow = views::BubbleBorder::TOP_LEFT;
    anchor->Offset(0, kAnchorOffset);
    return;
  }

  // As the view won't fit above or below, try on the right.
  if (work_area.right() - anchor->x() >= min_space_x) {
    *arrow = views::BubbleBorder::LEFT_TOP;
    anchor->Offset(kAnchorOffset, 0);
    return;
  }

  *arrow = views::BubbleBorder::RIGHT_TOP;
  anchor->Offset(-kAnchorOffset, 0);
}

void UpdateArrowPositionAndAnchorPoint(app_list::AppListView* view) {
  static const int kArrowSize = 10;
  static const int kPadding = 20;

  gfx::Size preferred = view->GetPreferredSize();
  // Add the size of the arrow to the space needed, as the preferred size is
  // of the view excluding the arrow.
  int min_space_x = preferred.width() + kAnchorOffset + kPadding + kArrowSize;
  int min_space_y = preferred.height() + kAnchorOffset + kPadding + kArrowSize;

  // TODO(benwells): Make sure the app list does not appear underneath
  // the task bar.
  gfx::Point anchor = view->anchor_point();
  gfx::Display display = gfx::Screen::GetDisplayNearestPoint(anchor);
  const gfx::Rect& display_rect = display.work_area();
  views::BubbleBorder::ArrowLocation arrow;
  GetArrowLocationAndUpdateAnchor(display.work_area(),
                                  min_space_x,
                                  min_space_y,
                                  &arrow,
                                  &anchor);
  view->SetBubbleArrowLocation(arrow);
  view->SetAnchorPoint(anchor);
}

CommandLine GetAppListCommandLine() {
  CommandLine* current = CommandLine::ForCurrentProcess();
  CommandLine command_line(current->GetProgram());

  if (current->HasSwitch(switches::kUserDataDir)) {
    FilePath user_data_dir = current->GetSwitchValuePath(
        switches::kUserDataDir);
    command_line.AppendSwitchPath(switches::kUserDataDir, user_data_dir);
  }

  command_line.AppendSwitch(switches::kShowAppList);
  return command_line;
}

string16 GetAppListIconPath() {
  FilePath icon_path;
  if (!PathService::Get(base::DIR_MODULE, &icon_path))
    return string16();

  icon_path = icon_path.Append(chrome::kBrowserResourcesDll);
  std::stringstream ss;
  ss << ",-" << IDI_APP_LIST;
  string16 result = icon_path.value();
  result.append(UTF8ToUTF16(ss.str()));
  return result;
}

string16 GetAppModelId() {
  static const wchar_t kAppListId[] = L"ChromeAppList";
  // The AppModelId should be the same for all profiles in a user data directory
  // but different for different user data directories, so base it on the
  // initial profile in the current user data directory.
  FilePath initial_profile_path =
      g_browser_process->profile_manager()->GetInitialProfileDir();
  return ShellIntegration::GetAppModelIdForProfile(kAppListId,
                                                   initial_profile_path);
}

base::LazyInstance<AppListResources>::Leaky g_app_list_resources =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace app_list_controller {

void ShowAppList() {
#if !defined(USE_AURA)

  // The controller will be owned by the view delegate, and the delegate is
  // owned by the app list view. The app list view manages it's own lifetime.
  app_list::AppListView* view = new app_list::AppListView(
      new AppListViewDelegate(new AppListControllerWin()));
  gfx::Point cursor = gfx::Screen::GetCursorScreenPoint();
  view->InitAsBubble(
      GetDesktopWindow(),
      g_app_list_resources.Get().pagination_model(),
      NULL,
      cursor,
      views::BubbleBorder::BOTTOM_LEFT);

  UpdateArrowPositionAndAnchorPoint(view);
  HWND hwnd = view->GetWidget()->GetTopLevelWidget()->GetNativeWindow();
  ui::win::SetAppIdForWindow(GetAppModelId(), hwnd);
  CommandLine relaunch = GetAppListCommandLine();
  ui::win::SetRelaunchDetailsForWindow(
      relaunch.GetCommandLineString(),
      l10n_util::GetStringUTF16(IDS_APP_LIST_SHORTCUT_NAME),
      hwnd);
  string16 icon_path = GetAppListIconPath();
  ui::win::SetAppIconForWindow(icon_path, hwnd);
  view->Show();
#endif
}

}  // namespace app_list_controller
