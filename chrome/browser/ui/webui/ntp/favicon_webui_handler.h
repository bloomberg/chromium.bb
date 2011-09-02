// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_FAVICON_WEBUI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_FAVICON_WEBUI_HANDLER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "content/browser/webui/web_ui.h"

class ExtensionIconColorManager;
class GURL;
class Profile;

namespace base {
class ListValue;
class StringValue;
}

class FaviconWebUIHandler : public WebUIMessageHandler {
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
  void OnFaviconDataAvailable(FaviconService::Handle request_handle,
                              history::FaviconData favicon);
  base::StringValue* GetDominantColorCssString(
      scoped_refptr<RefCountedMemory> png);

  CancelableRequestConsumerTSimple<int> consumer_;

  // Map from request ID to DOM ID so we can make the appropriate callback when
  // the favicon request comes back. This map exists because
  // CancelableRequestConsumerTSimple only takes POD keys.
  std::map<int, std::string> dom_id_map_;
  // A counter to track ID numbers as we use them.
  int id_;

  // Raw PNG representation of the favicon to show when the favicon
  // database doesn't have a favicon for a webpage.
  scoped_refptr<RefCountedMemory> default_favicon_;

  // Manage retrieval of icons from apps.
  scoped_ptr<ExtensionIconColorManager> app_icon_color_manager_;

  DISALLOW_COPY_AND_ASSIGN(FaviconWebUIHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_FAVICON_WEBUI_HANDLER_H_
