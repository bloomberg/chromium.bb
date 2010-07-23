// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_ADD_STARTUP_PAGE_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_ADD_STARTUP_PAGE_HANDLER_H_

#include "chrome/browser/dom_ui/options_ui.h"

// Chrome personal options page UI handler.
class AddStartupPageHandler : public OptionsPageUIHandler {
 public:
  AddStartupPageHandler();
  virtual ~AddStartupPageHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

 private:
  DISALLOW_COPY_AND_ASSIGN(AddStartupPageHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_ADD_STARTUP_PAGE_HANDLER_H_

