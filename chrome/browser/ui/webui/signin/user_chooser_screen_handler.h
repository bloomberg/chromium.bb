// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_USER_CHOOSER_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_USER_CHOOSER_SCREEN_HANDLER_H_

#include "base/compiler_specific.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class DictionaryValue;
class ListValue;
}

class UserChooserScreenHandler : public content::WebUIMessageHandler {
 public:
  UserChooserScreenHandler();
  virtual ~UserChooserScreenHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  void GetLocalizedValues(base::DictionaryValue* localized_strings);

 private:
  void HandleInitialize(const base::ListValue* args);
  void HandleAddUser(const base::ListValue* args);
  void HandleLaunchGuest(const base::ListValue* args);
  void HandleLaunchUser(const base::ListValue* args);
  void HandleRemoveUser(const base::ListValue* args);

  // Sends user list to account chooser.
  void SendUserList();

  DISALLOW_COPY_AND_ASSIGN(UserChooserScreenHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_USER_CHOOSER_SCREEN_HANDLER_H_
