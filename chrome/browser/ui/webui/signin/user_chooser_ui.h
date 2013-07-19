// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_USER_CHOOSER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_USER_CHOOSER_UI_H_

#include "content/public/browser/web_ui_controller.h"

class UserChooserScreenHandler;

namespace base {
class DictionaryValue;
}
namespace content {
class WebUIDataSource;
}

// A WebUI dialog to display available users.
class UserChooserUI : public content::WebUIController {
 public:
  explicit UserChooserUI(content::WebUI* web_ui);
  virtual ~UserChooserUI();

 private:
  content::WebUIDataSource* CreateUIDataSource(
      const base::DictionaryValue& localized_strings);
  void GetLocalizedStrings(base::DictionaryValue* localized_strings);

  UserChooserScreenHandler* user_chooser_screen_handler_;

  DISALLOW_COPY_AND_ASSIGN(UserChooserUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_USER_CHOOSER_UI_H_
