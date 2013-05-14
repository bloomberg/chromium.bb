// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_SHIM_APP_SHIM_HANDLER_MAC_H_
#define APPS_APP_SHIM_APP_SHIM_HANDLER_MAC_H_

#include <string>

namespace apps {

// Registrar, and interface for services that can handle interactions with OSX
// shim processes.
class AppShimHandler {
 public:
  class Host {
   public:
    // Invoked when the app is closed in the browser process.
    virtual void OnAppClosed() = 0;

   protected:
    virtual ~Host() {}
  };

  // Register a handler for an |app_mode_id|.
  static void RegisterHandler(const std::string& app_mode_id,
                              AppShimHandler* handler);

  // Remove a handler for an |app_mode_id|.
  static void RemoveHandler(const std::string& app_mode_id);

  // Returns the handler registered for the given |app_mode_id|, or NULL if none
  // is registered.
  static AppShimHandler* GetForAppMode(const std::string& app_mode_id);

  // Invoked by the shim host when the shim process is launched. The handler
  // must return true if successful, or false to indicate back to the shim
  // process that it should close.
  virtual bool OnShimLaunch(Host* host) = 0;

  // Invoked by the shim host when the connection to the shim process is closed.
  virtual void OnShimClose(Host* host) = 0;

  // Invoked by the shim host when the shim process receives a focus event.
  virtual void OnShimFocus(Host* host) = 0;

 protected:
  AppShimHandler() {}
  virtual ~AppShimHandler() {}
};

}  // namespace apps

#endif  // APPS_APP_SHIM_APP_SHIM_HANDLER_MAC_H_
