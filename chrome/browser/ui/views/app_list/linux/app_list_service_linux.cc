// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_list/linux/app_list_service_linux.h"

#include "base/memory/singleton.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/shell_integration_linux.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_factory.h"
#include "chrome/browser/ui/app_list/app_list_shower.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/app_list/scoped_keep_alive.h"
#include "chrome/browser/ui/views/app_list/linux/app_list_controller_delegate_linux.h"
#include "chrome/browser/ui/views/app_list/linux/app_list_linux.h"
#include "content/public/browser/browser_thread.h"
#include "grit/chromium_strings.h"
#include "grit/google_chrome_strings.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/screen.h"

namespace {

class AppListFactoryLinux : public AppListFactory {
 public:
  explicit AppListFactoryLinux(AppListServiceLinux* service)
      : service_(service) {}
  virtual ~AppListFactoryLinux() {}

  virtual AppList* CreateAppList(
      Profile* profile,
      AppListService* service,
      const base::Closure& on_should_dismiss) OVERRIDE {
    // The view delegate will be owned by the app list view. The app list view
    // manages it's own lifetime.
    AppListViewDelegate* view_delegate = new AppListViewDelegate(
        profile, service->GetControllerDelegate());
    app_list::AppListView* view = new app_list::AppListView(view_delegate);
    gfx::Point cursor = gfx::Screen::GetNativeScreen()->GetCursorScreenPoint();
    view->InitAsBubbleAtFixedLocation(NULL,
                                      &pagination_model_,
                                      cursor,
                                      views::BubbleBorder::FLOAT,
                                      false /* border_accepts_events */);
    return new AppListLinux(view, on_should_dismiss);
  }

 private:
  // PaginationModel that is shared across all views.
  app_list::PaginationModel pagination_model_;
  AppListServiceLinux* service_;

  DISALLOW_COPY_AND_ASSIGN(AppListFactoryLinux);
};

void CreateShortcuts() {
  std::string app_list_title =
      l10n_util::GetStringUTF8(IDS_APP_LIST_SHORTCUT_NAME);

  if (!ShellIntegrationLinux::CreateAppListDesktopShortcut(
           app_list::kAppListWMClass,
           app_list_title)) {
    LOG(WARNING) << "Unable to create App Launcher shortcut.";
  }
}

}  // namespace

AppListServiceLinux::~AppListServiceLinux() {}

// static
AppListServiceLinux* AppListServiceLinux::GetInstance() {
  return Singleton<AppListServiceLinux,
                   LeakySingletonTraits<AppListServiceLinux> >::get();
}

void AppListServiceLinux::set_can_close(bool can_close) {
  shower_->set_can_close(can_close);
}

void AppListServiceLinux::OnAppListClosing() {
  shower_->CloseAppList();
}

void AppListServiceLinux::Init(Profile* initial_profile) {
  PerformStartupChecks(initial_profile);
}

void AppListServiceLinux::CreateForProfile(Profile* requested_profile) {
  shower_->CreateViewForProfile(requested_profile);
}

void AppListServiceLinux::ShowForProfile(Profile* requested_profile) {
  DCHECK(requested_profile);
  if (requested_profile->IsManaged())
    return;

  ScopedKeepAlive keep_alive;

  InvalidatePendingProfileLoads();
  SetProfilePath(requested_profile->GetPath());
  shower_->ShowForProfile(requested_profile);
  RecordAppListLaunch();
}

void AppListServiceLinux::DismissAppList() {
  shower_->DismissAppList();
}

bool AppListServiceLinux::IsAppListVisible() const {
  return shower_->IsAppListVisible();
}

gfx::NativeWindow AppListServiceLinux::GetAppListWindow() {
  return shower_->GetWindow();
}

Profile* AppListServiceLinux::GetCurrentAppListProfile() {
  return shower_->profile();
}

AppListControllerDelegate* AppListServiceLinux::GetControllerDelegate() {
  return controller_delegate_.get();
}

void AppListServiceLinux::CreateShortcut() {
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE, base::Bind(&CreateShortcuts));
}

AppListServiceLinux::AppListServiceLinux()
    : shower_(new AppListShower(
          scoped_ptr<AppListFactory>(new AppListFactoryLinux(this)),
          this)),
      controller_delegate_(new AppListControllerDelegateLinux(this)) {
}

// static
AppListService* AppListService::Get(chrome::HostDesktopType desktop_type) {
  return AppListServiceLinux::GetInstance();
}

// static
void AppListService::InitAll(Profile* initial_profile) {
  AppListServiceLinux::GetInstance()->Init(initial_profile);
}
