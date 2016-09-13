// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/app_list_presenter_mus.h"

#include "services/shell/public/cpp/connector.h"

namespace ash {

namespace {

bool HasConnection(app_list::mojom::AppListPresenterPtr* interface_ptr) {
  return interface_ptr->is_bound() && !interface_ptr->encountered_error();
}

}  // namespace

AppListPresenterMus::AppListPresenterMus(::shell::Connector* connector)
    : connector_(connector) {}

AppListPresenterMus::~AppListPresenterMus() {}

void AppListPresenterMus::Show(int64_t display_id) {
  ConnectIfNeeded();
  presenter_->Show(display_id);
}

void AppListPresenterMus::Dismiss() {
  ConnectIfNeeded();
  presenter_->Dismiss();
}

void AppListPresenterMus::ToggleAppList(int64_t display_id) {
  ConnectIfNeeded();
  presenter_->ToggleAppList(display_id);
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

void AppListPresenterMus::ConnectIfNeeded() {
  if (!connector_ || HasConnection(&presenter_))
    return;
  connector_->ConnectToInterface("exe:chrome", &presenter_);
  CHECK(HasConnection(&presenter_))
      << "Could not connect to app_list::mojom::AppListPresenter.";
}

}  // namespace ash
