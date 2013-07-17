// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_LOCAL_DISCOVERY_LOCAL_DISCOVERY_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_LOCAL_DISCOVERY_LOCAL_DISCOVERY_UI_HANDLER_H_

#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui_message_handler.h"

// UI Handler for chrome://devices/
// It listens to local discovery notifications and passes those notifications
// into the Javascript to update the page.
class LocalDiscoveryUIHandler : public content::WebUIMessageHandler {
 public:
  LocalDiscoveryUIHandler();
  virtual ~LocalDiscoveryUIHandler();

  // WebUIMessageHandler implementation.
  // Does nothing for now.
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Callback for adding new device to the devices page.
  // |name| contains a user friendly name of the device.
  void OnNewDevice(const std::string& name);

  content::ActionCallback action_callback_;

  DISALLOW_COPY_AND_ASSIGN(LocalDiscoveryUIHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_LOCAL_DISCOVERY_LOCAL_DISCOVERY_UI_HANDLER_H_
