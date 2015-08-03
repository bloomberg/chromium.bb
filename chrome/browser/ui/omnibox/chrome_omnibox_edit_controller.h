// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_EDIT_CONTROLLER_H_
#define CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_EDIT_CONTROLLER_H_

#include "components/omnibox/browser/omnibox_edit_controller.h"

class CommandUpdater;

namespace content {
class WebContents;
}

// Chrome-specific extension of the OmniboxEditController base class.
class ChromeOmniboxEditController : public OmniboxEditController {
 public:
  // Returns the WebContents of the currently active tab.
  virtual content::WebContents* GetWebContents() = 0;

  CommandUpdater* command_updater() { return command_updater_; }
  const CommandUpdater* command_updater() const { return command_updater_; }

 protected:
  explicit ChromeOmniboxEditController(CommandUpdater* command_updater);
  ~ChromeOmniboxEditController() override;

 private:
  // OmniboxEditController:
  void OnAutocompleteAccept(const GURL& destination_url,
                            WindowOpenDisposition disposition,
                            ui::PageTransition transition) override;

  CommandUpdater* command_updater_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOmniboxEditController);
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_EDIT_CONTROLLER_H_
