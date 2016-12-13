// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/app_list_presenter_mus.h"

#include "ash/common/wm_shell.h"
#include "ui/app_list/presenter/app_list.h"

namespace ash {

AppListPresenterMus::AppListPresenterMus() {}

AppListPresenterMus::~AppListPresenterMus() {}

void AppListPresenterMus::Show(int64_t display_id) {
  app_list::mojom::AppListPresenter* app_list_presenter =
      WmShell::Get()->app_list()->GetAppListPresenter();
  if (app_list_presenter)
    app_list_presenter->Show(display_id);
}

void AppListPresenterMus::Dismiss() {
  app_list::mojom::AppListPresenter* app_list_presenter =
      WmShell::Get()->app_list()->GetAppListPresenter();
  if (app_list_presenter)
    app_list_presenter->Dismiss();
}

void AppListPresenterMus::ToggleAppList(int64_t display_id) {
  app_list::mojom::AppListPresenter* app_list_presenter =
      WmShell::Get()->app_list()->GetAppListPresenter();
  if (app_list_presenter)
    app_list_presenter->ToggleAppList(display_id);
}

bool AppListPresenterMus::IsVisible() const {
  return false;
}

bool AppListPresenterMus::GetTargetVisibility() const {
  // TODO(mfomitchev): we have GetTargetVisibility() in mojom, but this
  // shouldn't be a synchronous method. We should go through the call sites and
  // either teach them to use a callback, or perhaps use a visibility observer.
  //  NOTIMPLEMENTED();
  return false;
}

}  // namespace ash
