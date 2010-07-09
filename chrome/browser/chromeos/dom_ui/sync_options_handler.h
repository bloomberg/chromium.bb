// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DOM_UI_SYNC_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_DOM_UI_SYNC_OPTIONS_HANDLER_H_

#include "chrome/browser/dom_ui/options_ui.h"

// ChromeOS system options page UI handler.
class SyncOptionsHandler : public OptionsPageUIHandler {
 public:
  SyncOptionsHandler();
  virtual ~SyncOptionsHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncOptionsHandler);
};

#endif  // CHROME_BROWSER_CHROMEOS_DOM_UI_SYNC_OPTIONS_HANDLER_H_
