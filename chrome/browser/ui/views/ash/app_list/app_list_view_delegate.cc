// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/app_list/app_list_view_delegate.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/ash/app_list/app_list_model_builder.h"
#include "chrome/browser/ui/views/ash/app_list/chrome_app_list_item.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/system/statistics_provider.h"
#endif

AppListViewDelegate::AppListViewDelegate() {
}

AppListViewDelegate::~AppListViewDelegate() {
}

bool AppListViewDelegate::ShouldGenerateIconShadow() {
#if defined(OS_CHROMEOS)
  static bool use_shadow = true;
  static bool configured = false;

  if (!configured) {
    std::string board;
    chromeos::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
        "CHROMEOS_RELEASE_BOARD", &board);

    // Boards that should not have shadow.
    const char* kBoards[] = { "x86-mario", "x86-alex", "x86-zgb" };
    for (size_t i = 0; i < arraysize(kBoards); ++i) {
      if (board == kBoards[i]) {
        use_shadow = false;
        break;
      }
    }

    configured = true;
  }

  return use_shadow;
#else
  return true;
#endif
}

void AppListViewDelegate::SetModel(ash::AppListModel* model) {
  if (model) {
    if (!model_builder_.get()) {
      model_builder_.reset(
          new AppListModelBuilder(
              ProfileManager::GetDefaultProfileOrOffTheRecord()));
    }
    model_builder_->SetModel(model);
  } else {
    model_builder_.reset();
  }
}

void AppListViewDelegate::UpdateModel(const std::string& query) {
  DCHECK(model_builder_.get());
  model_builder_->Build(query);
}

void AppListViewDelegate::OnAppListItemActivated(
    ash::AppListItemModel* item,
    int event_flags) {
  static_cast<ChromeAppListItem*>(item)->Activate(event_flags);
}
