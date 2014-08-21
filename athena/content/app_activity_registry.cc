// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/app_activity_registry.h"

#include "athena/activity/public/activity_manager.h"
#include "athena/content/app_activity.h"
#include "athena/content/app_activity_proxy.h"
#include "athena/content/public/app_content_control_delegate.h"
#include "athena/content/public/app_registry.h"
#include "ui/aura/window.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace athena {

AppActivityRegistry::AppActivityRegistry(
    const std::string& app_id,
    content::BrowserContext* browser_context) :
      app_id_(app_id),
      browser_context_(browser_context),
      unloaded_activity_proxy_(NULL) {}

AppActivityRegistry::~AppActivityRegistry() {
  CHECK(activity_list_.empty());
  if (unloaded_activity_proxy_)
    ActivityManager::Get()->RemoveActivity(unloaded_activity_proxy_);
  DCHECK(!unloaded_activity_proxy_);
}

void AppActivityRegistry::RegisterAppActivity(AppActivity* app_activity) {
  if (unloaded_activity_proxy_) {
    // Since we add an application window, the activity isn't unloaded anymore.
    ActivityManager::Get()->RemoveActivity(unloaded_activity_proxy_);
    // With the removal the object should have been deleted and we should have
    // been informed of the object's destruction.
    DCHECK(!unloaded_activity_proxy_);
  }
  // The same window should never be added twice.
  CHECK(std::find(activity_list_.begin(),
                  activity_list_.end(),
                  app_activity) == activity_list_.end());
  activity_list_.push_back(app_activity);
}

void AppActivityRegistry::UnregisterAppActivity(AppActivity* app_activity) {
  // It is possible that a detach gets called without ever being attached.
  std::vector<AppActivity*>::iterator it =
      std::find(activity_list_.begin(), activity_list_.end(), app_activity);
  if (it == activity_list_.end())
    return;

  activity_list_.erase(it);
  // When the last window gets destroyed and there is no proxy to restart, we
  // delete ourselves.
  if (activity_list_.empty() && !unloaded_activity_proxy_) {
    AppRegistry::Get()->RemoveAppActivityRegistry(this);
    // after this call this object should be gone.
  }
}

AppActivity* AppActivityRegistry::GetAppActivityAt(size_t index) {
  if (index >= activity_list_.size())
    return NULL;
  return activity_list_[index];
}

void AppActivityRegistry::Unload() {
  CHECK(!unloaded_activity_proxy_);
  DCHECK(!activity_list_.empty());

  // In order to allow an entire application to unload we require that all of
  // its activities are marked as unloaded.
  for (std::vector<AppActivity*>::iterator it = activity_list_.begin();
       it != activity_list_.end(); ++it) {
    if ((*it)->GetCurrentState() != Activity::ACTIVITY_UNLOADED)
      return;
  }

  // Create an activity proxy which can be used to re-activate the app. Insert
  // the proxy then into the activity stream at the location of the (newest)
  // current activity.
  unloaded_activity_proxy_ =
      new AppActivityProxy(activity_list_[0]->GetActivityViewModel(), this);
  ActivityManager::Get()->AddActivity(unloaded_activity_proxy_);
  // The new activity should be in the place of the most recently used app
  // window. To get it there, we get the most recently used application window
  // and place the proxy activities window in front or behind, so that when the
  // activity disappears it takes its place.
  MoveBeforeMruApplicationWindow(unloaded_activity_proxy_->GetWindow());

  // Unload the application. This operation will be asynchronous.
  if (!AppRegistry::Get()->GetDelegate()->UnloadApplication(app_id_,
                                                            browser_context_)) {
    while(!activity_list_.empty())
      delete activity_list_.back();
  }
}

void AppActivityRegistry::ProxyDestroyed(AppActivityProxy* proxy) {
  DCHECK_EQ(unloaded_activity_proxy_, proxy);
  unloaded_activity_proxy_ = NULL;
  if (activity_list_.empty()) {
    AppRegistry::Get()->RemoveAppActivityRegistry(this);
    // |This| is gone now.
  }
}

void AppActivityRegistry::RestartApplication(AppActivityProxy* proxy) {
  DCHECK_EQ(unloaded_activity_proxy_, proxy);
  // Restart the application.
  AppRegistry::Get()->GetDelegate()->RestartApplication(app_id_,
                                                        browser_context_);
  // Remove the activity from the Activity manager.
  ActivityManager::Get()->RemoveActivity(unloaded_activity_proxy_);
  delete unloaded_activity_proxy_;  // Will call ProxyDestroyed.
  // After this call |this| might be gone if the app did not open a window yet.
}

void AppActivityRegistry::MoveBeforeMruApplicationWindow(aura::Window* window) {
  DCHECK(activity_list_.size());
  // TODO(skuhne): This needs to be changed to some kind of delegate which
  // resides in the window manager.
  const aura::Window::Windows children =
      activity_list_[0]->GetWindow()->parent()->children();;
  // Find the first window in the container which is part of the application.
  for (aura::Window::Windows::const_iterator child_iterator = children.begin();
      child_iterator != children.end(); ++child_iterator) {
    for (std::vector<AppActivity*>::iterator app_iterator =
             activity_list_.begin();
         app_iterator != activity_list_.end(); ++app_iterator) {
      if (*child_iterator == (*app_iterator)->GetWindow()) {
        // Since "StackChildBelow" does not change the order if the window
        // if the window is below - but not immediately behind - the target
        // window, we re-stack both ways.
        window->parent()->StackChildBelow(window, *child_iterator);
        window->parent()->StackChildBelow(*child_iterator, window);
        return;
      }
    }
  }
  NOTREACHED() << "The application does not get tracked by the mru list";
}

}  // namespace athena
