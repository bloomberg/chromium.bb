// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_HANDLER_H_

#include "base/macros.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_message_handler.h"

class ExtensionService;

namespace content {
class WebUIDataSource;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace extensions {

// Extension Settings UI handler.
class ExtensionSettingsHandler : public content::WebUIMessageHandler,
                                 public content::WebContentsObserver {
 public:
  ExtensionSettingsHandler();
  ~ExtensionSettingsHandler() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Note: This uses |web_ui()| from |WebUIMessageHandler|, so it must only be
  // called after |web_ui->AddMessageHandler(this)| has been called.
  void GetLocalizedValues(content::WebUIDataSource* source);

 private:
  // WebContentsObserver implementation.
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Helper method that reloads all unpacked extensions.
  void ReloadUnpackedExtensions();

  // Our model.  Outlives us since it's owned by our containing profile.
  ExtensionService* extension_service_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingsHandler);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_HANDLER_H_
