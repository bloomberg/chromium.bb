// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_controller_delegate_views.h"

#include "chrome/browser/ui/app_list/app_list_service_views.h"
#include "ui/app_list/views/app_list_view.h"

AppListControllerDelegateViews::AppListControllerDelegateViews(
    AppListServiceViews* service)
    : AppListControllerDelegateImpl(service),
      service_(service) {
}

AppListControllerDelegateViews::~AppListControllerDelegateViews() {}

gfx::Rect AppListControllerDelegateViews::GetAppListBounds() {
  // We use the bounds of the app list view here because the bounds of the app
  // list window include the shadow behind it (and the shadow size varies across
  // platforms).
  app_list::AppListView* app_list_view = service_->shower().app_list();
  if (app_list_view)
    return app_list_view->GetBoundsInScreen();
  return gfx::Rect();
}

void AppListControllerDelegateViews::ViewClosing() {
  service_->OnViewBeingDestroyed();
}

void AppListControllerDelegateViews::OnShowChildDialog() {
  DCHECK(service_->shower().app_list());
  service_->shower().app_list()->SetAppListOverlayVisible(true);
  service_->set_can_dismiss(false);
}

void AppListControllerDelegateViews::OnCloseChildDialog() {
  DCHECK(service_->shower().app_list());
  service_->shower().app_list()->SetAppListOverlayVisible(false);
  service_->set_can_dismiss(true);
}

bool AppListControllerDelegateViews::CanDoCreateShortcutsFlow() {
  return true;
}
