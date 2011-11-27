// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_INPUT_METHOD_EXTENSION_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_INPUT_METHOD_EXTENSION_API_H_

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_function.h"

// Implements the experimental.inputMethod.get method.
class GetInputMethodFunction : public SyncExtensionFunction {
 public:
  GetInputMethodFunction();

 protected:
  virtual ~GetInputMethodFunction();

  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("inputMethodPrivate.get");
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_INPUT_METHOD_EXTENSION_API_H_
