// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SYNC_PROMO_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SYNC_PROMO_HANDLER_H_
#pragma once

#include "chrome/browser/ui/webui/sync_setup_handler.h"

// The handler for Javascript messages related to the "sync promo" page.
class SyncPromoHandler : public SyncSetupHandler {
 public:
  SyncPromoHandler();
  virtual ~SyncPromoHandler();

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui) OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

 protected:
  virtual void ShowSetupUI() OVERRIDE;

 private:
  // Javascript callback handler to initialize the sync promo.
  void HandleInitializeSyncPromo(const base::ListValue* args);

  // Javascript callback handler to close the sync promo.
  void HandleCloseSyncPromo(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(SyncPromoHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SYNC_PROMO_HANDLER_H_
