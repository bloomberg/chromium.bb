// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_PROXY_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_PROXY_HANDLER_H_

#include "chrome/browser/ui/webui/options/chromeos/cros_options_page_ui_handler.h"

namespace chromeos {

// ChromeOS proxy options page UI handler.
class ProxyHandler : public CrosOptionsPageUIHandler {
 public:
  ProxyHandler();
  virtual ~ProxyHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

 private:

  DISALLOW_COPY_AND_ASSIGN(ProxyHandler);
};

} // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_PROXY_HANDLER_H_
