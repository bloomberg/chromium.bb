// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PERFORMANCE_MONITOR_WEB_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PERFORMANCE_MONITOR_WEB_UI_HANDLER_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
class Time;
}  // namespace base

namespace performance_monitor {

// This class handles messages to and from the performance monitor page.
// Incoming calls are handled by the Handle* functions and callbacks are made
// from ReturnResults functions.
class WebUIHandler : public content::WebUIMessageHandler,
                     public base::SupportsWeakPtr<WebUIHandler> {
 public:
  WebUIHandler();

 private:
  virtual ~WebUIHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Returns |results| through the given |callback| string.
  void ReturnResults(const std::string& callback,
                     const base::ListValue* results);

  // Callback for the "getActiveIntervals" message.
  void HandleGetActiveIntervals(const base::ListValue* args);

  // Callback for the "getAllEventTypes" message.
  void HandleGetAllEventTypes(const base::ListValue* args);

  // Callback for the "getEvents" message.
  void HandleGetEvents(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(WebUIHandler);
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_UI_WEBUI_PERFORMANCE_MONITOR_WEB_UI_HANDLER_H_
