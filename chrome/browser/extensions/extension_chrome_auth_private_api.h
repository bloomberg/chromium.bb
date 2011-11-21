// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_CHROME_AUTH_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_CHROME_AUTH_PRIVATE_API_H_
#pragma once

#include <string>
#include "chrome/browser/extensions/extension_function.h"

class SetCloudPrintCredentialsFunction : public AsyncExtensionFunction {
 public:
  SetCloudPrintCredentialsFunction();

  // For use only in tests - sets a flag that can cause this function to not
  // actually set the credentials but instead simply reflect the passed in
  // arguments appended together as one string back in results_.
  static void SetTestMode(bool test_mode_enabled);

 protected:
  virtual ~SetCloudPrintCredentialsFunction();

  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("chromeAuthPrivate.setCloudPrintCredentials");
};


#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_CHROME_AUTH_PRIVATE_API_H_
