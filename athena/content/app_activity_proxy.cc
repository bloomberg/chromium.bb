// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/app_activity_proxy.h"

#include "athena/content/app_activity.h"
#include "athena/content/app_activity_registry.h"
#include "athena/wm/public/window_list_provider.h"
#include "athena/wm/public/window_manager.h"
#include "ui/aura/window.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace athena {

AppActivityProxy::AppActivityProxy(AppActivity* replaced_activity,
                                   AppActivityRegistry* creator) :
    app_activity_registry_(creator),
    title_(replaced_activity->GetActivityViewModel()->GetTitle()),
    color_(replaced_activity->GetActivityViewModel()->GetRepresentativeColor()),
    replaced_activity_(replaced_activity),
    view_(new views::View()),
    restart_called_(false) {
}

ActivityViewModel* AppActivityProxy::GetActivityViewModel() {
  return this;
}

void AppActivityProxy::SetCurrentState(ActivityState state) {
  // We only restart the application when we are switching to visible, and only
  // once.
  if (state != ACTIVITY_VISIBLE || restart_called_)
    return;
  restart_called_ = true;
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
  return view_->GetWidget() ? view_->GetWidget()->GetNativeWindow() : nullptr;
}

content::WebContents* AppActivityProxy::GetWebContents() {
  return nullptr;
}

void AppActivityProxy::Init() {
  DCHECK(replaced_activity_);
  // Get the content proxy to present the content.
  content_proxy_ = replaced_activity_->GetContentProxy();
  WindowListProvider* window_list_provider =
      WindowManager::Get()->GetWindowListProvider();
  window_list_provider->StackWindowBehindTo(GetWindow(),
                                            replaced_activity_->GetWindow());
  // After the Init() function returns, the passed |replaced_activity_| might
  // get destroyed. Since we do not need it anymore we reset it.
  replaced_activity_ = nullptr;
}

SkColor AppActivityProxy::GetRepresentativeColor() const {
  return color_;
}

base::string16 AppActivityProxy::GetTitle() const {
  return title_;
}

gfx::ImageSkia AppActivityProxy::GetIcon() const {
  return gfx::ImageSkia();
}

bool AppActivityProxy::UsesFrame() const {
  return true;
}

views::View* AppActivityProxy::GetContentsView() {
  return view_;
}

gfx::ImageSkia AppActivityProxy::GetOverviewModeImage() {
  return content_proxy_->GetContentImage();
}

void AppActivityProxy::PrepareContentsForOverview() {
}

void AppActivityProxy::ResetContentsView() {
}

AppActivityProxy::~AppActivityProxy() {
  app_activity_registry_->ProxyDestroyed(this);
}

}  // namespace athena
