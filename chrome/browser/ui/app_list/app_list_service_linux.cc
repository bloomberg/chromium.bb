// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_service_linux.h"

#include "base/memory/singleton.h"

AppListServiceLinux::~AppListServiceLinux() {}

// static
AppListServiceLinux* AppListServiceLinux::GetInstance() {
  return Singleton<AppListServiceLinux,
                   LeakySingletonTraits<AppListServiceLinux> >::get();
}

void AppListServiceLinux::Init(Profile* initial_profile) {
  HandleCommandLineFlags(initial_profile);
}

void AppListServiceLinux::CreateForProfile(Profile* requested_profile) {
  NOTIMPLEMENTED();
}

void AppListServiceLinux::ShowForProfile(Profile* requested_profile) {
  NOTIMPLEMENTED();
}

void AppListServiceLinux::DismissAppList() {
  NOTIMPLEMENTED();
}

bool AppListServiceLinux::IsAppListVisible() const {
  NOTIMPLEMENTED();
  return false;
}

gfx::NativeWindow AppListServiceLinux::GetAppListWindow() {
  NOTIMPLEMENTED();
  return gfx::NativeWindow();
}

Profile* AppListServiceLinux::GetCurrentAppListProfile() {
  NOTIMPLEMENTED();
  return NULL;
}

AppListControllerDelegate* AppListServiceLinux::CreateControllerDelegate() {
  NOTIMPLEMENTED();
  return NULL;
}

void AppListServiceLinux::CreateShortcut() {
  NOTIMPLEMENTED();
}

AppListServiceLinux::AppListServiceLinux() {}

// static
AppListService* AppListService::Get(chrome::HostDesktopType desktop_type) {
  return AppListServiceLinux::GetInstance();
}

// static
void AppListService::InitAll(Profile* initial_profile) {
  AppListServiceLinux::GetInstance()->Init(initial_profile);
}
