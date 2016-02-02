// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_LIST_START_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_APP_LIST_START_PAGE_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

namespace app_list {

// Handler for the app launcher start page.
class StartPageHandler : public content::WebUIMessageHandler {
 public:
  StartPageHandler();
  ~StartPageHandler() override;

 private:
  // content::WebUIMessageHandler overrides:
  void RegisterMessages() override;

  // JS callbacks.
  void HandleAppListShown(const base::ListValue* args);
  void HandleDoodleClicked(const base::ListValue* args);
  void HandleInitialize(const base::ListValue* args);
  void HandleLaunchApp(const base::ListValue* args);

  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(StartPageHandler);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_WEBUI_APP_LIST_START_PAGE_HANDLER_H_
