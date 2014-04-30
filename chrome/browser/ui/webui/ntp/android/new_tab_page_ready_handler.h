// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_ANDROID_NEW_TAB_PAGE_READY_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_ANDROID_NEW_TAB_PAGE_READY_HANDLER_H_

#include "base/compiler_specific.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

// Sends a notification when the NTP is completely finished loading
// and navigating.
class NewTabPageReadyHandler : public content::WebUIMessageHandler {
 public:
  NewTabPageReadyHandler();
  virtual ~NewTabPageReadyHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Callback for "notifyNTPReady".
  void HandleNewTabPageReady(const base::ListValue* args);

  // Callback for "NTPUnexpectedNavigation".
  void HandleNewTabPageUnexpectedNavigation(const base::ListValue* args);

  // Callback for "notifyNTPTitleLoaded".
  void HandleNewTabPageTitleLoaded(const base::ListValue* args);

 private:
  DISALLOW_COPY_AND_ASSIGN(NewTabPageReadyHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_ANDROID_NEW_TAB_PAGE_READY_HANDLER_H_
