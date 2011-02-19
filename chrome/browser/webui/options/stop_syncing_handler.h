// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBUI_OPTIONS_STOP_SYNCING_HANDLER_H_
#define CHROME_BROWSER_WEBUI_OPTIONS_STOP_SYNCING_HANDLER_H_

#include "chrome/browser/webui/options/options_ui.h"

// Chrome personal stuff stop syncing overlay UI handler.
class StopSyncingHandler : public OptionsPageUIHandler {
 public:
  StopSyncingHandler();
  virtual ~StopSyncingHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages();

 private:
  void StopSyncing(const ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(StopSyncingHandler);
};

#endif  // CHROME_BROWSER_WEBUI_OPTIONS_STOP_SYNCING_HANDLER_H_
