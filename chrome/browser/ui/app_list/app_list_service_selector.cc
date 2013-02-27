// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/host_desktop.h"

#if defined(ENABLE_APP_LIST) && defined(USE_ASH)
#include "chrome/browser/ui/app_list/app_list_service_ash.h"
#endif

#if defined(ENABLE_APP_LIST) && defined(OS_WIN)
#include "chrome/browser/ui/app_list/app_list_service_win.h"
#endif

#if defined(ENABLE_APP_LIST) && defined(OS_MACOSX)
#include "chrome/browser/ui/app_list/app_list_service_mac.h"
#endif

#if !defined(ENABLE_APP_LIST)
#include "chrome/browser/ui/app_list/app_list_service_disabled.h"
#endif

// static
AppListService* AppListService::Get() {
#if defined(ENABLE_APP_LIST) && defined(USE_ASH) && defined(OS_WIN)
  if (chrome::GetActiveDesktop() == chrome::HOST_DESKTOP_TYPE_ASH)
    return chrome::GetAppListServiceAsh();
#endif

#if !defined(ENABLE_APP_LIST)
  return chrome::GetAppListServiceDisabled();
#elif defined(OS_WIN)
  return chrome::GetAppListServiceWin();
#elif defined(OS_MACOSX)
  return chrome::GetAppListServiceMac();
#elif defined(OS_CHROMEOS)
  return chrome::GetAppListServiceAsh();
#else
#error "ENABLE_APP_LIST defined, but no AppListService available"
#endif
}

// static
void AppListService::InitAll(Profile* initial_profile) {
#if defined(ENABLE_APP_LIST) && defined(USE_ASH) && defined(OS_WIN)
  chrome::GetAppListServiceAsh()->Init(initial_profile);
  chrome::GetAppListServiceWin()->Init(initial_profile);
#else
  Get()->Init(initial_profile);
#endif
}
