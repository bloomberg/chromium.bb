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

class GURL;
class Profile;

namespace base {
class ListValue;
}

class FaviconWebUIHandler : public WebUIMessageHandler {
 public:
  FaviconWebUIHandler();
  virtual ~FaviconWebUIHandler();

  // WebUIMessageHandler
  virtual void RegisterMessages() OVERRIDE;

  void HandleGetFaviconDominantColor(const base::ListValue* args);

 private:
  // Called when favicon data is available from the history backend.
  void OnFaviconDataAvailable(FaviconService::Handle request_handle,
                              history::FaviconData favicon);

  CancelableRequestConsumerTSimple<int> consumer_;

  // Raw PNG representation of the favicon to show when the favicon
  // database doesn't have a favicon for a webpage.
  scoped_refptr<RefCountedMemory> default_favicon_;

  // A mapping of favicon ID to callback names for requests that are
  // in-progress.
  std::map<int, std::string> callbacks_map_;

  DISALLOW_COPY_AND_ASSIGN(FaviconWebUIHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_FAVICON_WEBUI_HANDLER_H_
