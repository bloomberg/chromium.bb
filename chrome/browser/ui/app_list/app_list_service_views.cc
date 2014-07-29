// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_service_views.h"

#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/scoped_keep_alive.h"

AppListServiceViews::AppListServiceViews(
    scoped_ptr<AppListControllerDelegate> controller_delegate)
    : shower_(this),
      can_dismiss_(true),
      controller_delegate_(controller_delegate.Pass()) {
}

AppListServiceViews::~AppListServiceViews() {}

void AppListServiceViews::OnViewBeingDestroyed() {
  can_dismiss_ = true;
  shower_.HandleViewBeingDestroyed();
}

void AppListServiceViews::Init(Profile* initial_profile) {
  PerformStartupChecks(initial_profile);
}

void AppListServiceViews::CreateForProfile(Profile* requested_profile) {
  shower_.CreateViewForProfile(requested_profile);
}

void AppListServiceViews::ShowForProfile(Profile* requested_profile) {
  DCHECK(requested_profile);

  ScopedKeepAlive keep_alive;

  InvalidatePendingProfileLoads();
  SetProfilePath(requested_profile->GetPath());
  shower_.ShowForProfile(requested_profile);
  RecordAppListLaunch();
}

void AppListServiceViews::DismissAppList() {
  if (!can_dismiss_)
    return;

  shower_.DismissAppList();
}

bool AppListServiceViews::IsAppListVisible() const {
  return shower_.IsAppListVisible();
}

gfx::NativeWindow AppListServiceViews::GetAppListWindow() {
  return shower_.GetWindow();
}

Profile* AppListServiceViews::GetCurrentAppListProfile() {
  return shower_.profile();
}

AppListControllerDelegate* AppListServiceViews::GetControllerDelegate() {
  return controller_delegate_.get();
}

AppListControllerDelegate*
AppListServiceViews::GetControllerDelegateForCreate() {
  return controller_delegate_.get();
}
