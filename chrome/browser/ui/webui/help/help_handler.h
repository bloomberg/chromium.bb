// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HELP_HELP_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_HELP_HELP_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/ui/webui/help/version_updater.h"
#include "content/public/browser/web_ui_message_handler.h"

// WebUI message handler for the help page.
class HelpHandler : public content::WebUIMessageHandler {
 public:
  HelpHandler();
  virtual ~HelpHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Fills |localized_strings| with string values for the UI.
  void GetLocalizedValues(base::DictionaryValue* localized_strings);

 private:
  // Initiates the update process.
  void CheckForUpdate(const base::ListValue* args);

  // Relaunches the browser.
  void RelaunchNow(const base::ListValue* args);

  // Callback method which forwards status updates to the page.
  void UpdateStatus(VersionUpdater::Status status, int progress);

  // Specialized instance of the VersionUpdater used to update the browser.
  scoped_ptr<VersionUpdater> version_updater_;

  DISALLOW_COPY_AND_ASSIGN(HelpHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_HELP_HELP_HANDLER_H_
