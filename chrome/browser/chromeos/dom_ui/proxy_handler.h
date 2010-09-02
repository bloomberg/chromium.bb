// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DOM_UI_PROXY_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_DOM_UI_PROXY_HANDLER_H_

#include "chrome/browser/chromeos/dom_ui/cros_options_page_ui_handler.h"
namespace chromeos {

// ChromeOS proxy options page UI handler.
class ProxyHandler : public CrosOptionsPageUIHandler {
 public:
  ProxyHandler();
  virtual ~ProxyHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

 private:

  DISALLOW_COPY_AND_ASSIGN(ProxyHandler);
};
} // namespace chromeos
#endif  // CHROME_BROWSER_CHROMEOS_DOM_UI_PROXY_HANDLER_H_
