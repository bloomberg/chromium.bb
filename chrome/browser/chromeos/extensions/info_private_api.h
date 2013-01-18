// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_INFO_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_INFO_PRIVATE_API_H_

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_function.h"

namespace base {
class Value;
}

namespace extensions {

class GetChromeosInfoFunction : public AsyncExtensionFunction {
 public:
  GetChromeosInfoFunction();

 protected:
  virtual ~GetChromeosInfoFunction();

  virtual bool RunImpl() OVERRIDE;

 private:
  // Returns a newly allocate value, or null.
  base::Value* GetValue(const std::string& property_name);

  DECLARE_EXTENSION_FUNCTION("chromeosInfoPrivate.get", CHROMEOSINFOPRIVATE_GET)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_INFO_PRIVATE_API_H_
