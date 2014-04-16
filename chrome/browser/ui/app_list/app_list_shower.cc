// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/ui/app_list/app_list_shower.h"
#include "chrome/browser/ui/app_list/scoped_keep_alive.h"

AppListShower::AppListShower(scoped_ptr<AppListFactory> factory,
                             AppListService* service)
    : factory_(factory.Pass()),
      service_(service),
      profile_(NULL),
      can_close_app_list_(true) {
}

AppListShower::~AppListShower() {
}

void AppListShower::ShowForProfile(Profile* requested_profile) {
  // If the app list is already displaying |profile| just activate it (in case
  // we have lost focus).
  if (IsAppListVisible() && (requested_profile == profile_)) {
    app_list_->Show();
    return;
  }

  if (!app_list_) {
    CreateViewForProfile(requested_profile);
  } else if (requested_profile != profile_) {
    profile_ = requested_profile;
    app_list_->SetProfile(requested_profile);
  }

  keep_alive_.reset(new ScopedKeepAlive);
  if (!IsAppListVisible())
    app_list_->MoveNearCursor();
  app_list_->Show();
}

gfx::NativeWindow AppListShower::GetWindow() {
  if (!IsAppListVisible())
    return NULL;
  return app_list_->GetWindow();
}

void AppListShower::CreateViewForProfile(Profile* requested_profile) {
  profile_ = requested_profile;
  app_list_.reset(factory_->CreateAppList(
      profile_,
      service_,
      base::Bind(&AppListShower::DismissAppList, base::Unretained(this))));
}

void AppListShower::DismissAppList() {
  if (app_list_ && can_close_app_list_) {
    app_list_->Hide();
    keep_alive_.reset();
  }
}

void AppListShower::CloseAppList() {
  app_list_.reset();
  profile_ = NULL;
  can_close_app_list_ = true;

  // We may end up here as the result of the OS deleting the AppList's
  // widget (WidgetObserver::OnWidgetDestroyed). If this happens and there
  // are no browsers around then deleting the keep alive will result in
  // deleting the Widget again (by way of CloseAllSecondaryWidgets). When
  // the stack unravels we end up back in the Widget that was deleted and
  // crash. By delaying deletion of the keep alive we ensure the Widget has
  // correctly been destroyed before ending the keep alive so that
  // CloseAllSecondaryWidgets() won't attempt to delete the AppList's Widget
  // again.
  if (base::MessageLoop::current()) {  // NULL in tests.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&AppListShower::ResetKeepAlive, base::Unretained(this)));
    return;
  }
  keep_alive_.reset();
}

bool AppListShower::IsAppListVisible() const {
  return app_list_ && app_list_->IsVisible();
}

void AppListShower::WarmupForProfile(Profile* profile) {
  DCHECK(!profile_);
  CreateViewForProfile(profile);
  app_list_->Prerender();
}

bool AppListShower::HasView() const {
  return !!app_list_;
}

void AppListShower::ResetKeepAlive() {
  keep_alive_.reset();
}
