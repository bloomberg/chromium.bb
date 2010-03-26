// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_APP_LAUNCHER_UI_H_
#define CHROME_BROWSER_DOM_UI_APP_LAUNCHER_UI_H_

#include <string>

#include "base/ref_counted.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/dom_ui/dom_ui.h"

class Extension;
class ExtensionsService;
class GURL;
class RefCountedMemory;

// The HTML source for the apps view.
class AppLauncherUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  AppLauncherUIHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string& path) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppLauncherUIHTMLSource);
};

// The handler for Javascript messages related to the "apps" view.
class AppLauncherDOMHandler : public DOMMessageHandler {
 public:
  explicit AppLauncherDOMHandler(ExtensionsService* extension_service);
  virtual ~AppLauncherDOMHandler();

  // DOMMessageHandler implementation.
  virtual DOMMessageHandler* Attach(DOMUI* dom_ui);
  virtual void RegisterMessages();

  // Populate a dictionary with the information from an extension.
  static void CreateAppInfo(Extension* extension, DictionaryValue* value);

  // Callback for the "getAll" message.
  void HandleGetAll(const Value* value);

  // Callback for the "launch" message.
  void HandleLaunch(const Value* value);

 private:
  // The apps are represented in the extensions model.
  scoped_refptr<ExtensionsService> extensions_service_;

  DISALLOW_COPY_AND_ASSIGN(AppLauncherDOMHandler);
};

// This class is used to hook up chrome://applauncher/.
class AppLauncherUI : public DOMUI {
 public:
  explicit AppLauncherUI(TabContents* contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(AppLauncherUI);
};

#endif  // CHROME_BROWSER_DOM_UI_APP_LAUNCHER_UI_H_
