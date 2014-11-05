// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/app_activity.h"

#include "athena/activity/public/activity_manager.h"
#include "athena/activity/public/activity_view.h"
#include "athena/content/app_activity_registry.h"
#include "athena/content/content_proxy.h"
#include "athena/content/media_utils.h"
#include "athena/content/public/app_registry.h"
#include "athena/wm/public/window_list_provider.h"
#include "athena/wm/public/window_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace athena {

// TODO(mukai): specifies the same accelerators of WebActivity.
AppActivity::AppActivity(const std::string& app_id, views::WebView* web_view)
    : app_id_(app_id),
      web_view_(web_view),
      current_state_(ACTIVITY_UNLOADED),
      app_activity_registry_(nullptr),
      activity_view_(nullptr) {
  Observe(web_view->GetWebContents());
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
  return current_state_ == ACTIVITY_UNLOADED ?
      Activity::ACTIVITY_MEDIA_STATE_NONE :
      GetActivityMediaState(GetWebContents());
}

aura::Window* AppActivity::GetWindow() {
  return web_view_ && web_view_->GetWidget()
             ? web_view_->GetWidget()->GetNativeWindow()
             : nullptr;
}

content::WebContents* AppActivity::GetWebContents() {
  return !web_view_ ? nullptr : web_view_->GetWebContents();
}

void AppActivity::Init() {
  // Before we remove the proxy, we have to register the activity and
  // initialize its to move it to the proper activity list location.
  RegisterActivity();

  DCHECK(app_activity_registry_);
  Activity* app_proxy = app_activity_registry_->unloaded_activity_proxy();
  if (app_proxy) {
    // Note: At this time the |AppActivity| did not get registered to the
    // |ResourceManager| - so we can move it around if needed.
    WindowListProvider* window_list_provider =
        WindowManager::Get()->GetWindowListProvider();
    // TODO(skuhne): After the decision is made how we want to handle visibility
    // transitions (issue 421680) this code might change.
    // If the proxy was the active window, its deletion will cause a window
    // reordering since the next activatable window in line will move up to the
    // front. Since the application window is still hidden at this time, it is
    // not yet activatable and the window behind it will move to the front.
    if (wm::IsActiveWindow(app_proxy->GetWindow())) {
      // Delete the proxy window first and then move the new window to the top
      // of the stack, replacing the proxy window. Note that by deleting the
      // proxy the activation will change to the next (activatable) object and
      // thus we have to move the window in front at the end.
      Activity::Delete(app_proxy);
      if (GetWindow() != window_list_provider->GetWindowList().back()) {
        window_list_provider->StackWindowFrontOf(
            GetWindow(),
            window_list_provider->GetWindowList().back());
      }
    } else {
      // The app window goes in front of the proxy window (we need to first
      // place the window before we can delete it).
      window_list_provider->StackWindowFrontOf(GetWindow(),
                                               app_proxy->GetWindow());
      Activity::Delete(app_proxy);
    }
    // The proxy should now be deleted.
    DCHECK(!app_activity_registry_->unloaded_activity_proxy());
  }

  // Make sure the content gets properly shown.
  if (current_state_ == ACTIVITY_VISIBLE) {
    HideContentProxy();
  } else if (current_state_ == ACTIVITY_INVISIBLE) {
    ShowContentProxy();
  } else {
    // If not previously specified, we change the state now to invisible..
    SetCurrentState(ACTIVITY_INVISIBLE);
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

void AppActivity::SetActivityView(ActivityView* view) {
  DCHECK(!activity_view_);
  activity_view_ = view;
}

bool AppActivity::UsesFrame() const {
  return false;
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
      web_view_(nullptr),
      current_state_(ACTIVITY_UNLOADED),
      app_activity_registry_(nullptr),
      activity_view_(nullptr) {
}

AppActivity::~AppActivity() {
  // If this activity is registered, we unregister it now.
  if (app_activity_registry_)
    app_activity_registry_->UnregisterAppActivity(this);
}

void AppActivity::TitleWasSet(content::NavigationEntry* entry,
                              bool explicit_set) {
  if (activity_view_)
    activity_view_->UpdateTitle();
}

void AppActivity::DidUpdateFaviconURL(
    const std::vector<content::FaviconURL>& candidates) {
  if (activity_view_)
    activity_view_->UpdateIcon();
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
    content_proxy_.reset(new ContentProxy(web_view_));
}

}  // namespace athena
