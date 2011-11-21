// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_OPTIONS_SYNC_SETUP_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_OPTIONS_SYNC_SETUP_HANDLER_H_

#include "chrome/browser/ui/webui/sync_setup_handler.h"

// The handler for Javascript messages related to sync setup UI in the options
// page.
class OptionsSyncSetupHandler : public SyncSetupHandler {
 public:
  explicit OptionsSyncSetupHandler(ProfileManager* profile_manager);
  virtual ~OptionsSyncSetupHandler();

 protected:
  virtual void ShowSetupUI() OVERRIDE;
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_OPTIONS_SYNC_SETUP_HANDLER_H_
