// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_FAVICON_WEBUI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_FAVICON_WEBUI_HANDLER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_types.h"
#include "content/public/browser/web_ui_message_handler.h"

class ExtensionIconColorManager;

namespace base {
class ListValue;
}

class FaviconWebUIHandler : public content::WebUIMessageHandler {
 public:
  FaviconWebUIHandler();
  virtual ~FaviconWebUIHandler();

  // WebUIMessageHandler
  virtual void RegisterMessages() OVERRIDE;

  // Called from the JS to get the dominant color of a favicon. The first
  // argument is a favicon URL, the second is the ID of the DOM node that is
  // asking for it.
  void HandleGetFaviconDominantColor(const base::ListValue* args);

  // As above, but for an app tile. The sole argument is the extension ID.
  void HandleGetAppIconDominantColor(const base::ListValue* args);

  // Callback getting signal that an app icon is loaded.
  void NotifyAppIconReady(const std::string& extension_id);

 private:
  // Called when favicon data is available from the history backend.
  void OnFaviconDataAvailable(
      int request_handle,
      const favicon_base::FaviconRawBitmapResult& bitmap_result);

  base::CancelableTaskTracker cancelable_task_tracker_;

  // Map from request ID to DOM ID so we can make the appropriate callback when
  // the favicon request comes back. This map exists because
  // CancelableRequestConsumerTSimple only takes POD keys.
  std::map<int, std::string> dom_id_map_;
  // A counter to track ID numbers as we use them.
  int id_;

  // Raw PNG representation of the favicon to show when the favicon
  // database doesn't have a favicon for a webpage.
  scoped_refptr<base::RefCountedMemory> default_favicon_;

  // Manage retrieval of icons from apps.
  scoped_ptr<ExtensionIconColorManager> app_icon_color_manager_;

  DISALLOW_COPY_AND_ASSIGN(FaviconWebUIHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_FAVICON_WEBUI_HANDLER_H_
