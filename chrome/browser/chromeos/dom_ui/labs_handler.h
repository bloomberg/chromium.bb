// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DOM_UI_LABS_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_DOM_UI_LABS_HANDLER_H_

#include <string>

#include "chrome/browser/dom_ui/options_ui.h"

// ChromeOS labs options page UI handler.
class LabsHandler : public OptionsPageUIHandler {
 public:
  LabsHandler();
  virtual ~LabsHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

 private:

  DISALLOW_COPY_AND_ASSIGN(LabsHandler);
};

#endif  // CHROME_BROWSER_CHROMEOS_DOM_UI_LABS_HANDLER_H_
