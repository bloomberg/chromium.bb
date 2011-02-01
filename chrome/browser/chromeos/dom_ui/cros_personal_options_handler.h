// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DOM_UI_CROS_PERSONAL_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_DOM_UI_CROS_PERSONAL_OPTIONS_HANDLER_H_

#include "chrome/browser/dom_ui/options/options_ui.h"

namespace chromeos {

class CrosPersonalOptionsHandler : public OptionsPageUIHandler {
 public:
  CrosPersonalOptionsHandler();
  virtual ~CrosPersonalOptionsHandler();

  // Overridden from PersonalOptionsHandler:
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);
  virtual void RegisterMessages();

 private:
  void LoadAccountPicture(const ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(CrosPersonalOptionsHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DOM_UI_CROS_PERSONAL_OPTIONS_HANDLER_H_
