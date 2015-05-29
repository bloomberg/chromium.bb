// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_WEBUI_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_WEBUI_MESSAGE_HANDLER_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}  // namespace base

namespace media_router {

class MediaRouterUI;

// The handler for Javascript messages related to the media router dialog.
class MediaRouterWebUIMessageHandler : public content::WebUIMessageHandler {
 public:
  MediaRouterWebUIMessageHandler();
  ~MediaRouterWebUIMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  // Handlers for JavaScript messages.
  void OnGetInitialSettings(const base::ListValue* args);
  void OnCreateRoute(const base::ListValue* args);
  void OnActOnIssue(const base::ListValue* args);
  void OnCloseRoute(const base::ListValue* args);
  void OnCloseDialog(const base::ListValue* args);

  // Helper function for getting a pointer to the corresponding MediaRouterUI.
  MediaRouterUI* GetMediaRouterUI() const;

  // Keeps track of whether a command to close the dialog has been issued.
  bool dialog_closing_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterWebUIMessageHandler);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_WEBUI_MESSAGE_HANDLER_H_
