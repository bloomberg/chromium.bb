// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_LOCAL_OMNIBOX_POPUP_LOCAL_OMNIBOX_POPUP_UI_H_
#define CHROME_BROWSER_UI_WEBUI_LOCAL_OMNIBOX_POPUP_LOCAL_OMNIBOX_POPUP_UI_H_

#include "base/basictypes.h"
#include "content/public/browser/web_ui_controller.h"

// The UI for chrome://local-omnibox-popup/
class LocalOmniboxPopupUI : public content::WebUIController {
 public:
  explicit LocalOmniboxPopupUI(content::WebUI* contents);
  virtual ~LocalOmniboxPopupUI();

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalOmniboxPopupUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_LOCAL_OMNIBOX_POPUP_LOCAL_OMNIBOX_POPUP_UI_H_
