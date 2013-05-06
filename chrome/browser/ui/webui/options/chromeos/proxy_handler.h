// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_PROXY_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_PROXY_HANDLER_H_

#include "base/compiler_specific.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace chromeos {
namespace options {

// ChromeOS proxy options page UI handler.
class ProxyHandler : public ::options::OptionsPageUIHandler {
 public:
  explicit ProxyHandler();
  virtual ~ProxyHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void InitializePage() OVERRIDE;

 private:

  DISALLOW_COPY_AND_ASSIGN(ProxyHandler);
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_PROXY_HANDLER_H_
