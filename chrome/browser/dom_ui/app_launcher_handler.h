// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_APP_LAUNCHER_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_APP_LAUNCHER_HANDLER_H_

#include "chrome/browser/dom_ui/dom_ui.h"

class Extension;
class ExtensionsService;

// The handler for Javascript messages related to the "apps" view.
class AppLauncherHandler : public DOMMessageHandler {
 public:
  explicit AppLauncherHandler(ExtensionsService* extension_service);
  virtual ~AppLauncherHandler();

  // DOMMessageHandler implementation.
  virtual DOMMessageHandler* Attach(DOMUI* dom_ui);
  virtual void RegisterMessages();

  // Populate a dictionary with the information from an extension.
  static void CreateAppInfo(Extension* extension, DictionaryValue* value);

  // Callback for the "getAll" message.
  void HandleGetApps(const Value* value);

  // Callback for the "launch" message.
  void HandleLaunchApp(const Value* value);

 private:
  // The apps are represented in the extensions model.
  scoped_refptr<ExtensionsService> extensions_service_;

  DISALLOW_COPY_AND_ASSIGN(AppLauncherHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_APP_LAUNCHER_HANDLER_H_
