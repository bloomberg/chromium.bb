// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_UI_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace media_router {

class MediaRouterWebUIMessageHandler;

// Implements the chrome://media-router user interface.
class MediaRouterUI : public ConstrainedWebDialogUI {
 public:
  // |web_ui| owns this object and is used to initialize the base class.
  explicit MediaRouterUI(content::WebUI* web_ui);
  ~MediaRouterUI() override;

  // Closes the media router UI.
  void Close();

 private:
  // Owned by the |web_ui| passed in the ctor, and guaranteed to be deleted
  // only after it has deleted |this|.
  MediaRouterWebUIMessageHandler* handler_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterUI);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_UI_H_
