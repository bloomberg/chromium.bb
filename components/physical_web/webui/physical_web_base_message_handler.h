// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PHYSICAL_WEB_WEBUI_PHYSICAL_WEB_BASE_MESSAGE_HANDLER_H_
#define COMPONENTS_PHYSICAL_WEB_WEBUI_PHYSICAL_WEB_BASE_MESSAGE_HANDLER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/values.h"

namespace physical_web {

class PhysicalWebDataSource;

}  // namespace physical_web

namespace physical_web_ui {

// This is the equivalent of content::WebUI::MessageCallback.
typedef base::Callback<void(const base::ListValue*)> MessageCallback;

// The base handler for Javascript messages for the chrome://physical-web page.
// This does not implement WebUIMessageHandler or register its methods.
class PhysicalWebBaseMessageHandler {
 public:
  PhysicalWebBaseMessageHandler();
  virtual ~PhysicalWebBaseMessageHandler();

  // Handles the RequestNearbyURLs message, returning URLs that are currently
  // being broadcasted by Physical Web transports.
  void HandleRequestNearbyURLs(const base::ListValue* args);

  // Handles a click on a Physical Web URL, recording the click and
  // directing the user appropriately.
  void HandlePhysicalWebItemClicked(const base::ListValue* args);

  // Registers the messages that this MessageHandler can handle.
  void RegisterMessages();

 protected:
  // Subclasses should implement these protected methods as a pass through to
  // the similarly named methods of the appropriate WebUI object (the exact
  // WebUI class differs per platform).
  virtual void RegisterMessageCallback(
      const std::string& message,
      const MessageCallback& callback) = 0;
  virtual void CallJavaScriptFunction(const std::string& function,
                                      const base::Value& arg) = 0;
  virtual physical_web::PhysicalWebDataSource* GetPhysicalWebDataSource() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PhysicalWebBaseMessageHandler);
};

}  // namespace physical_web_ui

#endif  // COMPONENTS_PHYSICAL_WEB_WEBUI_PHYSICAL_WEB_BASE_MESSAGE_HANDLER_H_
