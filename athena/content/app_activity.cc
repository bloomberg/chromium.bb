// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/app_activity.h"

#include "athena/activity/public/activity_manager.h"
#include "athena/content/app_activity_registry.h"
#include "athena/content/content_proxy.h"
#include "athena/content/public/app_registry.h"
#include "athena/wm/public/window_list_provider.h"
#include "athena/wm/public/window_manager.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "ui/aura/window.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

namespace athena {

// TODO(mukai): specifies the same accelerators of WebActivity.
AppActivity::AppActivity(extensions::AppWindow* app_window,
                         views::WebView* web_view)
    : app_id_(app_window->extension_id()),
      web_view_(web_view),
      current_state_(ACTIVITY_UNLOADED),
      app_activity_registry_(NULL) {
  DCHECK_EQ(app_window->web_contents(), web_view->GetWebContents());
  Observe(app_window->web_contents());
}

scoped_ptr<ContentProxy> AppActivity::GetContentProxy() {
  // Note: After this call, the content is still valid because the contents
  // destruction will destroy this |AppActivity| object.
  if (content_proxy_.get())
    content_proxy_->OnPreContentDestroyed();
  return content_proxy_.Pass();
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
      HideContentProxy();
      return;
    case ACTIVITY_INVISIBLE:
      if (current_state == ACTIVITY_VISIBLE)
        ShowContentProxy();
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

content::WebContents* AppActivity::GetWebContents() {
  return !web_view_ ? NULL : web_view_->GetWebContents();
}

void AppActivity::Init() {
  DCHECK(app_activity_registry_);
  Activity* app_proxy = app_activity_registry_->unloaded_activity_proxy();
  if (app_proxy) {
    // Note: At this time the |AppActivity| did not get registered to the
    // |ResourceManager| - so we can move it around if needed.
    WindowListProvider* window_list_provider =
        WindowManager::Get()->GetWindowListProvider();
    window_list_provider->StackWindowFrontOf(app_proxy->GetWindow(),
                                             GetWindow());
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

gfx::ImageSkia AppActivity::GetIcon() const {
  return gfx::ImageSkia();
}

bool AppActivity::UsesFrame() const {
  return false;
}

views::Widget* AppActivity::CreateWidget() {
  // Make sure the content gets properly shown.
  if (current_state_ == ACTIVITY_VISIBLE) {
    HideContentProxy();
  } else if (current_state_ == ACTIVITY_INVISIBLE) {
    ShowContentProxy();
  } else {
    // If not previously specified, we change the state now to invisible..
    SetCurrentState(ACTIVITY_INVISIBLE);
  }
  RegisterActivity();
  return web_view_->GetWidget();
}

views::View* AppActivity::GetContentsView() {
  return web_view_;
}

gfx::ImageSkia AppActivity::GetOverviewModeImage() {
  if (content_proxy_.get())
    return content_proxy_->GetContentImage();
  return gfx::ImageSkia();
}

void AppActivity::PrepareContentsForOverview() {
  // Turn on fast resizing to avoid re-laying out the web contents when
  // entering / exiting overview mode and the content is visible.
  if (!content_proxy_.get())
    web_view_->SetFastResize(true);
}

void AppActivity::ResetContentsView() {
  // Turn on fast resizing to avoid re-laying out the web contents when
  // entering / exiting overview mode and the content is visible.
  if (!content_proxy_.get()) {
    web_view_->SetFastResize(false);
    web_view_->Layout();
  }
}

AppActivity::AppActivity(const std::string& app_id)
    : app_id_(app_id),
      web_view_(NULL),
      current_state_(ACTIVITY_UNLOADED),
      app_activity_registry_(NULL) {
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
  content::WebContents* web_contents = web_view_->GetWebContents();
  AppRegistry* app_registry = AppRegistry::Get();
  // Get the application's registry.
  app_activity_registry_ = app_registry->GetAppActivityRegistry(
      app_id_, web_contents->GetBrowserContext());
  DCHECK(app_activity_registry_);
  // Register the activity.
  app_activity_registry_->RegisterAppActivity(this);
}

void AppActivity::HideContentProxy() {
  content_proxy_.reset();
}

void AppActivity::ShowContentProxy() {
  if (!content_proxy_.get() && web_view_)
    content_proxy_.reset(new ContentProxy(web_view_, this));
}

}  // namespace athena
