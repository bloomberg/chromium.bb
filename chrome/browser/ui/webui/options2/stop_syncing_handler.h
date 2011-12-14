// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_STOP_SYNCING_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_STOP_SYNCING_HANDLER_H_

#include "chrome/browser/ui/webui/options2/options_ui2.h"

// Chrome personal stuff stop syncing overlay UI handler.
class StopSyncingHandler : public OptionsPage2UIHandler {
 public:
  StopSyncingHandler();
  virtual ~StopSyncingHandler();

  // OptionsPage2UIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  void StopSyncing(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(StopSyncingHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_STOP_SYNCING_HANDLER_H_
