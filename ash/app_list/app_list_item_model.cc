// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_item_model.h"

#include "ash/app_list/app_list_item_model_observer.h"

namespace ash {

AppListItemModel::AppListItemModel() {
}

AppListItemModel::~AppListItemModel() {
}

void AppListItemModel::SetIcon(const SkBitmap& icon) {
  icon_ = icon;
  FOR_EACH_OBSERVER(AppListItemModelObserver, observers_,
                    ItemIconChanged());
}

void AppListItemModel::SetTitle(const std::string& title) {
  title_ = title;
  FOR_EACH_OBSERVER(AppListItemModelObserver, observers_,
                    ItemTitleChanged());
}

void AppListItemModel::AddObserver(AppListItemModelObserver* observer) {
  observers_.AddObserver(observer);
}

void AppListItemModel::RemoveObserver(AppListItemModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash
