// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_ACTIVITY_UI_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_ACTIVITY_UI_H_
#pragma once

#include "chrome/browser/extensions/extension_activity_log.h"
#include "content/public/browser/web_ui_controller.h"

namespace extensions {
class Extension;
}

class ExtensionActivityUI : public content::WebUIController,
                            public ExtensionActivityLog::Observer {
 public:
  explicit ExtensionActivityUI(content::WebUI* web_ui);
  virtual ~ExtensionActivityUI();

  // Callback for "requestExtensionData".
  void HandleRequestExtensionData(const base::ListValue* args);

  // ExtensionActivityLog::Observer implementation.
  virtual void OnExtensionActivity(const extensions::Extension* extension,
                                   ExtensionActivityLog::Activity activity,
                                   const std::string& msg) OVERRIDE;

 private:
  const extensions::Extension* extension_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionActivityUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_ACTIVITY_UI_H_
