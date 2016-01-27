// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_shower_views.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/profiler/scoped_tracker.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/apps/scoped_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_shower_delegate.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
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

void AppListShower::ShowForCurrentProfile() {
  DCHECK(HasView());
  keep_alive_.reset(new ScopedKeepAlive);

  // If the app list is already displaying |profile| just activate it (in case
  // we have lost focus).
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
  DCHECK(requested_profile);
  if (HasView() && requested_profile->IsSameProfile(profile_))
    return;

  profile_ = requested_profile->GetOriginalProfile();
  if (HasView()) {
    UpdateViewForNewProfile();
    return;
  }
  app_list_ = MakeViewForCurrentProfile();

  // TODO(tapted): Remove ScopedTracker below once crbug.com/431326 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "431326 AppListShowerDelegate::OnViewCreated()"));

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
  app_list::AppListView* view;
  {
    // TODO(tapted): Remove ScopedTracker below once crbug.com/431326 is fixed.
    tracked_objects::ScopedTracker tracking_profile1(
        FROM_HERE_WITH_EXPLICIT_FUNCTION("431326 AppListView()"));

    // The app list view manages its own lifetime.
    view = new app_list::AppListView(delegate_->GetViewDelegateForCreate());
  }

  gfx::Point cursor = gfx::Screen::GetScreen()->GetCursorScreenPoint();
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
  if (base::ThreadTaskRunnerHandle::IsSet()) {  // Not set in tests.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&AppListShower::ResetKeepAlive, base::Unretained(this)));
    return;
  }
  ResetKeepAlive();
}

void AppListShower::ResetKeepAlive() {
  keep_alive_.reset();
}
