// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_list/linux/app_list_service_linux.h"

#include "base/memory/singleton.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_factory.h"
#include "chrome/browser/ui/app_list/app_list_shower.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/app_list/keep_alive_service_impl.h"
#include "chrome/browser/ui/views/app_list/linux/app_list_controller_delegate_linux.h"
#include "chrome/browser/ui/views/app_list/linux/app_list_linux.h"
#include "ui/app_list/views/app_list_view.h"
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
    // TODO(mgiuca): Set the app launcher window's WM_CLASS so that it can be
    // associated with its own launch icon in the window manager.
    return new AppListLinux(view, on_should_dismiss);
  }

 private:
  // PaginationModel that is shared across all views.
  app_list::PaginationModel pagination_model_;
  AppListServiceLinux* service_;

  DISALLOW_COPY_AND_ASSIGN(AppListFactoryLinux);
};

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
  HandleCommandLineFlags(initial_profile);
}

void AppListServiceLinux::CreateForProfile(Profile* requested_profile) {
  shower_->CreateViewForProfile(requested_profile);
}

void AppListServiceLinux::ShowForProfile(Profile* requested_profile) {
  DCHECK(requested_profile);
  if (requested_profile->IsManaged())
    return;

  ScopedKeepAlive show_app_list_keepalive;

  // TODO(mgiuca): Call SetDidRunForNDayActiveStats.

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
  NOTIMPLEMENTED();
}

AppListServiceLinux::AppListServiceLinux()
    : shower_(new AppListShower(
          scoped_ptr<AppListFactory>(new AppListFactoryLinux(this)),
          scoped_ptr<KeepAliveService>(new KeepAliveServiceImpl),
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
