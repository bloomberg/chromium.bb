// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_AUTOFILL_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_AUTOFILL_OPTIONS_HANDLER_H_

#include "chrome/browser/dom_ui/options_ui.h"

class AutoFillOptionsHandler : public OptionsPageUIHandler {
 public:
  AutoFillOptionsHandler();
  virtual ~AutoFillOptionsHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

  virtual void RegisterMessages();

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoFillOptionsHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_AUTOFILL_OPTIONS_HANDLER_H_
