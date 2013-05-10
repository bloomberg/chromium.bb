// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_nsobject.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/app_list_service_impl.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#import "ui/app_list/cocoa/app_list_view_controller.h"
#import "ui/app_list/cocoa/app_list_window_controller.h"

namespace gfx {
class ImageSkia;
}

namespace {

// AppListServiceMac manages global resources needed for the app list to
// operate, and controls when the app list is opened and closed.
class AppListServiceMac : public AppListServiceImpl {
 public:
  virtual ~AppListServiceMac() {}

  static AppListServiceMac* GetInstance() {
    return Singleton<AppListServiceMac,
                     LeakySingletonTraits<AppListServiceMac> >::get();
  }

  void CreateAppList(Profile* profile);
  NSWindow* GetNativeWindow();

  // AppListService overrides:
  virtual void ShowAppList(Profile* requested_profile) OVERRIDE;
  virtual void DismissAppList() OVERRIDE;
  virtual bool IsAppListVisible() const OVERRIDE;
  virtual void EnableAppList() OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<AppListServiceMac>;

  AppListServiceMac() {}

  scoped_nsobject<AppListWindowController> window_controller_;

  DISALLOW_COPY_AND_ASSIGN(AppListServiceMac);
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
  AppListServiceMac::GetInstance()->DismissAppList();
}

gfx::NativeWindow AppListControllerDelegateCocoa::GetAppListWindow() {
  return AppListServiceMac::GetInstance()->GetNativeWindow();
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

void AppListServiceMac::CreateAppList(Profile* requested_profile) {
  if (profile() == requested_profile)
    return;

  SetProfile(requested_profile);
  scoped_ptr<app_list::AppListViewDelegate> delegate(
      new AppListViewDelegate(new AppListControllerDelegateCocoa(), profile()));
  window_controller_.reset([[AppListWindowController alloc] init]);
  [[window_controller_ appListViewController] setDelegate:delegate.Pass()];
}

void AppListServiceMac::ShowAppList(Profile* requested_profile) {
  InvalidatePendingProfileLoads();

  if (IsAppListVisible() && (requested_profile == profile())) {
    DCHECK(window_controller_);
    [[window_controller_ window] makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
    return;
  }

  SaveProfilePathToLocalState(requested_profile->GetPath());

  DismissAppList();
  CreateAppList(requested_profile);

  DCHECK(window_controller_);
  [[window_controller_ window] makeKeyAndOrderFront:nil];
  [NSApp activateIgnoringOtherApps:YES];
}

void AppListServiceMac::DismissAppList() {
  if (!window_controller_)
    return;

  [[window_controller_ window] close];
}

bool AppListServiceMac::IsAppListVisible() const {
  return [[window_controller_ window] isVisible];
}

void AppListServiceMac::EnableAppList() {
  // TODO(tapted): Implement enable logic here for OSX.
}

NSWindow* AppListServiceMac::GetNativeWindow() {
  return [window_controller_ window];
}

}  // namespace

// static
AppListService* AppListService::Get() {
  return AppListServiceMac::GetInstance();
}

// static
void AppListService::InitAll(Profile* initial_profile) {
  Get()->Init(initial_profile);
}
