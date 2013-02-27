// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_service_disabled.h"

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "chrome/browser/ui/app_list/app_list_service.h"

namespace {

class AppListServiceDisabled : public AppListService {
 public:
  static AppListServiceDisabled* GetInstance() {
    return Singleton<AppListServiceDisabled,
                     LeakySingletonTraits<AppListServiceDisabled> >::get();
  }

 private:
  friend struct DefaultSingletonTraits<AppListServiceDisabled>;

  AppListServiceDisabled() {}

  DISALLOW_COPY_AND_ASSIGN(AppListServiceDisabled);
};

}  // namespace

namespace chrome {

AppListService* GetAppListServiceDisabled() {
  return AppListServiceDisabled::GetInstance();
}

}  // namespace chrome
