// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_INFO_UI_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_INFO_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "googleurl/src/gurl.h"


namespace base {
class ListValue;
}

namespace content {
class WebUIDataSource;
}

// WebUI controller for the informative bubble shown on clicking a script badge.
class ExtensionInfoUI : public content::WebUIController {
 public:
  explicit ExtensionInfoUI(content::WebUI* web_ui, const GURL& url);

  // Returns the chrome://extension-info/ URL for this extension.
  static GURL GetURL(const std::string& extension_id);

 private:
  virtual ~ExtensionInfoUI();

  // Load details about the extension into source_. Called during construction.
  void AddExtensionDataToSource(const std::string& extension_id);

  content::WebUIDataSource* source_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInfoUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_INFO_UI_H_
