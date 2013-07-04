// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_SHIM_APP_SHIM_HANDLER_MAC_H_
#define APPS_APP_SHIM_APP_SHIM_HANDLER_MAC_H_

#include <string>

#include "apps/app_shim/app_shim_launch.h"
#include "base/files/file_path.h"

class Profile;

namespace apps {

// Registrar, and interface for services that can handle interactions with OSX
// shim processes.
class AppShimHandler {
 public:
  class Host {
   public:
    // Invoked when the app is successfully launched.
    virtual void OnAppLaunchComplete(AppShimLaunchResult result) = 0;
    // Invoked when the app is closed in the browser process.
    virtual void OnAppClosed() = 0;

    // Allows the handler to determine which app this host corresponds to.
    virtual base::FilePath GetProfilePath() const = 0;
    virtual std::string GetAppId() const = 0;

   protected:
    virtual ~Host() {}
  };

  // Register a handler for an |app_mode_id|.
  static void RegisterHandler(const std::string& app_mode_id,
                              AppShimHandler* handler);

  // Remove a handler for an |app_mode_id|.
  static void RemoveHandler(const std::string& app_mode_id);

  // Returns the handler registered for the given |app_mode_id|. If there is
  // none registered, it returns the default handler or NULL if there is no
  // default handler.
  static AppShimHandler* GetForAppMode(const std::string& app_mode_id);

  // Sets the default handler to return when there is no app-specific handler.
  // Setting this to NULL removes the default handler.
  static void SetDefaultHandler(AppShimHandler* handler);

  // Invoked by the shim host when the shim process is launched. The handler
  // must call OnAppLaunchComplete to inform the shim of the result.
  // |launch_now| indicates whether to launch the associated app.
  virtual void OnShimLaunch(Host* host, AppShimLaunchType launch_type) = 0;

  // Invoked by the shim host when the connection to the shim process is closed.
  virtual void OnShimClose(Host* host) = 0;

  // Invoked by the shim host when the shim process receives a focus event.
  virtual void OnShimFocus(Host* host, AppShimFocusType focus_type) = 0;

  // Invoked by the shim host when the shim process is hidden or shown.
  virtual void OnShimSetHidden(Host* host, bool hidden) = 0;

  // Invoked by the shim host when the shim process receives a quit event.
  virtual void OnShimQuit(Host* host) = 0;

 protected:
  AppShimHandler() {}
  virtual ~AppShimHandler() {}
};

}  // namespace apps

#endif  // APPS_APP_SHIM_APP_SHIM_HANDLER_MAC_H_
