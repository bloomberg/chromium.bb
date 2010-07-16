// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DOM_UI_CORE_CHROMEOS_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_DOM_UI_CORE_CHROMEOS_OPTIONS_HANDLER_H_

#include "chrome/browser/dom_ui/core_options_handler.h"

namespace chromeos {

// CoreChromeOSOptionsHandler handles ChromeOS settings.
class CoreChromeOSOptionsHandler : public ::CoreOptionsHandler {
 public:

 protected:
  // ::CoreOptionsHandler Implementation
  virtual Value* FetchPref(const std::wstring& pref_name);
  virtual void ObservePref(const std::wstring& pref_name);
  virtual void SetPref(const std::wstring& pref_name,
                       Value::ValueType pref_type,
                       const std::string& value_string);

};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DOM_UI_CORE_CHROMEOS_OPTIONS_HANDLER_H_
