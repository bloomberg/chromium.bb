// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/app_activity.h"

#include "athena/activity/public/activity_manager.h"
#include "athena/content/app_activity_registry.h"
#include "athena/content/public/app_registry.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

namespace athena {

// TODO(mukai): specifies the same accelerators of WebActivity.
AppActivity::AppActivity(const std::string& app_id)
    : app_id_(app_id),
      web_view_(NULL),
      current_state_(ACTIVITY_UNLOADED),
      app_activity_registry_(NULL) {
}

ActivityViewModel* AppActivity::GetActivityViewModel() {
  return this;
}

void AppActivity::SetCurrentState(Activity::ActivityState state) {
  DCHECK_NE(state, current_state_);
  ActivityState current_state = current_state_;
  // Remember the last requested state now so that a call to GetCurrentState()
  // returns the new state.
  current_state_ = state;

  switch (state) {
    case ACTIVITY_VISIBLE:
      MakeVisible();
      return;
    case ACTIVITY_INVISIBLE:
      if (current_state == ACTIVITY_VISIBLE)
        MakeInvisible();
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
  DCHECK(web_view_ || ACTIVITY_UNLOADED == current_state_);
  return current_state_;
}

bool AppActivity::IsVisible() {
  return web_view_ &&
         web_view_->visible() &&
         current_state_ != ACTIVITY_UNLOADED;
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
  DCHECK(app_activity_registry_);
  Activity* app_proxy = app_activity_registry_->unloaded_activity_proxy();
  if (app_proxy) {
    // TODO(skuhne): This should call the WindowListProvider to re-arrange.
    // Note: At this time the |AppActivity| did not get registered to the
    // |ResourceManager| - so we can move it around if needed.
    aura::Window* proxy_window = app_proxy->GetWindow();
    proxy_window->parent()->StackChildBelow(GetWindow(), proxy_window);
    Activity::Delete(app_proxy);
    // With the removal the object, the proxy should be deleted.
    DCHECK(!app_activity_registry_->unloaded_activity_proxy());
  }
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
    content::WebContents* web_contents = GetWebContents();
    web_view_ = new views::WebView(web_contents->GetBrowserContext());
    web_view_->SetWebContents(web_contents);
    // Make sure the content gets properly shown.
    if (current_state_ == ACTIVITY_VISIBLE) {
      MakeVisible();
    } else if (current_state_ == ACTIVITY_INVISIBLE) {
      MakeInvisible();
    } else {
      // If not previously specified, we change the state now to invisible..
      SetCurrentState(ACTIVITY_INVISIBLE);
    }
    Observe(web_contents);
    RegisterActivity();
  }
  return web_view_;
}

void AppActivity::CreateOverviewModeImage() {
  // TODO(skuhne): Implement this!
}

gfx::ImageSkia AppActivity::GetOverviewModeImage() {
  return overview_mode_image_;
}

AppActivity::~AppActivity() {
  // If this activity is registered, we unregister it now.
  if (app_activity_registry_)
    app_activity_registry_->UnregisterAppActivity(this);
}

void AppActivity::TitleWasSet(content::NavigationEntry* entry,
                              bool explicit_set) {
  ActivityManager::Get()->UpdateActivity(this);
}

void AppActivity::DidUpdateFaviconURL(
    const std::vector<content::FaviconURL>& candidates) {
  ActivityManager::Get()->UpdateActivity(this);
}

// Register an |activity| with an application.
// Note: This should only get called once for an |app_window| of the
// |activity|.
void AppActivity::RegisterActivity() {
  content::WebContents* web_contents = GetWebContents();
  AppRegistry* app_registry = AppRegistry::Get();
  // Get the application's registry.
  app_activity_registry_ = app_registry->GetAppActivityRegistry(
      app_id_, web_contents->GetBrowserContext());
  DCHECK(app_activity_registry_);
  // Register the activity.
  app_activity_registry_->RegisterAppActivity(this);
}

void AppActivity::MakeVisible() {
  // TODO(skuhne): Once we know how to handle the Overview mode, this has to
  // be moved into an ActivityContentController which is used by all activities.
  // Make the content visible.
  // TODO(skuhne): If this can be combined with web_activity, move this into a
  // separate class.
  web_view_->SetVisible(true);
  web_view_->GetWebContents()->GetNativeView()->Show();

  // Remove our proxy image.
  // TODO(skuhne): Once we have figured out how to do overview mode that code
  // needs to go here.
  overview_mode_image_ = gfx::ImageSkia();
}

void AppActivity::MakeInvisible() {
  // TODO(skuhne): Once we know how to handle the Overview mode, this has to
  // be moved into an ActivityContentController which is used by all activities.
  // TODO(skuhne): If this can be combined with web_activity, move this into a
  // separate class.
  DCHECK(web_view_->visible());
  // Create our proxy image.
  if (current_state_ == ACTIVITY_VISIBLE) {
    // Create a proxy image of the current visible content.
    // TODO(skuhne): Do this once we figure out how to do overview mode.
    overview_mode_image_ = gfx::ImageSkia();
  }
  // Now we can hide this.
  // Note: This might have to be done asynchronously after the readback took
  // place.
  web_view_->SetVisible(false);
  web_view_->GetWebContents()->GetNativeView()->Hide();
}

}  // namespace athena
