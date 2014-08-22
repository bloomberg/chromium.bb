// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/app_activity_proxy.h"

#include "athena/content/app_activity_registry.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace athena {

AppActivityProxy::AppActivityProxy(ActivityViewModel* view_model,
                                   AppActivityRegistry* creator) :
    app_activity_registry_(creator),
    title_(view_model->GetTitle()),
    image_(view_model->GetOverviewModeImage()),
    color_(view_model->GetRepresentativeColor()),
    // TODO(skuhne): We probably need to do something better with the view
    // (e.g. showing the image).
    view_(new views::View()) {}

AppActivityProxy::~AppActivityProxy() {
  app_activity_registry_->ProxyDestroyed(this);
}

ActivityViewModel* AppActivityProxy::GetActivityViewModel() {
  return this;
}

void AppActivityProxy::SetCurrentState(ActivityState state) {
  // We ignore all calls which try to re-load the application at a lower than
  // running invisible state.
  if (state != ACTIVITY_VISIBLE && state != ACTIVITY_INVISIBLE)
    return;
  app_activity_registry_->RestartApplication(this);
  // Note: This object is now destroyed.
}

Activity::ActivityState AppActivityProxy::GetCurrentState() {
  return ACTIVITY_UNLOADED;
}

bool AppActivityProxy::IsVisible() {
  return true;
}

Activity::ActivityMediaState AppActivityProxy::GetMediaState() {
  // This proxy has never any media playing.
  return ACTIVITY_MEDIA_STATE_NONE;
}

aura::Window* AppActivityProxy::GetWindow() {
  return view_->GetWidget()->GetNativeWindow();
}

void AppActivityProxy::Init() {
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

}  // namespace athena
