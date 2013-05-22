// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_DESKTOP_USER_ACTION_HANDLER_AURA_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_DESKTOP_USER_ACTION_HANDLER_AURA_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/client/user_action_client.h"

class Browser;

namespace content {
class WebContents;
}

// Handles back/forward commands on a per-browser basis.
class DesktopUserActionHandlerAura : public aura::client::UserActionClient {
 public:
  explicit DesktopUserActionHandlerAura(Browser* browser);
  virtual ~DesktopUserActionHandlerAura();

  // Overridden from aura::client::UserActionClient:
  virtual bool OnUserAction(
      aura::client::UserActionClient::Command command) OVERRIDE;

 private:
  // Returns the active web contents of |browser_|.
  content::WebContents* GetActiveWebContents() const;

  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(DesktopUserActionHandlerAura);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_DESKTOP_USER_ACTION_HANDLER_AURA_H_
