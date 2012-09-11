// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "grit/theme_resources.h"
#include "ui/app_list/app_list_view.h"
#include "ui/app_list/pagination_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/win/shell.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/widget/widget.h"

namespace {

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
  virtual gfx::ImageSkia GetWindowAppIcon() OVERRIDE;

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

gfx::ImageSkia AppListControllerWin::GetWindowAppIcon() {
  gfx::ImageSkia* resource = ResourceBundle::GetSharedInstance().
      GetImageSkiaNamed(IDR_APP_LIST);
  return *resource;
};

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

base::LazyInstance<AppListResources>::Leaky g_app_list_resources =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace app_list_controller {

void ShowAppList() {
#if !defined(USE_AURA)
  static const wchar_t app_list_id[] = L"ChromeAppList";
  // TODO(benwells): Remove these constants and orient the view around the
  // cursor position.
  static const int default_anchor_x = 500;
  static const int default_anchor_y = 750;

  // The controller will be owned by the view delegate, and the delegate is
  // owned by the app list view. The app list view manages it's own lifetime.
  app_list::AppListView* view = new app_list::AppListView(
      new AppListViewDelegate(new AppListControllerWin()));
  view->InitAsBubble(
      GetDesktopWindow(),
      g_app_list_resources.Get().pagination_model(),
      NULL,
      gfx::Point(default_anchor_x, default_anchor_y),
      views::BubbleBorder::BOTTOM_LEFT);
  view->Show();
  view->GetWidget()->GetTopLevelWidget()->UpdateWindowIcon();
  ui::win::SetAppIdForWindow(app_list_id,
      view->GetWidget()->GetTopLevelWidget()->GetNativeWindow());
#endif
}

}  // namespace app_list_controller
