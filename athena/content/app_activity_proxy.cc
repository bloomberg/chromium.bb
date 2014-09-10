// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/app_activity_proxy.h"

#include "athena/content/app_activity_registry.h"
#include "athena/wm/public/window_list_provider.h"
#include "athena/wm/public/window_manager.h"
#include "ui/aura/window.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace athena {

AppActivityProxy::AppActivityProxy(Activity* replaced_activity,
                                   AppActivityRegistry* creator) :
    app_activity_registry_(creator),
    title_(replaced_activity->GetActivityViewModel()->GetTitle()),
    image_(replaced_activity->GetActivityViewModel()->GetOverviewModeImage()),
    color_(replaced_activity->GetActivityViewModel()->GetRepresentativeColor()),
    replaced_activity_(replaced_activity),
    // TODO(skuhne): We probably need to do something better with the view
    // (e.g. showing the passed image / layer).
    view_(new views::View()) {
}

AppActivityProxy::~AppActivityProxy() {
  app_activity_registry_->ProxyDestroyed(this);
}

ActivityViewModel* AppActivityProxy::GetActivityViewModel() {
  return this;
}

void AppActivityProxy::SetCurrentState(ActivityState state) {
  // We only restart the application when we are switching to visible.
  if (state != ACTIVITY_VISIBLE)
    return;
  app_activity_registry_->RestartApplication(this);
  // Note: This object is now destroyed.
}

Activity::ActivityState AppActivityProxy::GetCurrentState() {
  return ACTIVITY_UNLOADED;
}

bool AppActivityProxy::IsVisible() {
  return false;
}

Activity::ActivityMediaState AppActivityProxy::GetMediaState() {
  // This proxy has never any media playing.
  return ACTIVITY_MEDIA_STATE_NONE;
}

aura::Window* AppActivityProxy::GetWindow() {
  return view_->GetWidget()->GetNativeWindow();
}

void AppActivityProxy::Init() {
  DCHECK(replaced_activity_);
  WindowListProvider* window_list_provider =
      WindowManager::GetInstance()->GetWindowListProvider();
  window_list_provider->StackWindowBehindTo(GetWindow(),
                                            replaced_activity_->GetWindow());
  // We moved.
  replaced_activity_ = NULL;
}

SkColor AppActivityProxy::GetRepresentativeColor() const {
  return color_;
}

base::string16 AppActivityProxy::GetTitle() const {
  return title_;
}

bool AppActivityProxy::UsesFrame() const {
  return true;
}

views::View* AppActivityProxy::GetContentsView() {
  return view_;
}

void AppActivityProxy::CreateOverviewModeImage() {
  // Nothing we can do here.
}

gfx::ImageSkia AppActivityProxy::GetOverviewModeImage() {
  return image_;
}

void AppActivityProxy::PrepareContentsForOverview() {
}

void AppActivityProxy::ResetContentsView() {
}

}  // namespace athena
