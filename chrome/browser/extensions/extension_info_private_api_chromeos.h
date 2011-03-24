// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INFO_PRIVATE_API_CHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INFO_PRIVATE_API_CHROMEOS_H_
#pragma once

#include <string>
#include <vector>
#include "base/scoped_ptr.h"
#include "chrome/browser/extensions/extension_function.h"

class DictionaryValue;

namespace chromeos {
class StartupCustomizationDocument;
}  // namespace chromeos

class GetChromeosInfoFunction : public AsyncExtensionFunction {
 public:
  GetChromeosInfoFunction();

 protected:
  virtual ~GetChromeosInfoFunction();

  virtual bool RunImpl();

 private:
  // This method is called on FILE thread.
  void LoadValues();

  // This method is called on UI thread.
  void RespondOnUIThread();

  scoped_ptr<DictionaryValue> result_dictionary_;
  std::vector<std::string> properties_;
  std::vector<std::pair<std::string, std::string> > new_results_;

  DECLARE_EXTENSION_FUNCTION_NAME("chromeosInfoPrivate.get");
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INFO_PRIVATE_API_CHROMEOS_H_
