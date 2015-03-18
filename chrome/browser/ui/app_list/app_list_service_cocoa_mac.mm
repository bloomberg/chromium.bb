// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_service_cocoa_mac.h"

#include "base/memory/singleton.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate_impl.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
#import "ui/app_list/cocoa/app_list_view_controller.h"
#import "ui/app_list/cocoa/app_list_window_controller.h"

AppListServiceCocoaMac::~AppListServiceCocoaMac() {
}

// static
AppListServiceCocoaMac* AppListServiceCocoaMac::GetInstance() {
  return Singleton<AppListServiceCocoaMac,
                   LeakySingletonTraits<AppListServiceCocoaMac>>::get();
}

void AppListServiceCocoaMac::ShowForProfile(Profile* requested_profile) {
  CreateForProfile(requested_profile);
  DCHECK(ReadyToShow());
  ShowWindowNearDock();
}

Profile* AppListServiceCocoaMac::GetCurrentAppListProfile() {
  return profile_;
}

AppListControllerDelegate* AppListServiceCocoaMac::GetControllerDelegate() {
  return controller_delegate_.get();
}

void AppListServiceCocoaMac::CreateForProfile(Profile* requested_profile) {
  DCHECK(requested_profile);
  InvalidatePendingProfileLoads();
  if (profile_ && requested_profile->IsSameProfile(profile_))
    return;

  profile_ = requested_profile->GetOriginalProfile();
  SetProfilePath(profile_->GetPath());

  if (!window_controller_)
    window_controller_.reset([[AppListWindowController alloc] init]);

  [[window_controller_ appListViewController] setDelegate:nil];
  [[window_controller_ appListViewController]
      setDelegate:GetViewDelegate(profile_)];
}

void AppListServiceCocoaMac::DestroyAppList() {
  // Due to reference counting, Mac can't guarantee that the widget is deleted,
  // but mac supports a visible app list with a NULL profile, so there's also no
  // need to tear it down completely.
  DismissAppList();
  [[window_controller_ appListViewController] setDelegate:NULL];

  profile_ = NULL;
}

NSWindow* AppListServiceCocoaMac::GetNativeWindow() const {
  return [window_controller_ window];
}

bool AppListServiceCocoaMac::ReadyToShow() {
  if (!window_controller_) {
    // Note that this will start showing an unpopulated window, the caller needs
    // to ensure it will be populated later.
    window_controller_.reset([[AppListWindowController alloc] init]);
  }
  return true;  // Cocoa app list can be shown empty.
}

AppListServiceCocoaMac::AppListServiceCocoaMac()
    : profile_(nullptr),
      controller_delegate_(new AppListControllerDelegateImpl(this)) {
}
