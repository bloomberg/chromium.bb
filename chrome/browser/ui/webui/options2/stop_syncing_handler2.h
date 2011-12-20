// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_STOP_SYNCING_HANDLER2_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_STOP_SYNCING_HANDLER2_H_

#include "chrome/browser/ui/webui/options2/options_ui2.h"

namespace options2 {

// Chrome personal stuff stop syncing overlay UI handler.
class StopSyncingHandler : public OptionsPageUIHandler {
 public:
  StopSyncingHandler();
  virtual ~StopSyncingHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  void StopSyncing(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(StopSyncingHandler);
};

}  // namespace options2

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_STOP_SYNCING_HANDLER2_H_
