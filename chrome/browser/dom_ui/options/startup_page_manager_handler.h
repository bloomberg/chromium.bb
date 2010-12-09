// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_OPTIONS_STARTUP_PAGE_MANAGER_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_OPTIONS_STARTUP_PAGE_MANAGER_HANDLER_H_

#include "chrome/browser/dom_ui/options/options_ui.h"

class StartupPageManagerHandler : public OptionsPageUIHandler {
 public:
  StartupPageManagerHandler();
  virtual ~StartupPageManagerHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

 private:

  DISALLOW_COPY_AND_ASSIGN(StartupPageManagerHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_OPTIONS_STARTUP_PAGE_MANAGER_HANDLER_H_
