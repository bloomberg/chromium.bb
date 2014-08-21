// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_CONTENT_APP_ACTIVITY_REGISTRY_H_
#define ATHENA_CONTENT_APP_ACTIVITY_REGISTRY_H_

#include <vector>

#include "athena/activity/public/activity_view_model.h"
#include "athena/content/app_activity_proxy.h"

namespace aura {
class Window;
}

namespace content {
class BrowserContext;
}

namespace athena {

class AppActivity;

// This class keeps track of all existing |AppActivity|s and shuts down all of
// them when the application gets unloaded to save memory. It will then replace
// the |AppActivity| in the Activity list as proxy to allow restarting of the
// application.
class ATHENA_EXPORT AppActivityRegistry {
 public:
  AppActivityRegistry(const std::string& app_id,
                      content::BrowserContext* browser_context);
  virtual ~AppActivityRegistry();

  // Register an |AppActivity| with this application.
  void RegisterAppActivity(AppActivity* app_activity);

  // Unregister a previously attached |AppActivity|.
  // Note that detaching the last |AppActivity| will delete this object - unless
  // the resource manager was trying to unload the application.
  // Note furthermore that Detach can be called without ever being registered.
  void UnregisterAppActivity(AppActivity* app_activity);

  // Returns the number of activities/windows with this application.
  int NumberOfActivities() const { return activity_list_.size(); }

  // Returns the |AppActivity| at |index|. It will return NULL if an invalid
  // index was specified.
  AppActivity* GetAppActivityAt(size_t index);

  // Unload all application associated activities to save resources.
  void Unload();

  // Returns true if the application is in the unloaded state.
  bool IsUnloaded() { return unloaded_activity_proxy_ != NULL; }

  content::BrowserContext* browser_context() const { return browser_context_; }
  const std::string& app_id() const { return app_id_; }

  AppActivityProxy* unloaded_activity_proxy_for_test() {
    return unloaded_activity_proxy_;
  }

 protected:
  friend AppActivityProxy;

  // When the |AppActivityProxy| gets destroyed it should call this function
  // to disconnect from this object. This call might destroy |this|.
  void ProxyDestroyed(AppActivityProxy* proxy);

  // When called by the |AppActivityProxy| to restart the application, it can
  // cause the application to restart. When that happens the proxy will get
  // destroyed. After this call |this| might be destroyed.
  void RestartApplication(AppActivityProxy* proxy);

 private:
  // Move the window before the most recently used application window.
  void MoveBeforeMruApplicationWindow(aura::Window* window);

  // A list of all activities associated with this application.
  std::vector<AppActivity*> activity_list_;

  // The application id for this proxy.
  std::string app_id_;

  // The browser context of the user.
  content::BrowserContext* browser_context_;

  // When the activity is unloaded this is the AppActivityProxy. The object is
  // owned the the ActivityManager.
  AppActivityProxy* unloaded_activity_proxy_;

  // The presentation values.
  SkColor color_;
  base::string16 title_;
  gfx::ImageSkia image_;

  DISALLOW_COPY_AND_ASSIGN(AppActivityRegistry);
};

}  // namespace athena

#endif  // ATHENA_CONTENT_APP_ACTIVITY_REGISTRY_H_
