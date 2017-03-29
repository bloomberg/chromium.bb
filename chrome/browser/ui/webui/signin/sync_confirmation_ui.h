// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SYNC_CONFIRMATION_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SYNC_CONFIRMATION_UI_H_

#include <memory>

#include "base/macros.h"
#include "ui/web_dialogs/web_dialog_ui.h"

class SyncConfirmationHandler;

namespace ui {
class WebUI;
}

class SyncConfirmationUI : public ui::WebDialogUI {
 public:
  explicit SyncConfirmationUI(content::WebUI* web_ui);
   // Used to inject a SyncConfirmationHandler in tests.
  SyncConfirmationUI(content::WebUI* web_ui,
                     std::unique_ptr<SyncConfirmationHandler> handler);
  ~SyncConfirmationUI() override {}

  DISALLOW_COPY_AND_ASSIGN(SyncConfirmationUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SYNC_CONFIRMATION_UI_H_
