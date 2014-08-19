// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_shower_views.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_shower_delegate.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/app_list/scoped_keep_alive.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/screen.h"

AppListShower::AppListShower(AppListShowerDelegate* delegate)
    : delegate_(delegate),
      profile_(NULL),
      app_list_(NULL),
      window_icon_updated_(false) {
}

AppListShower::~AppListShower() {
}

void AppListShower::ShowForProfile(Profile* requested_profile) {
  // If the app list is already displaying |profile| just activate it (in case
  // we have lost focus).
  if (IsAppListVisible() && (requested_profile == profile_)) {
    Show();
    return;
  }

  if (!HasView()) {
    CreateViewForProfile(requested_profile);
  } else if (requested_profile != profile_) {
    profile_ = requested_profile;
    UpdateViewForNewProfile();
  }

  keep_alive_.reset(new ScopedKeepAlive);
  if (!IsAppListVisible())
    delegate_->MoveNearCursor(app_list_);
  Show();
}

gfx::NativeWindow AppListShower::GetWindow() {
  if (!IsAppListVisible())
    return NULL;
  return app_list_->GetWidget()->GetNativeWindow();
}

void AppListShower::CreateViewForProfile(Profile* requested_profile) {
  profile_ = requested_profile;
  app_list_ = MakeViewForCurrentProfile();
  delegate_->OnViewCreated();
}

void AppListShower::DismissAppList() {
  if (HasView()) {
    Hide();
    delegate_->OnViewDismissed();
    // This can be reached by pressing the dismiss accelerator. To prevent
    // events from being processed with a destroyed dispatcher, delay the reset
    // of the keep alive.
    ResetKeepAliveSoon();
  }
}

void AppListShower::HandleViewBeingDestroyed() {
  app_list_ = NULL;
  profile_ = NULL;

  // We may end up here as the result of the OS deleting the AppList's
  // widget (WidgetObserver::OnWidgetDestroyed). If this happens and there
  // are no browsers around then deleting the keep alive will result in
  // deleting the Widget again (by way of CloseAllSecondaryWidgets). When
  // the stack unravels we end up back in the Widget that was deleted and
  // crash. By delaying deletion of the keep alive we ensure the Widget has
  // correctly been destroyed before ending the keep alive so that
  // CloseAllSecondaryWidgets() won't attempt to delete the AppList's Widget
  // again.
  ResetKeepAliveSoon();
}

bool AppListShower::IsAppListVisible() const {
  return app_list_ && app_list_->GetWidget()->IsVisible();
}

void AppListShower::WarmupForProfile(Profile* profile) {
  DCHECK(!profile_);
  CreateViewForProfile(profile);
  app_list_->Prerender();
}

bool AppListShower::HasView() const {
  return !!app_list_;
}

app_list::AppListView* AppListShower::MakeViewForCurrentProfile() {
  // The view delegate will be owned by the app list view. The app list view
  // manages its own lifetime.
  AppListViewDelegate* view_delegate = new AppListViewDelegate(
      profile_, delegate_->GetControllerDelegateForCreate());
  app_list::AppListView* view = new app_list::AppListView(view_delegate);
  gfx::Point cursor = gfx::Screen::GetNativeScreen()->GetCursorScreenPoint();
  view->InitAsBubbleAtFixedLocation(NULL,
                                    0,
                                    cursor,
                                    views::BubbleBorder::FLOAT,
                                    false /* border_accepts_events */);
  return view;
}

void AppListShower::UpdateViewForNewProfile() {
  app_list_->SetProfileByPath(profile_->GetPath());
}

void AppListShower::Show() {
  app_list_->GetWidget()->Show();
  if (!window_icon_updated_) {
    app_list_->GetWidget()->GetTopLevelWidget()->UpdateWindowIcon();
    window_icon_updated_ = true;
  }
  app_list_->GetWidget()->Activate();
}

void AppListShower::Hide() {
  app_list_->GetWidget()->Hide();
}

void AppListShower::ResetKeepAliveSoon() {
  if (base::MessageLoop::current()) {  // NULL in tests.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&AppListShower::ResetKeepAlive, base::Unretained(this)));
    return;
  }
  ResetKeepAlive();
}

void AppListShower::ResetKeepAlive() {
  keep_alive_.reset();
}
