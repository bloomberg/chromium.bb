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
// from Return* functions.
class WebUIHandler : public content::WebUIMessageHandler,
                     public base::SupportsWeakPtr<WebUIHandler> {
 public:
  WebUIHandler();

 private:
  virtual ~WebUIHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Callback for the "getActiveIntervals" message.
  void HandleGetActiveIntervals(const base::ListValue* args);

  // Returns results through callback for the "getActiveIntervals" message.
  void ReturnActiveIntervals(const base::ListValue* results);

  DISALLOW_COPY_AND_ASSIGN(WebUIHandler);
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_UI_WEBUI_PERFORMANCE_MONITOR_WEB_UI_HANDLER_H_
