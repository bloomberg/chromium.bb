// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/aura/app_list/app_list_view_delegate.h"

#include "chrome/browser/ui/views/aura/app_list/chrome_app_list_item.h"

AppListViewDelegate::AppListViewDelegate() {
}

AppListViewDelegate::~AppListViewDelegate() {
}

void AppListViewDelegate::OnAppListItemActivated(
    aura_shell::AppListItemModel* item,
    int event_flags) {
  static_cast<ChromeAppListItem*>(item)->Activate(event_flags);
}
