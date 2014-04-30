// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_controller_delegate_views.h"

#include "chrome/browser/ui/app_list/app_list_service_views.h"

AppListControllerDelegateViews::AppListControllerDelegateViews(
    AppListServiceViews* service)
    : AppListControllerDelegateImpl(service),
      service_(service) {
}

AppListControllerDelegateViews::~AppListControllerDelegateViews() {}

void AppListControllerDelegateViews::ViewClosing() {
  service_->OnViewBeingDestroyed();
}

void AppListControllerDelegateViews::OnShowExtensionPrompt() {
  service_->set_can_dismiss(false);
}

void AppListControllerDelegateViews::OnCloseExtensionPrompt() {
  service_->set_can_dismiss(true);
}

bool AppListControllerDelegateViews::CanDoCreateShortcutsFlow() {
  return true;
}
