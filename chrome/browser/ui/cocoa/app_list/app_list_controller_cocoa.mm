// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_nsobject.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/app_list_service_mac.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#import "ui/app_list/cocoa/apps_grid_controller.h"
#import "ui/app_list/cocoa/app_list_window_controller.h"

namespace gfx {
class ImageSkia;
}

namespace {

// AppListControllerCocoa manages global resources needed for the app list to
// operate, and controls when the app list is opened and closed.
// TODO(tapted): rename this class to AppListServiceMac and move entire file to
// chrome/browser/ui/app_list/app_list_service_mac.cc .
class AppListControllerCocoa : public AppListService {
 public:
  virtual ~AppListControllerCocoa() {}

  static AppListControllerCocoa* GetInstance() {
    return Singleton<AppListControllerCocoa,
                     LeakySingletonTraits<AppListControllerCocoa> >::get();
  }

  void CreateAppList(Profile* profile);
  NSWindow* GetNativeWindow();

  // AppListService overrides:
  virtual void ShowAppList(Profile* profile) OVERRIDE;
  virtual void DismissAppList() OVERRIDE;
  virtual bool IsAppListVisible() const OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<AppListControllerCocoa>;

  AppListControllerCocoa() {}

  scoped_nsobject<AppListWindowController> window_controller_;

  DISALLOW_COPY_AND_ASSIGN(AppListControllerCocoa);
};

class AppListControllerDelegateCocoa : public AppListControllerDelegate {
 public:
  AppListControllerDelegateCocoa();
  virtual ~AppListControllerDelegateCocoa();

 private:
  // AppListControllerDelegate overrides:
  virtual void DismissView() OVERRIDE;
  virtual gfx::NativeWindow GetAppListWindow() OVERRIDE;
  virtual bool CanPin() OVERRIDE;
  virtual bool CanShowCreateShortcutsDialog() OVERRIDE;
  virtual void ActivateApp(Profile* profile,
                           const extensions::Extension* extension,
                           int event_flags) OVERRIDE;
  virtual void LaunchApp(Profile* profile,
                         const extensions::Extension* extension,
                         int event_flags) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AppListControllerDelegateCocoa);
};

AppListControllerDelegateCocoa::AppListControllerDelegateCocoa() {}

AppListControllerDelegateCocoa::~AppListControllerDelegateCocoa() {}

void AppListControllerDelegateCocoa::DismissView() {
  AppListControllerCocoa::GetInstance()->DismissAppList();
}

gfx::NativeWindow AppListControllerDelegateCocoa::GetAppListWindow() {
  return AppListControllerCocoa::GetInstance()->GetNativeWindow();
}

bool AppListControllerDelegateCocoa::CanPin() {
  return false;
}

bool AppListControllerDelegateCocoa::CanShowCreateShortcutsDialog() {
  // TODO(tapted): Return true when create shortcuts menu is tested on mac.
  return false;
}

void AppListControllerDelegateCocoa::ActivateApp(
    Profile* profile, const extensions::Extension* extension, int event_flags) {
  LaunchApp(profile, extension, event_flags);
}

void AppListControllerDelegateCocoa::LaunchApp(
    Profile* profile, const extensions::Extension* extension, int event_flags) {
  chrome::OpenApplication(chrome::AppLaunchParams(
      profile, extension, NEW_FOREGROUND_TAB));
}

void AppListControllerCocoa::CreateAppList(Profile* profile) {
  scoped_ptr<app_list::AppListViewDelegate> delegate(
      new AppListViewDelegate(new AppListControllerDelegateCocoa(), profile));
  scoped_nsobject<AppsGridController> content(
      [[AppsGridController alloc] initWithViewDelegate:delegate.Pass()]);
  window_controller_.reset(
      [[AppListWindowController alloc] initWithGridController:content]);
}

void AppListControllerCocoa::ShowAppList(Profile* profile) {
  if (!window_controller_)
    CreateAppList(profile);

  [[window_controller_ window] makeKeyAndOrderFront:nil];
}

void AppListControllerCocoa::DismissAppList() {
  if (!window_controller_)
    return;

  [[window_controller_ window] close];
}

bool AppListControllerCocoa::IsAppListVisible() const {
  return [[window_controller_ window] isVisible];
}

NSWindow* AppListControllerCocoa::GetNativeWindow() {
  return [window_controller_ window];
}

}  // namespace

namespace chrome {

AppListService* GetAppListServiceMac() {
  return AppListControllerCocoa::GetInstance();
}

}  // namespace chrome
