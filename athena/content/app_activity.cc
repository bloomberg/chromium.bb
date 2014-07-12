// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/app_activity.h"

#include "apps/shell/browser/shell_app_window.h"
#include "athena/activity/public/activity_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/webview.h"

namespace athena {

// TODO(mukai): specifies the same accelerators of WebActivity.
AppActivity::AppActivity(apps::ShellAppWindow* app_window)
    : app_window_(app_window),
      web_view_(NULL),
      current_state_(ACTIVITY_UNLOADED) {
  DCHECK(app_window_);
}

AppActivity::~AppActivity() {
  if (GetCurrentState() != ACTIVITY_UNLOADED)
    SetCurrentState(ACTIVITY_UNLOADED);
}

ActivityViewModel* AppActivity::GetActivityViewModel() {
  return this;
}

void AppActivity::SetCurrentState(Activity::ActivityState state) {
  switch (state) {
    case ACTIVITY_VISIBLE:
      // Fall through (for the moment).
    case ACTIVITY_INVISIBLE:
      // By clearing the overview mode image we allow the content to be shown.
      overview_mode_image_ = gfx::ImageSkia();
      // TODO(skuhne): Find out how to reload an app from the extension system.
      break;
    case ACTIVITY_BACKGROUND_LOW_PRIORITY:
      DCHECK(ACTIVITY_VISIBLE == current_state_ ||
             ACTIVITY_INVISIBLE == current_state_);
      // TODO(skuhne): Do this.
      break;
    case ACTIVITY_PERSISTENT:
      DCHECK_EQ(ACTIVITY_BACKGROUND_LOW_PRIORITY, current_state_);
      // TODO(skuhne): Do this.
      break;
    case ACTIVITY_UNLOADED:
      DCHECK_NE(ACTIVITY_UNLOADED, current_state_);
      // TODO(skuhne): Find out how to evict an app from the extension system.
      //      web_view_->EvictContent();
      break;
  }
  // Remember the last requested state.
  current_state_ = state;
}

Activity::ActivityState AppActivity::GetCurrentState() {
  // TODO(skuhne): Check here also eviction status.
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

}  // namespace athena
