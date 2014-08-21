// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/app_activity.h"

#include "athena/activity/public/activity_manager.h"
#include "athena/content/app_activity_registry.h"
#include "athena/content/public/app_content_control_delegate.h"
#include "athena/content/public/app_registry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/shell/browser/shell_app_window.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

namespace athena {

// TODO(mukai): specifies the same accelerators of WebActivity.
AppActivity::AppActivity(extensions::ShellAppWindow* app_window)
    : app_window_(app_window),
      web_view_(NULL),
      current_state_(ACTIVITY_UNLOADED),
      app_activity_registry_(NULL) {
}

AppActivity::~AppActivity() {
  // If this activity is registered, we unregister it now.
  if (app_activity_registry_)
    app_activity_registry_->UnregisterAppActivity(this);
}

ActivityViewModel* AppActivity::GetActivityViewModel() {
  return this;
}

void AppActivity::SetCurrentState(Activity::ActivityState state) {
  ActivityState current_state = state;
  // Remember the last requested state now so that a call to GetCurrentState()
  // returns the new state.
  current_state_ = state;

  switch (state) {
    case ACTIVITY_VISIBLE:
      // Fall through (for the moment).
    case ACTIVITY_INVISIBLE:
      // By clearing the overview mode image we allow the content to be shown.
      overview_mode_image_ = gfx::ImageSkia();
      // Note: A reload from the unloaded state will be performed through the
      // |AppActivityProxy| object and no further action isn't necessary here.
      break;
    case ACTIVITY_BACKGROUND_LOW_PRIORITY:
      DCHECK(ACTIVITY_VISIBLE == current_state ||
             ACTIVITY_INVISIBLE == current_state);
      // TODO(skuhne): Do this.
      break;
    case ACTIVITY_PERSISTENT:
      DCHECK_EQ(ACTIVITY_BACKGROUND_LOW_PRIORITY, current_state);
      // TODO(skuhne): Do this.
      break;
    case ACTIVITY_UNLOADED:
      DCHECK_NE(ACTIVITY_UNLOADED, current_state);
      // This will cause the application to shut down, close its windows and
      // delete this object. Instead a |AppActivityProxy| will be created as
      // place holder.
      if (app_activity_registry_)
        app_activity_registry_->Unload();
      break;
  }
}

Activity::ActivityState AppActivity::GetCurrentState() {
  if (!web_view_) {
    DCHECK_EQ(ACTIVITY_UNLOADED, current_state_);
    return ACTIVITY_UNLOADED;
  }
  // TODO(skuhne): This should be controlled by an observer and should not
  // reside here.
  if (IsVisible() && current_state_ != ACTIVITY_VISIBLE)
    SetCurrentState(ACTIVITY_VISIBLE);
  // Note: If the activity is not visible it does not necessarily mean that it
  // does not have GPU compositor resources (yet).
  return current_state_;
}

bool AppActivity::IsVisible() {
  return web_view_ && web_view_->IsDrawn();
}

Activity::ActivityMediaState AppActivity::GetMediaState() {
  // TODO(skuhne): The function GetTabMediaStateForContents(WebContents),
  // and the AudioStreamMonitor needs to be moved from Chrome into contents to
  // make it more modular and so that we can use it from here.
  return Activity::ACTIVITY_MEDIA_STATE_NONE;
}

aura::Window* AppActivity::GetWindow() {
  return !web_view_ ? NULL : web_view_->GetWidget()->GetNativeWindow();
}

void AppActivity::Init() {
}

SkColor AppActivity::GetRepresentativeColor() const {
  // TODO(sad): Compute the color from the favicon.
  return SK_ColorGRAY;
}

base::string16 AppActivity::GetTitle() const {
  return web_view_->GetWebContents()->GetTitle();
}

bool AppActivity::UsesFrame() const {
  return false;
}

views::View* AppActivity::GetContentsView() {
  if (!web_view_) {
    // TODO(oshima): use apps::NativeAppWindowViews
    content::WebContents* web_contents =
        app_window_->GetAssociatedWebContents();
    web_view_ = new views::WebView(web_contents->GetBrowserContext());
    web_view_->SetWebContents(web_contents);
    SetCurrentState(ACTIVITY_INVISIBLE);
    Observe(web_contents);
    overview_mode_image_ = gfx::ImageSkia();
  }
  return web_view_;
}

void AppActivity::CreateOverviewModeImage() {
  // TODO(skuhne): Implement this!
}

gfx::ImageSkia AppActivity::GetOverviewModeImage() {
  return overview_mode_image_;
}

void AppActivity::TitleWasSet(content::NavigationEntry* entry,
                              bool explicit_set) {
  ActivityManager::Get()->UpdateActivity(this);
}

void AppActivity::DidUpdateFaviconURL(
    const std::vector<content::FaviconURL>& candidates) {
  ActivityManager::Get()->UpdateActivity(this);
}

void AppActivity::DidStartNavigationToPendingEntry(
      const GURL& url,
      content::NavigationController::ReloadType reload_type) {
  if (!app_activity_registry_)
    RegisterActivity();
}

// Register an |activity| with an application.
// Note: This should only get called once for an |app_window| of the
// |activity|.
void AppActivity::RegisterActivity() {
  content::WebContents* web_contents = app_window_->GetAssociatedWebContents();
  AppRegistry* app_registry = AppRegistry::Get();
  // Get the application's registry.
  app_activity_registry_ = app_registry->GetAppActivityRegistry(
          app_registry->GetDelegate()->GetApplicationID(web_contents),
          web_contents->GetBrowserContext());
  DCHECK(app_activity_registry_);
  // Register the activity.
  app_activity_registry_->RegisterAppActivity(this);
}

}  // namespace athena
