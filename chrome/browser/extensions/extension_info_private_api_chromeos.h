// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INFO_PRIVATE_API_CHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INFO_PRIVATE_API_CHROMEOS_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_function.h"

class GetChromeosInfoFunction : public AsyncExtensionFunction {
 public:
  GetChromeosInfoFunction();

 protected:
  virtual ~GetChromeosInfoFunction();

  virtual bool RunImpl() OVERRIDE;

 private:
  bool GetValue(const std::string& property_name, std::string* value);

  DECLARE_EXTENSION_FUNCTION_NAME("chromeosInfoPrivate.get");
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INFO_PRIVATE_API_CHROMEOS_H_
